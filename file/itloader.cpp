#include "itloader.h"
#include "itdecompress.hpp"
#include <stringutil.h>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <glm/gtx/color_space.hpp>

namespace chromatracker::file {

// https://github.com/schismtracker/schismtracker/wiki/ITTECH.TXT
// https://github.com/OpenMPT/openmpt/blob/master/soundlib/Load_it.cpp

const ticks IT_TICK_TIME = 8; // chroma ticks per IT tick
const int MAX_CHANNELS = 64;

const uint32_t INST_SAMPLE_NUM_OFFSET = 0x40 + 2*MIDDLE_C + 1;

ITLoader::ITLoader(SDL_RWops *stream)
    : stream(stream)
{
    checkHeader();
    loadObjects();
}

ITLoader::~ITLoader()
{
    SDL_RWclose(stream);
}

void ITLoader::checkHeader()
{
    SDL_RWseek(stream, 0, RW_SEEK_SET);
    char signature[5]{0};
    SDL_RWread(stream, signature, 1, 4);
    if (std::memcmp(signature, "IMPM", 4)) {
        throw std::runtime_error("Unrecognized format");
    }

    SDL_RWseek(stream, 0x2A, RW_SEEK_SET);
    compatibleVersion = SDL_ReadLE16(stream);
    if (compatibleVersion < 0x200) {
        throw std::runtime_error("Old IT files (pre 2.00) not supported");
    }

    uint16_t songFlags = SDL_ReadLE16(stream);
    instrumentMode = songFlags & (1<<2);
}

void ITLoader::loadObjects()
{
    SDL_RWseek(stream, 0x20, RW_SEEK_SET);
    numOrders = SDL_ReadLE16(stream);
    numInstruments = SDL_ReadLE16(stream);
    numSamples = SDL_ReadLE16(stream);
    numPatterns = SDL_ReadLE16(stream);

    SDL_RWseek(stream, 0xC0, RW_SEEK_SET);
    orders.reset(new uint8_t[numOrders]);
    SDL_RWread(stream, orders.get(), 1, numOrders);
    instOffsets.reset(new uint32_t[numInstruments]);
    SDL_RWread(stream, instOffsets.get(), 4, numInstruments);
    sampleOffsets.reset(new uint32_t[numSamples]);
    SDL_RWread(stream, sampleOffsets.get(), 4, numSamples);
    patternOffsets.reset(new uint32_t[numPatterns]);
    SDL_RWread(stream, patternOffsets.get(), 4, numPatterns);
}

void ITLoader::loadSong(Song *song)
{
    this->song = song;

    auto firstSection = song->sections.emplace_back(new Section);

    char songName[27]{0};
    SDL_RWseek(stream, 0x04, RW_SEEK_SET);
    SDL_RWread(stream, songName, 1, 26);
    firstSection->title = string(songName);

    // highlight information (not used for playback)
    uint8_t rowsPerBeat = SDL_ReadU8(stream);
    uint8_t rowsPerMeasure = SDL_ReadU8(stream);
    if (rowsPerMeasure % rowsPerBeat == 0) {
        firstSection->meter = rowsPerMeasure / rowsPerBeat;
    }

    SDL_RWseek(stream, 0x30, RW_SEEK_SET);
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
        loadITSample(sampleOffsets[i], sample, extra);
    }

    if (instrumentMode) {
        song->samples.reserve(numInstruments);
        instrumentExtras.reserve(numInstruments);
        for (int i = 0; i < numInstruments; i++) {
            auto sample = song->samples.emplace_back(new Sample);
            InstrumentExtra *extra = &instrumentExtras.emplace_back();
            loadInstrument(instOffsets[i], sample, extra);
        }
    }

    // add names and colors to samples
    for (int i = 0; i < song->samples.size(); i++) {
        auto sample = song->samples[i];
        if (sample->name.empty())
            sample->name = leftPad(std::to_string(i + 1), 2);
        sample->color = glm::rgbColor(
            glm::vec3((float)i / song->samples.size() * 360.0f, 1, 1));
    }
    offsetSamples.resize(song->samples.size());

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
        section->trackEvents.resize(MAX_CHANNELS);

