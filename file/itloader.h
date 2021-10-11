#pragma once
#include <common.h>

#include <song.h>
#include <SDL2/SDL_rwops.h>

namespace chromatracker::file {

class ITLoader
{
public:
    ITLoader(SDL_RWops *stream);

    void loadSong(Song *song);

private:
    struct InstrumentExtra
    {
        uint8_t defaultVolume = 64;
        bool autoFade = false;
    };

    void loadSample(Sample *sample, InstrumentExtra *extra);
    void loadInstrument(Sample *sample, InstrumentExtra *extra);
    void loadSection(Section *section);

    SDL_RWops *stream;

    Song *song;
    uint16_t compatibleVersion;
    vector<Sample> itSamples;
    vector<InstrumentExtra> itSampleExtras;
    vector<InstrumentExtra> instrumentExtras;
    uint8_t ticksPerRow;
    int maxUsedChannel = 0;
};

} // namespace
