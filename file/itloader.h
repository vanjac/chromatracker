#pragma once
#include <common.h>

#include "types.h"
#include <song.h>
#include <SDL2/SDL_rwops.h>

namespace chromatracker::file {

class ITLoader : public ModuleLoader
{
public:
    ITLoader(SDL_RWops *stream); // takes ownership of stream
    ~ITLoader();

    void loadSong(Song *song) override;
    vector<string> listSamples() override;
    void loadSample(int index, shared_ptr<Sample> sample) override;

private:
    struct InstrumentExtra
    {
        uint8_t defaultVolume = 64;
        bool autoFade = false;
    };

    void checkHeader();
    void loadObjects();

    void loadITSample(uint32_t offset, shared_ptr<Sample> sample,
                      InstrumentExtra *extra);
    void checkSampleHeader(uint32_t offset);
    void loadInstrument(uint32_t offset,
                        shared_ptr<Sample> sample, InstrumentExtra *extra);
    void checkInstrumentHeader(uint32_t offset);
    void loadPattern(uint32_t offset, shared_ptr<Section> section);

    SDL_RWops *stream;

    Song *song;
    uint16_t compatibleVersion;
    bool instrumentMode;

    uint16_t numOrders, numInstruments, numSamples, numPatterns;
    unique_ptr<uint8_t[]> orders;
    unique_ptr<uint32_t[]> instOffsets, sampleOffsets, patternOffsets;

    vector<shared_ptr<Sample>> itSamples;
    vector<InstrumentExtra> itSampleExtras;
    vector<InstrumentExtra> instrumentExtras;
    uint8_t ticksPerRow;
    int maxUsedChannel = 0;
};

} // namespace
