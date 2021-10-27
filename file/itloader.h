#pragma once
#include <common.h>

#include "types.h"
#include <song.h>
#include <SDL2/SDL_rwops.h>

namespace chromatracker::file {

class ITLoader : public ModuleLoader
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

    void loadSample(shared_ptr<Sample> sample, InstrumentExtra *extra);
    void loadInstrument(shared_ptr<Sample> sample, InstrumentExtra *extra);
    void loadSection(shared_ptr<Section> section);

    SDL_RWops *stream;

    Song *song;
    uint16_t compatibleVersion;
    vector<shared_ptr<Sample>> itSamples;
    vector<InstrumentExtra> itSampleExtras;
    vector<InstrumentExtra> instrumentExtras;
    uint8_t ticksPerRow;
    int maxUsedChannel = 0;
};

} // namespace
