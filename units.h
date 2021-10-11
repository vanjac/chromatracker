#pragma once
#include <common.h>

namespace chromatracker {

using ticks = int32_t;
const ticks TICKS_PER_BEAT = 192;

using frames = int32_t; // 1 frame = 1 sample in all channels of a wave

const int OCTAVE = 12;
const int MIN_PITCH = 0;                        // C-0
const int MAX_PITCH = OCTAVE * 10 - 1;          // B-9
const int MIDDLE_OCTAVE = 5;
const int MIDDLE_C = OCTAVE * MIDDLE_OCTAVE;    // C-5

string pitchToString(int pitch);

// velocity and amplitude (volume) are both 0 to 1
// velocity is a measure of loudness
float velocityToAmplitude(float velocity);
float amplitudeToVelocity(float amplitude);

// panning is -1 (left) to 1 (right)
float panningToLeftAmplitude(float pan);
float panningToRightAmplitude(float pan);

} // namespace
