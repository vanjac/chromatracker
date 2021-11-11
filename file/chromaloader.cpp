#include "chromaloader.h"
#include <version.h>
#include <stdexcept>

namespace chromatracker::file::chroma {

struct TypeCount
{
    ObjectType type;
    uint16_t count;
};

Loader::Loader(SDL_RWops *stream)
    : stream(stream)
{
    loadHeader();
}

Loader::~Loader()
{
    SDL_RWclose(stream);
}

void Loader::loadHeader()
{
    char signature[4];
    SDL_RWread(stream, signature, 1, 4);
    if (memcmp(signature, MAGIC, 4)) {
        throw std::runtime_error("Unrecognized format");
    }

    SDL_RWseek(stream, 2, RW_SEEK_CUR); // skip created version
    uint16_t compatibleVersion = SDL_ReadLE16(stream);
    if (compatibleVersion > VERSION) {
        throw std::runtime_error(
            "This file requires a newer version of chromatracker");
    }

    uint16_t numTypes = SDL_ReadLE16(stream);
    SDL_RWseek(stream, 2, RW_SEEK_CUR);
    vector<TypeCount> typeCounts;
    typeCounts.reserve(numTypes);
    for (int i = 0; i < numTypes; i++) {
        TypeCount &tc = typeCounts.emplace_back();
        tc.type = (ObjectType)SDL_ReadU8(stream);
        SDL_RWseek(stream, 1, RW_SEEK_CUR);
        tc.count = SDL_ReadLE16(stream);
    }

    for (auto &tc : typeCounts) {
        vector<uint32_t> &offsetsVec = objectOffsets[tc.type];
        offsetsVec.reserve(tc.count);
        for (int i = 0; i < tc.count; i++)
            offsetsVec.push_back(SDL_ReadLE32(stream));
    }
}

float Loader::readFloat()
{
    uint32_t i = SDL_ReadLE32(stream);
    return *(float *)&i;
}

string Loader::readString()
{
    uint16_t size = SDL_ReadLE16(stream);
    unique_ptr<char[]> buffer(new char[size + 1]);
    SDL_RWread(stream, buffer.get(), 1, size);
    buffer[size] = 0;
    return buffer.get();
}

void Loader::loadSong(Song *song)
{
    this->song = song;

    auto &songOffsets = objectOffsets[ObjectType::Song];
    if (!songOffsets.empty())
        loadSongInfo(songOffsets[0], song);

    auto &sampleOffsets = objectOffsets[ObjectType::Sample];
    song->samples.reserve(sampleOffsets.size());
    for (uint32_t offset : sampleOffsets)
        loadSample(offset, song->samples.emplace_back(new Sample));

    auto &waveOffsets = objectOffsets[ObjectType::Wave];
    int numWaves = glm::min(waveOffsets.size(), sampleOffsets.size());
    for (int i = 0; i < numWaves; i++)
        loadWave(waveOffsets[i], song->samples[i]->channels);

    auto &trackOffsets = objectOffsets[ObjectType::Track];
    song->tracks.reserve(trackOffsets.size());
    for (uint32_t offset : trackOffsets)
        loadTrack(offset, song->tracks.emplace_back(new Track));

    auto &sectionOffsets = objectOffsets[ObjectType::Section];
    song->sections.reserve(sectionOffsets.size());
    // create sections first for next section references
    for (int i = 0; i < sectionOffsets.size(); i++)
        song->sections.emplace_back(new Section);
    for (int i = 0; i < sectionOffsets.size(); i++)
        loadSection(sectionOffsets[i], song->sections[i]);

    auto &eventsOffsets = objectOffsets[ObjectType::Events];
    int numEventses = glm::min(sectionOffsets.size(), eventsOffsets.size());
    for (int i = 0; i < numEventses; i++)
        loadEvents(eventsOffsets[i], song->sections[i]->trackEvents);
}

vector<string> Loader::listSamples()
{
    vector<string> sampleNames;

    auto &sampleOffsets = objectOffsets[ObjectType::Sample];
    sampleNames.reserve(sampleOffsets.size());
    for (uint32_t offset : sampleOffsets) {
        SDL_RWseek(stream, offset, RW_SEEK_SET);
        sampleNames.push_back(readString());
    }

    return sampleNames;
}

void Loader::loadSample(int index, shared_ptr<Sample> sample)
{
    auto &sampleOffsets = objectOffsets[ObjectType::Sample];
    if (index < 0 || index >= sampleOffsets.size())
        return;
    loadSample(sampleOffsets[index], sample);
}

void Loader::loadSongInfo(uint32_t offset, Song *song)
{
    SDL_RWseek(stream, offset, RW_SEEK_SET);
    song->volume = readFloat();
}

void Loader::loadSample(uint32_t offset, shared_ptr<Sample> sample)
{
    SDL_RWseek(stream, offset, RW_SEEK_SET);
    sample->name = readString();
    sample->color.r = SDL_ReadU8(stream) / 255.0f;
    sample->color.g = SDL_ReadU8(stream) / 255.0f;
    sample->color.b = SDL_ReadU8(stream) / 255.0f;

    uint8_t flags = SDL_ReadU8(stream);
    sample->interpolationMode =
        (Sample::InterpolationMode)((flags >> INTERPOLATION_MODE_FLAG) & 0x3);
    sample->loopMode = (Sample::LoopMode)((flags >> LOOP_MODE_FLAG) & 0x3);
    sample->newNoteAction =
        (Sample::NewNoteAction)((flags >> NEW_NOTE_ACTION_FLAG) & 0x3);

    sample->frameRate = SDL_ReadLE32(stream);
    sample->loopStart = SDL_ReadLE32(stream);
    sample->loopEnd = SDL_ReadLE32(stream);
    sample->volume = readFloat();
    sample->tune = readFloat();
    sample->fadeOut = readFloat();
}

void Loader::loadWave(uint32_t offset, vector<vector<float>> &wave)
{
    SDL_RWseek(stream, offset, RW_SEEK_SET);
    uint32_t numFrames = SDL_ReadLE32(stream);
    uint16_t numChannels = SDL_ReadLE16(stream);
    WaveFormat format = (WaveFormat)SDL_ReadU8(stream);
    SDL_RWseek(stream, 1, RW_SEEK_CUR);
    if (format != WaveFormat::Float)
        return; // unrecognized format
    
    wave.resize(numChannels);
    for (auto &channel : wave) {
        channel.resize(numFrames);
        // TODO endianess
        SDL_RWread(stream, channel.data(), sizeof(float), numFrames);
    }
}

void Loader::loadTrack(uint32_t offset, shared_ptr<Track> track)
{
    SDL_RWseek(stream, offset, RW_SEEK_SET);
    track->volume = readFloat();
    track->pan = readFloat();
    uint8_t flags = SDL_ReadU8(stream);
    track->mute = (flags>>MUTE_FLAG) & 0x1;
}

void Loader::loadSection(uint32_t offset, shared_ptr<Section> section)
{
    SDL_RWseek(stream, offset, RW_SEEK_SET);
    section->title = readString();
    section->length = SDL_ReadLE32(stream);
    section->tempo = (int16_t)SDL_ReadLE16(stream);
    section->meter = (int16_t)SDL_ReadLE16(stream);
    uint16_t nextIndex = SDL_ReadLE16(stream);
    if (nextIndex < song->sections.size())
        section->next = song->sections[nextIndex];
}

void Loader::loadEvents(uint32_t offset, vector<vector<Event>> &trackEvents)
{
    SDL_RWseek(stream, offset, RW_SEEK_SET);
    trackEvents.resize(song->tracks.size());
    for (auto &events : trackEvents) {
        uint32_t numEvents = SDL_ReadLE32(stream);
        events.reserve(numEvents);
        for (int i = 0; i < numEvents; i++) {
            Event &event = events.emplace_back();
            event.time = SDL_ReadLE32(stream);
            uint16_t sampleIndex = SDL_ReadLE16(stream);
            if (sampleIndex < song->samples.size())
                event.sample = song->samples[sampleIndex];
            event.pitch = (int8_t)SDL_ReadU8(stream);
            event.special = (Event::Special)SDL_ReadU8(stream);
            event.velocity = readFloat();
        }
    }
}

} // namespace
