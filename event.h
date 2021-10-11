#pragma once
#include <common.h>

#include "units.h"

namespace chromatracker {

struct Sample;

struct Event
{
    static const int NO_PITCH; // keep previous
    static const float NO_VELOCITY;

    enum class Special
    {
        None = 0,
        FadeOut = 1,
        Slide = 2,
    };

    ticks time {0};
    Sample *sample {nullptr};
    int pitch {NO_PITCH};
    float velocity {NO_VELOCITY};
    Special special {Special::None};

    bool empty();
};

} // namespace