        uint32_t patternOffset = patternOffsets[order];
        if (patternOffset == 0) { // empty
            section->length = 64 * (int)ticksPerRow * IT_TICK_TIME;
        } else {
            // TODO: inefficient, can load the same pattern many times
            loadPattern(patternOffset, section);
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

vector<string> ITLoader::listSamples()
{
    vector<string> sampleNames;
    sampleNames.reserve(numSamples);
    for (int i = 0; i < numSamples; i++) {
        checkSampleHeader(sampleOffsets[i]);
        SDL_RWseek(stream, sampleOffsets[i] + 0x14, RW_SEEK_SET);
        char nameBuf[27]{0};
        SDL_RWread(stream, nameBuf, 1, 26);
        sampleNames.push_back(nameBuf);
    }
    if (!instrumentMode) {
        return sampleNames;
    }

    vector<string> instrumentNames;
    instrumentNames.reserve(numInstruments);
    for (int i = 0; i < numInstruments; i++) {
        checkInstrumentHeader(instOffsets[i]);
        SDL_RWseek(stream, instOffsets[i] + INST_SAMPLE_NUM_OFFSET,
                   RW_SEEK_SET);
        uint8_t sampleNum = SDL_ReadU8(stream);
        SDL_RWseek(stream, instOffsets[i] + 0x20, RW_SEEK_SET);
        char nameBuf[27]{0};
        SDL_RWread(stream, nameBuf, 1, 26);
        string name = nameBuf;
        if (!name.empty()) {
            instrumentNames.push_back(name);
        } else if (sampleNum != 0 && sampleNum <= sampleNames.size()) {
            instrumentNames.push_back(sampleNames[sampleNum - 1]);
        } else {
            instrumentNames.push_back("");
        }
    }
    return instrumentNames;
}

void ITLoader::loadSample(int index, shared_ptr<Sample> sample)
{
    if (!instrumentMode) {
        if (index < 0 || index >= numSamples)
            return;
        InstrumentExtra extra;
        loadITSample(sampleOffsets[index], sample, &extra);
    } else {
        if (index < 0 || index >= numInstruments)
            return;
        InstrumentExtra extra;
        loadInstrument(instOffsets[index], sample, &extra);
    }
}

template<typename T>
void loadWave(SDL_RWops *stream, shared_ptr<Sample> sample,
              frames numFrames, int numChannels)
{
    int numSamples = numFrames * numChannels;
    unique_ptr<T[]> data(new T[numSamples]);
    // TODO endianess
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

void ITLoader::checkSampleHeader(uint32_t offset)
{
    SDL_RWseek(stream, offset, RW_SEEK_SET);
    char signature[5]{0};
    SDL_RWread(stream, signature, 1, 4);
    if (std::memcmp(signature, "IMPS", 4)) {
        throw std::runtime_error("Invalid sample header");
    }
}

void ITLoader::loadITSample(uint32_t offset, shared_ptr<Sample> sample,
                            InstrumentExtra *extra)
{
    checkSampleHeader(offset);

    SDL_RWseek(stream, offset + 0x11, RW_SEEK_SET);
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
    SDL_RWread(stream, nameBuf, 1, 26);
    sample->name = nameBuf;

    uint8_t convertFlags = SDL_ReadU8(stream);
    bool signedSamples = convertFlags & (1<<0);

    SDL_RWseek(stream, offset + 0x30, RW_SEEK_SET);
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

void ITLoader::checkInstrumentHeader(uint32_t offset)
{
    SDL_RWseek(stream, offset, RW_SEEK_SET);
    char signature[5]{0};
    SDL_RWread(stream, signature, 1, 4);
    if (std::memcmp(signature, "IMPI", 4)) {
        throw std::runtime_error("Invalid instrument header");
    }
}

void ITLoader::loadInstrument(uint32_t offset, shared_ptr<Sample> sample,
                              InstrumentExtra *extra)
{
    SDL_RWseek(stream, offset, RW_SEEK_SET);
    checkInstrumentHeader(offset);

    // get the sample associated with middle c
    // TODO also get note and transpose
    SDL_RWseek(stream, offset + INST_SAMPLE_NUM_OFFSET, RW_SEEK_SET);
    uint8_t sampleNum = SDL_ReadU8(stream);
    if (sampleNum == 0 || sampleNum > numSamples) {
        return;
    }

    if (itSamples.empty()) {
        // probably loading single instrument
        loadITSample(sampleOffsets[sampleNum - 1], sample, extra);
    } else {
        // copies wave
        *sample = *(itSamples[sampleNum - 1]);
        *extra = itSampleExtras[sampleNum - 1];
    }

    SDL_RWseek(stream, offset + 0x14, RW_SEEK_SET);
    uint16_t fadeOut = SDL_ReadLE16(stream);
    float fadeTime = 1024.0 / fadeOut; // time to fade to zero in IT ticks

    SDL_RWseek(stream, offset + 0x18, RW_SEEK_SET);
    uint8_t globalVolume = SDL_ReadU8(stream);
    sample->volume *= globalVolume / 128.0f;

    SDL_RWseek(stream, offset + 0x20, RW_SEEK_SET);
    char nameBuf[27]{0};
    SDL_RWread(stream, nameBuf, 1, 26);
    string name = nameBuf;
    if (!name.empty()) // otherwise keep sample name
        sample->name = nameBuf;
    
    // volume envelope
    SDL_RWseek(stream, offset + 0x130, RW_SEEK_SET);
    uint8_t volEnvFlags = SDL_ReadU8(stream);
    bool volEnvEnable = volEnvFlags & (1<<0);
    if (volEnvEnable) {
        bool envSustain = volEnvFlags & (1<<2);
        if (!envSustain)
            extra->autoFade = true;

        uint8_t numNodes = SDL_ReadU8(stream);
        SDL_RWseek(stream, 3, RW_SEEK_CUR); // TODO clean up SEEK_CUR
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

void ITLoader::loadPattern(uint32_t offset, shared_ptr<Section> section)
{
    SDL_RWseek(stream, offset, RW_SEEK_SET);
    uint16_t packedLength = SDL_ReadLE16(stream);
    uint16_t numRows = SDL_ReadLE16(stream);
    section->length = numRows * (int)ticksPerRow * IT_TICK_TIME;
    SDL_RWseek(stream, offset + 0x08, RW_SEEK_SET);

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
        // set sample/special
        if (cell.note >= 120 && cell.note != 254) {
            event.special = Event::Special::FadeOut;
        } else if (cell.instrument > 0
                && cell.instrument <= song->samples.size()) {
            if (!((cell.volume >= 193 && cell.volume <= 202)
                   || cell.command == 7)) // no portamento
                event.sample = song->samples[cell.instrument - 1];
            InstrumentExtra *instExtra = &instrumentExtras[cell.instrument - 1];
            event.velocity = amplitudeToVelocity(
                instExtra->defaultVolume / 64.0f);
            if (instExtra->autoFade)
                event.special = Event::Special::FadeOut;
        }
        // set pitch
        if (cell.note >= 0 && cell.note < 120) {
            event.pitch = cell.note;
        }
        // set velocity, overriding instrument default
        if (cell.note == 254) {
            event.velocity = 0;
        } else if (cell.volume >= 0 && cell.volume <= 64) {
            event.velocity = amplitudeToVelocity(cell.volume / 64.0f);
        }
        // handle other commands
        if (cell.command == 19 && cmdNibble1 == 0xD) { // SDx
            if (cmdNibble1 == 0xD) {
                event.time += (frames)cmdNibble2 * IT_TICK_TIME;
            }
        } else if (cell.instrument > 0 && cell.command == 15) { // Oxx
            event.sample = getOffsetSample(cell.instrument - 1,
                                           cell.commandValue);
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

shared_ptr<Sample> ITLoader::getOffsetSample(int i, int value)
{
    if (offsetSamples[i].count(value))
        return offsetSamples[i][value];
    frames offset = value * 256;
    auto source = song->samples[i];
    if (source->channels.size() == 0 || source->channels[0].size() < offset)
        return source;

    auto sample = song->samples.emplace_back(new Sample);
    offsetSamples[i][value] = sample;
    *sample = *source;

    sample->name += " O" + leftPad(toUpper(hex(value)), 2);
    for (auto &channel : sample->channels) {
        channel.erase(channel.begin(), channel.begin() + offset);
    }
    if (sample->loopStart >= offset)
        sample->loopStart -= offset;
    else
        sample->loopStart = 0;
    if (sample->loopEnd >= offset)
        sample->loopEnd -= offset;
    else
        sample->loopEnd = 0;

    return sample;
}

} // namespace
