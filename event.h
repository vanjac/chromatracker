#pragma once
#include <common.h>

#include "sample.h"
#include "units.h"

namespace chromatracker {

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

    enum Mask // enum classes don't have bitwise operations :(
    {
        NO_MASK = 0,
        SAMPLE = (1<<0),
        PITCH = (1<<1),
        VELOCITY = (1<<2),
        SPECIAL = (1<<3),
        ALL = SAMPLE | PITCH | VELOCITY | SPECIAL
    };

    ticks time {0};
    ObjWeakPtr<Sample> sample;
    int pitch {NO_PITCH};
    float velocity {NO_VELOCITY};
    Special special {Special::None};

    bool empty() const;
    void merge(const Event &other); // fields of other take precedence
    void merge(const Event &other, Mask mask);
};

} // namespace
