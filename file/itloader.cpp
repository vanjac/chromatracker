#include "itloader.h"
#include "itdecompress.hpp"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace chromatracker::file {

// https://github.com/schismtracker/schismtracker/wiki/ITTECH.TXT
// https://github.com/OpenMPT/openmpt/blob/master/soundlib/Load_it.cpp

const ticks IT_TICK_TIME = 8; // chroma ticks per IT tick
const int MAX_CHANNELS = 64;

ITLoader::ITLoader(SDL_RWops *stream)
    : stream(stream)
{}

void ITLoader::loadSong(Song *song)
{
    this->song = song;

    char signature[5]{0};
    SDL_RWread(stream, signature, sizeof(char), 4);
    if (memcmp(signature, "IMPM", 4)) {
        throw std::runtime_error("Unrecognized format");
    }

    auto firstSection = song->sections.emplace_back(new Section);

    char songName[27]{0};
    SDL_RWread(stream, songName, sizeof(char), 26);
    firstSection->title = string(songName);

    // highlight information (not used for playback)
    uint8_t rowsPerBeat = SDL_ReadU8(stream);
    uint8_t rowsPerMeasure = SDL_ReadU8(stream);
    if (rowsPerMeasure % rowsPerBeat == 0) {
        firstSection->meter = rowsPerMeasure / rowsPerBeat;
    }

    uint16_t numOrders = SDL_ReadLE16(stream);
    uint16_t numInstruments = SDL_ReadLE16(stream);
    uint16_t numSamples = SDL_ReadLE16(stream);
    uint16_t numPatterns = SDL_ReadLE16(stream);

    cout <<numOrders<< " orders, " <<numInstruments<< " instruments, "
        <<numSamples<< " samples, " <<numPatterns<< " patterns\n";

    SDL_RWseek(stream, 2, RW_SEEK_CUR); // skip created version
    compatibleVersion = SDL_ReadLE16(stream);
    if (compatibleVersion < 0x200) {
        throw std::runtime_error("Old IT files (pre 2.00) not supported");
    }

    uint16_t songFlags = SDL_ReadLE16(stream);
    bool instrumentMode = songFlags & (1<<2);
    SDL_RWseek(stream, 2, RW_SEEK_CUR); // skip special flags

    uint8_t globalVolume = SDL_ReadU8(stream);
    uint8_t mixVolume = SDL_ReadU8(stream);
    song->volume = (globalVolume / 128.0f) * (mixVolume / 128.0f);

    ticksPerRow = SDL_ReadU8(stream);
    uint8_t initialTempo = SDL_ReadU8(stream);
    firstSection->tempo = initialTempo;

    // for now add all 64 tracks (will be reduced later)
    song->tracks.reserve(MAX_CHANNELS);
    SDL_RWseek(stream, 0x40, RW_SEEK_SET);
    for (int i = 0; i < MAX_CHANNELS; i++) {
        auto track = song->tracks.emplace_back(new Track);
        uint8_t pan = SDL_ReadU8(stream);
        track->mute = pan & 0x80;
        pan &= 0x7f;
        if (pan <= 64)
            track->pan = (pan - 32) / 32.0f; // TODO fix units and amplitude
    }
    for (auto &track : song->tracks) {
        uint8_t vol = SDL_ReadU8(stream);
        track->volume = vol / 64.0f;
    }

    unique_ptr<uint8_t[]> orders(new uint8_t[numOrders]);
    SDL_RWread(stream, orders.get(), 1, numOrders);
    unique_ptr<uint32_t[]> instOffsets(new uint32_t[numInstruments]);
    SDL_RWread(stream, instOffsets.get(), 4, numInstruments);
    unique_ptr<uint32_t[]> sampleOffsets(new uint32_t[numSamples]);
    SDL_RWread(stream, sampleOffsets.get(), 4, numSamples);
    unique_ptr<uint32_t[]> patternOffsets(new uint32_t[numPatterns]);
    SDL_RWread(stream, patternOffsets.get(), 4, numPatterns);

    for (int i = 0; i < numSamples; i++) {
        shared_ptr<Sample> sample;
        InstrumentExtra *extra;
        if (!instrumentMode) {
            sample = song->samples.emplace_back(new Sample);
            extra = &instrumentExtras.emplace_back();
        } else {
            sample = itSamples.emplace_back(new Sample);
            extra = &itSampleExtras.emplace_back();
        }
        SDL_RWseek(stream, sampleOffsets[i], RW_SEEK_SET);
        loadSample(sample, extra);
    }

    if (instrumentMode) {
        song->samples.reserve(numInstruments);
        instrumentExtras.reserve(numInstruments);
        for (int i = 0; i < numInstruments; i++) {
            auto sample = song->samples.emplace_back(new Sample);
            InstrumentExtra *extra = &instrumentExtras.emplace_back();
            SDL_RWseek(stream, instOffsets[i], RW_SEEK_SET);
            loadInstrument(sample, extra);
        }
    }

    // add number prefixes to samples
    for (int i = 0; i < song->samples.size(); i++) {
        auto sample = song->samples[i];
        std::ostringstream nameStream;
        nameStream << std::setw(2) << std::setfill('0') << (i + 1) << " "
            << sample->name;
        sample->name = nameStream.str();
    }

    song->sections.reserve(numOrders);
    for (int i = 0; i < numOrders; i++) {
        int order = orders[i];
        if (order == 255) {
            break;
        } else if (order == 254) {
            continue;
        } else if (order >= numPatterns) {
            throw std::runtime_error("Order out of range");
        }

        shared_ptr<Section> section;
        if (i == 0) {
            section = firstSection;
        } else {
            auto prevSection = song->sections.back();
            section = song->sections.emplace_back(new Section);
            prevSection->next = section;
        }
        section->trackEvents.insert(section->trackEvents.end(), MAX_CHANNELS,
                                    vector<Event>());

        uint32_t patternOffset = patternOffsets[order];
        if (patternOffset == 0) { // empty
            section->length = 64 * (int)ticksPerRow * IT_TICK_TIME;
        } else {
            // TODO: inefficient, can load the same pattern many times
            SDL_RWseek(stream, patternOffset, RW_SEEK_SET);
            loadSection(section);
        }
    }

    song->tracks.erase(song->tracks.begin() + (maxUsedChannel + 1),
                       song->tracks.end());
    for (auto &section : song->sections) {
        section->trackEvents.erase(
            section->trackEvents.begin() + (maxUsedChannel + 1),
            section->trackEvents.end());
    }
}

