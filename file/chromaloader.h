#pragma once
#include <common.h>

#include "chroma.h"
#include "types.h"
#include <song.h>
#include <unordered_map>
#include <SDL2/SDL_rwops.h>

namespace chromatracker::file::chroma {

class Loader : public ModuleLoader
{
public:
    Loader(SDL_RWops *stream);
    ~Loader();

    void loadSong(Song *song) override;
    vector<string> listSamples() override;
    void loadSample(int index, shared_ptr<Sample> sample) override;

private:
    float readFloat();
    string readString();

    void loadHeader();

    void loadSongInfo(uint32_t offset, Song *song);
    void loadSample(uint32_t offset, shared_ptr<Sample> sample);
    void loadWave(uint32_t offset, vector<vector<float>> &wave);
    void loadTrack(uint32_t offset, shared_ptr<Track> track);
    void loadSection(uint32_t offset, shared_ptr<Section> section);
    void loadEvents(uint32_t offset, vector<vector<Event>> &trackEvents);

    std::unordered_map<ObjectType, vector<uint32_t>> objectOffsets;

    SDL_RWops *stream;
    Song *song;
};

} // namespace
