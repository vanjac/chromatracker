#pragma once
#include <common.h>

namespace chromatracker::file::chroma {

const char MAGIC[4] = {'C', 'H', 'T', 'R'};

enum class ObjectType
{
    Song = 0,
    Sample = 1,
    Wave = 2,
    Track = 3,
    Section = 4,
    Events = 5,
};

enum class WaveFormat
{
    Float = 0,
};

// sample flags
const uint8_t INTERPOLATION_MODE_FLAG = 0;
const uint8_t LOOP_MODE_FLAG = 2;
const uint8_t NEW_NOTE_ACTION_FLAG = 4;

// track flags
const uint8_t MUTE_FLAG = 0;

} // namespace
