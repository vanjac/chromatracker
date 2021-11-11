#pragma once
#include <common.h>

#include "chroma.h"
#include <song.h>
#include <SDL2/SDL_rwops.h>

namespace chromatracker::file::chroma {

class Writer
{
public:
    Writer(SDL_RWops *stream); // takes ownership of stream
    ~Writer();

    void writeSong(const Song *song);

private:
    void writeFloat(float f);
    void writeString(string s);
    uint16_t writeObjectType(ObjectType type, uint16_t count); // return count

    // return offset
    uint32_t writeSongInfo(const Song *song);
    uint32_t writeSample(shared_ptr<const Sample> sample);
    uint32_t writeWave(const vector<vector<float>> &wave);
    uint32_t writeTrack(shared_ptr<const Track> track);
    uint32_t writeSection(shared_ptr<const Section> section);
    uint32_t writeEvents(const vector<vector<Event>> &trackEvents);

    SDL_RWops *stream;
    const Song *song;
};

} // namespace
