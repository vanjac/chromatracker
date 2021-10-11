#pragma once
#include <common.h>

#include <units.h>

namespace chromatracker::play {

using framesFine = int64_t; // 32.16 fixed point

constexpr framesFine framesToFine(frames f) {
    return (framesFine)f << 16u;
}

constexpr frames fineToFrames(framesFine f) {
    return f >> 16u;
}

} // namespace