template<typename T>
void loadWave(SDL_RWops *stream, shared_ptr<Sample> sample,
              frames numFrames, int numChannels)
{
    int numSamples = numFrames * numChannels;
    unique_ptr<T[]> data(new T[numSamples]);
    SDL_RWread(stream, data.get(), sizeof(T), numSamples);

    std::numeric_limits<T> limits;
    T *read = &data[0];
    for (int c = 0; c < numChannels; c++) {
        for (frames f = 0; f < numFrames; f++, read++) {
            float value = (float)(*read) / limits.max();
            if (!limits.is_signed)
                value = value * 2 - 1;
            sample->channels[c].push_back(value);
        }
    }
}

void ITLoader::loadSample(shared_ptr<Sample> sample, InstrumentExtra *extra)
{
    char signature[5]{0};
    SDL_RWread(stream, signature, sizeof(char), 4);
    if (memcmp(signature, "IMPS", 4)) {
        throw std::runtime_error("Invalid sample header");
    }

    SDL_RWseek(stream, 13, RW_SEEK_CUR); // skip filename, 0 byte
    uint8_t globalVolume = SDL_ReadU8(stream);
    sample->volume = globalVolume / 64.0f;

    uint8_t flags = SDL_ReadU8(stream);
    bool bit16 = flags & (1<<1);
    bool stereo = flags & (1<<2);
    bool compressed = flags & (1<<3);
    bool hasLoop = flags & (1<<4);
    bool hasSusLoop = flags & (1<<5);
    bool pingPongLoop = flags & (1<<6);
    bool pingPongSus = flags & (1<<7);
    if (hasSusLoop) { // overrides regular loop
        sample->loopMode = pingPongSus ? Sample::LoopMode::PingPong
                                       : Sample::LoopMode::Forward;
    } else if (hasLoop) {
        sample->loopMode = pingPongLoop ? Sample::LoopMode::PingPong
                                        : Sample::LoopMode::Forward;
    } else {
        sample->loopMode = Sample::LoopMode::Once;
    }

    extra->defaultVolume = SDL_ReadU8(stream);

    char nameBuf[27]{0};
    SDL_RWread(stream, nameBuf, sizeof(char), 26);
    sample->name = nameBuf;

    uint8_t convertFlags = SDL_ReadU8(stream);
    bool signedSamples = convertFlags & (1<<0);
    SDL_RWseek(stream, 1, RW_SEEK_CUR); // skip default pan

    uint32_t numFrames = SDL_ReadLE32(stream);
    uint32_t loopStart = SDL_ReadLE32(stream);
    uint32_t loopEnd = SDL_ReadLE32(stream);
    
    // ITTECH seems to be wrong, this is frames per second, not bytes
    uint32_t c5speed = SDL_ReadLE32(stream);
    sample->frameRate = c5speed;

    uint32_t susLoopStart = SDL_ReadLE32(stream);
    uint32_t susLoopEnd = SDL_ReadLE32(stream);
    if (hasSusLoop) {
        sample->loopStart = susLoopStart;
        sample->loopEnd = susLoopEnd;
    } else if (hasLoop) {
        sample->loopStart = loopStart;
        sample->loopEnd = loopEnd;
    } else {
        sample->loopStart = 0;
        sample->loopEnd = numFrames;
    }

    uint32_t samplePointer = SDL_ReadLE32(stream);
    SDL_RWseek(stream, samplePointer, RW_SEEK_SET);

    int numChannels = stereo ? 2 : 1;
    sample->channels.reserve(numChannels);
    for (int i = 0; i < numChannels; i++) {
        sample->channels.emplace_back().reserve(numFrames);
    }

    bool it215 = compatibleVersion >= 0x215;
    if (bit16) {
        if (compressed)
            ITDecompress<IT16BitParams>(stream, sample, numFrames, numChannels,
                                        it215).decompress();
        else if (signedSamples)
            loadWave<int16_t>(stream, sample, numFrames, numChannels);
        else
            loadWave<uint16_t>(stream, sample, numFrames, numChannels);
    } else {
        if (compressed)
            ITDecompress<IT8BitParams>(stream, sample, numFrames, numChannels,
                                       it215).decompress();
        else if (signedSamples)
            loadWave<int8_t>(stream, sample, numFrames, numChannels);
        else
            loadWave<uint8_t>(stream, sample, numFrames, numChannels);
    }

    sample->fadeOut = 1.0f; // will be overridden by instruments
}

