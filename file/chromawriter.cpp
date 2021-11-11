#include "chromawriter.h"
#include <version.h>
#include <algorithm>

namespace chromatracker::file::chroma {

template<typename T>
int findIndex(const vector<T> v, T item) {
    if (item == nullptr)
        return -1;
    auto it = std::find(v.begin(), v.end(), item);
    if (it == v.end()) {
        return -1;
    } else {
        return it - v.begin();
    }
}

Writer::Writer(SDL_RWops *stream)
    : stream(stream)
{}

Writer::~Writer()
{
    SDL_RWclose(stream);
}

void Writer::writeSong(const Song *song)
{
    this->song = song;
    std::shared_lock songLock(song->mu);

    SDL_RWwrite(stream, MAGIC, 1, 4);
    SDL_WriteLE16(stream, VERSION);
    SDL_WriteLE16(stream, 0); // compatible version

    // object directory
    int numObjects = 0;
    SDL_WriteLE16(stream, 6); // num types
    SDL_WriteLE16(stream, 0);
    numObjects += writeObjectType(ObjectType::Song, 1);
    numObjects += writeObjectType(ObjectType::Sample, song->samples.size());
    numObjects += writeObjectType(ObjectType::Track, song->tracks.size());
    numObjects += writeObjectType(ObjectType::Section, song->sections.size());
    // largest objects at the end
    numObjects += writeObjectType(ObjectType::Events, song->sections.size());
    numObjects += writeObjectType(ObjectType::Wave, song->samples.size());

    uint32_t objectOffsetsOffset = SDL_RWtell(stream);
    SDL_RWseek(stream, 4 * numObjects, RW_SEEK_CUR);
    vector<uint32_t> objectOffsets;
    objectOffsets.reserve(numObjects);

    objectOffsets.push_back(writeSongInfo(song));
    for (auto &sample : song->samples)
        objectOffsets.push_back(writeSample(sample));
    for (auto &track : song->tracks)
        objectOffsets.push_back(writeTrack(track));
    for (auto &section : song->sections)
        objectOffsets.push_back(writeSection(section));
    for (auto &section : song->sections) {
        std::shared_lock sectionLock(section->mu);
        objectOffsets.push_back(writeEvents(section->trackEvents));
    }
    for (auto &sample : song->samples) {
        std::shared_lock sampleLock(sample->mu);
        objectOffsets.push_back(writeWave(sample->channels));
    }

    SDL_RWseek(stream, objectOffsetsOffset, RW_SEEK_SET);
    // TODO endianess
    SDL_RWwrite(stream, objectOffsets.data(),
                sizeof(uint32_t), objectOffsets.size());
}

void Writer::writeFloat(float f)
{
    SDL_WriteLE32(stream, *(uint32_t *)&f);
}

void Writer::writeString(string s)
{
    if (s.size() > 65535)
        s = s.substr(0, 65535);
    SDL_WriteLE16(stream, s.size());
    SDL_RWwrite(stream, s.c_str(), 1, s.size());
}

uint16_t Writer::writeObjectType(ObjectType type, uint16_t count)
{
    SDL_WriteLE16(stream, (uint16_t)type);
    SDL_WriteLE16(stream, count);
    return count;
}

uint32_t Writer::writeSongInfo(const Song *song)
{
    uint32_t offset = SDL_RWtell(stream);
    writeFloat(song->volume);
    return offset;
}

uint32_t Writer::writeSample(shared_ptr<const Sample> sample)
{
    uint32_t offset = SDL_RWtell(stream);
    std::shared_lock lock(sample->mu);

    writeString(sample->name);
    SDL_WriteU8(stream, sample->color.r * 255);
    SDL_WriteU8(stream, sample->color.g * 255);
    SDL_WriteU8(stream, sample->color.b * 255);
    uint8_t flags =
        (uint8_t)sample->interpolationMode << INTERPOLATION_MODE_FLAG
        | (uint8_t)sample->loopMode << LOOP_MODE_FLAG
        | (uint8_t)sample->newNoteAction << NEW_NOTE_ACTION_FLAG;
    SDL_WriteU8(stream, flags);
    SDL_WriteLE32(stream, sample->frameRate);
    SDL_WriteLE32(stream, sample->loopStart);
    SDL_WriteLE32(stream, sample->loopEnd);
    writeFloat(sample->volume);
    writeFloat(sample->tune);
    writeFloat(sample->fadeOut);

    return offset;
}

uint32_t Writer::writeWave(const vector<vector<float>> &wave)
{
    uint32_t offset = SDL_RWtell(stream);

    if (wave.empty()){
        SDL_WriteLE32(stream, 0);
        SDL_WriteLE16(stream, 0);
        SDL_WriteLE16(stream, (uint16_t)WaveFormat::Float);
        return offset;
    }
    SDL_WriteLE32(stream, wave[0].size());
    SDL_WriteLE16(stream, wave.size());
    SDL_WriteLE16(stream, (uint16_t)WaveFormat::Float);
    for (auto &channel : wave) {
        // TODO endianess
        SDL_RWwrite(stream, channel.data(), sizeof(float), channel.size());
    }

    return offset;
}

uint32_t Writer::writeTrack(shared_ptr<const Track> track)
{
    uint32_t offset = SDL_RWtell(stream);
    std::shared_lock lock(track->mu);

    writeFloat(track->volume);
    writeFloat(track->pan);
    SDL_WriteU8(stream, track->mute ? (1<<MUTE_FLAG) : 0);

    return offset;
}

uint32_t Writer::writeSection(shared_ptr<const Section> section)
{
    uint32_t offset = SDL_RWtell(stream);
    std::shared_lock lock(section->mu);

    writeString(section->title);
    SDL_WriteLE32(stream, section->length);
    SDL_WriteLE16(stream, section->tempo);
    SDL_WriteLE16(stream, section->meter);
    SDL_WriteLE16(stream, findIndex(song->sections, section->next.lock()));

    return offset;
}

uint32_t Writer::writeEvents(const vector<vector<Event>> &trackEvents)
{
    uint32_t offset = SDL_RWtell(stream);

    for (auto &events : trackEvents) {
        SDL_WriteLE32(stream, events.size());
        for (auto &event : events) {
            SDL_WriteLE32(stream, event.time);
            int sampleIndex = findIndex(song->samples, event.sample.lock());
            SDL_WriteLE16(stream, sampleIndex);
            SDL_WriteU8(stream, event.pitch);
            SDL_WriteU8(stream, (uint8_t)event.special);
            writeFloat(event.velocity);
        }
    }

    return offset;
}

} // namespace
