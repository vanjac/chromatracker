#ifndef CHROMATRACKER_UNITS_H
#define CHROMATRACKER_UNITS_H

#include <string>

namespace chromatracker {

const int MIN_KEY = 0;
const int MAX_KEY = 12 * 10 - 1;  // inclusive
const int MIDDLE_C = 12 * 5;

const int TICKS_PER_BEAT = 192;
const int DEFAULT_TEMPO = 125;  // compatibility with MOD

const int OUT_CHANNELS = 2;

std::string pitch_to_string(int pitch);

float volume_control_to_amplitude(float vol);
float amplitude_to_volume_control(float amp);
float panning_control_to_left_amplitude(float pan);
float panning_control_to_right_amplitude(float pan);

}

#endif //CHROMATRACKER_UNITS_H