void ITLoader::loadInstrument(shared_ptr<Sample> sample, InstrumentExtra *extra)
{
    char signature[5]{0};
    SDL_RWread(stream, signature, sizeof(char), 4);
    if (memcmp(signature, "IMPI", 4)) {
        throw std::runtime_error("Invalid sample header");
    }

    // get the sample associated with middle c
    // TODO also get note and transpose
    int sampleNumOffset = 0x40 + 2*MIDDLE_C + 1;
    SDL_RWseek(stream, sampleNumOffset - 0x04, RW_SEEK_CUR);
    uint8_t sampleNum = SDL_ReadU8(stream);
    if (sampleNum == 0 || sampleNum > itSamples.size()) {
        return;
    }

    // copies wave
    *sample = *(itSamples[sampleNum - 1]);
    *extra = itSampleExtras[sampleNum - 1];

    SDL_RWseek(stream, 0x14 - sampleNumOffset - 1, RW_SEEK_CUR);
    uint16_t fadeOut = SDL_ReadLE16(stream);
    float fadeTime = 1024.0 / fadeOut; // time to fade to zero in IT ticks

    SDL_RWseek(stream, 2, RW_SEEK_CUR); // skip pitch-pan sep
    uint8_t globalVolume = SDL_ReadU8(stream);
    sample->volume *= globalVolume / 128.0f;

    SDL_RWseek(stream, 7, RW_SEEK_CUR);
    char nameBuf[27]{0};
    SDL_RWread(stream, nameBuf, sizeof(char), 26);
    string name = nameBuf;
    if (!name.empty()) // otherwise keep sample name
        sample->name = nameBuf;
    
    // volume envelope
    SDL_RWseek(stream, 0x130 - 0x3A, RW_SEEK_CUR);
    uint8_t volEnvFlags = SDL_ReadU8(stream);
    bool volEnvEnable = volEnvFlags & (1<<0);
    if (volEnvEnable) {
        bool envSustain = volEnvFlags & (1<<2);
        if (!envSustain)
            extra->autoFade = true;

        uint8_t numNodes = SDL_ReadU8(stream);
        SDL_RWseek(stream, 3, RW_SEEK_CUR);
        uint8_t susLoopEnd = SDL_ReadU8(stream);

        // skip to last node
        SDL_RWseek(stream, 3 * (numNodes - 1), RW_SEEK_CUR);
        uint8_t lastNodeY = SDL_ReadU8(stream);
        uint16_t lastNodeTime = SDL_ReadLE16(stream);

        uint16_t loopEndTime = 0;
        if (envSustain) {
            SDL_RWseek(stream, 3 * (susLoopEnd - numNodes) + 1, RW_SEEK_CUR);
            loopEndTime = SDL_ReadLE16(stream);
        }

        if (lastNodeY == 0) {
            fadeTime = lastNodeTime - loopEndTime;
        } else {
            fadeTime += lastNodeTime - loopEndTime;
        }
    }

    // TODO scale by default volume
    sample->fadeOut = 1 / fadeTime / IT_TICK_TIME;
}

