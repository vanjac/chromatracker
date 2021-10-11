#pragma once
#include <common.h>

#include "event.h"
#include "sample.h"
#include "units.h"
#include <shared_mutex> // https://stackoverflow.com/q/50972345

namespace chromatracker {

struct Track
{
    mutable nevercopy<std::shared_mutex> mu;
    bool mute {false};
    float volume {1.0};
    float pan {0.0};
};

struct Section
{
    static const int NO_TEMPO; // keep previous
    static const int NO_METER;

    mutable nevercopy<std::shared_mutex> mu; // also protects all events

    ticks length {0};
    vector<vector<Event>> trackEvents;

    string title;
    int tempo {NO_TEMPO}; // BPM
    int meter {NO_METER}; // beats per bar

    // section to play after this one
    // typically the next in the sequence, but may be altered to loop/skip/stop
    Section *next {nullptr};
};

struct Song
{
    // protects the song properties and vectors, but not contained objects
    mutable nevercopy<std::shared_mutex> mu;

    vector<unique_ptr<Sample>> samples;
    vector<unique_ptr<Track>> tracks;
    vector<unique_ptr<Section>> sections;

    float volume {0.5};
};

} // namespace