void ITLoader::loadSection(shared_ptr<Section> section)
{
    uint16_t packedLength = SDL_ReadLE16(stream);
    uint16_t numRows = SDL_ReadLE16(stream);
    section->length = numRows * (int)ticksPerRow * IT_TICK_TIME;
    SDL_RWseek(stream, 4, RW_SEEK_CUR);

    struct PatternCell {
        int note{-1}; // -1 = no note!
        int instrument{-1};
        int volume{-1};
        int command{-1};
        int commandValue{-1};
    };

    // https://github.com/schismtracker/schismtracker/wiki/ITTECH.TXT#impulse-pattern-format
    int pos = 0;
    int row = 0;
    vector<uint8_t> channelMasks(MAX_CHANNELS, 0);
    vector<PatternCell> channelCells(MAX_CHANNELS);
    while (pos < packedLength) {
        uint8_t channelVar = SDL_ReadU8(stream); pos++;
        if (channelVar == 0) {
            row++;
            continue;
        }

        int channelNum = channelVar & 0x7f;
        if (channelNum > 0) {
            channelNum--;
        }
        if (channelNum >= MAX_CHANNELS) {
            throw std::runtime_error("Exceeded maximum channels");
        } else if (channelNum > maxUsedChannel) {
            maxUsedChannel = channelNum;
        }

        if (channelVar & 0x80) {
            channelMasks[channelNum] = SDL_ReadU8(stream); pos++;
        }
        uint8_t mask = channelMasks[channelNum];

        // TODO allow writing to same cell twice (store cells per row)
        PatternCell cell;
        PatternCell *cellMemory = &channelCells[channelNum];
        if (mask & 1) {
            cell.note = SDL_ReadU8(stream); pos++;
            cellMemory->note = cell.note;
        } else if (mask & 0x10) {
            cell.note = cellMemory->note;
        }
        if (mask & 2) {
            cell.instrument = SDL_ReadU8(stream); pos++;
            cellMemory->instrument = cell.instrument;
        } else if (mask & 0x20) {
            cell.instrument = cellMemory->instrument;
        }
        if (mask & 4) {
            cell.volume = SDL_ReadU8(stream); pos++;
            cellMemory->volume = cell.volume;
        } else if (mask & 0x40) {
            cell.volume = cellMemory->volume;
        }
        if (mask & 8) {
            cell.command = SDL_ReadU8(stream); pos++;
            cellMemory->command = cell.command;
            cell.commandValue = SDL_ReadU8(stream); pos++;
            cellMemory->commandValue = cell.commandValue;
        } else if (mask & 0x80) {
            cell.command = cellMemory->command;
            cell.commandValue = cellMemory->commandValue;
        }

        uint8_t cmdNibble1 = cell.commandValue >> 4;
        uint8_t cmdNibble2 = cell.commandValue & 0xf;
        Event event;
        event.time = row * (int)ticksPerRow * IT_TICK_TIME;
        InstrumentExtra *instExtra = nullptr;
        // order is important, velocity gets overridden multiple times
        if (cell.instrument > 0 && cell.instrument <= song->samples.size()) {
            event.sample = song->samples[cell.instrument - 1];
            instExtra = &instrumentExtras[cell.instrument - 1];
            event.velocity = amplitudeToVelocity(
                instExtra->defaultVolume / 64.0f);
        }
        if (cell.volume >= 0 && cell.volume <= 64) {
            event.velocity = amplitudeToVelocity(cell.volume / 64.0f);
        }
        if (cell.note == 254) {
            event.velocity = 0;
        } else if (cell.note > 119) {
            event.special = Event::Special::FadeOut;
            event.sample.reset();
        } else if (cell.note != -1) {
            event.pitch = cell.note;
        }
        if ((cell.volume >= 193 && cell.volume <= 202) || cell.command == 7) {
            event.sample.reset(); // portamento
        }
        if (cell.command == 19 && cmdNibble1 == 0xD) { // SDx
            if (cmdNibble1 == 0xD) {
                event.time += (frames)cmdNibble2 * IT_TICK_TIME;
            }
        }
        if (instExtra && instExtra->autoFade) {
            event.special = Event::Special::FadeOut;
        }

        if (!event.empty()) {
            section->trackEvents[channelNum].push_back(event);
        }

        if (cell.command == 19 && cmdNibble1 == 0xC) { // SCx
            Event cutEvent;
            cutEvent.time = event.time + (frames)cmdNibble2 * IT_TICK_TIME;
            cutEvent.velocity = 0;
            section->trackEvents[channelNum].push_back(cutEvent);
        } else if (cell.command == 17) { // Qxy
            if (cmdNibble2 != 0) {
                Event retriggerEvent = event;
                for (int i = cmdNibble2; i < ticksPerRow; i += cmdNibble2) {
                    retriggerEvent.time = event.time + i * IT_TICK_TIME;
                    // TODO volume change
                    section->trackEvents[channelNum].push_back(retriggerEvent);
                }
            }
        }
        // TODO arpeggio
    }
}

} // namespace
