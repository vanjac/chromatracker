#include "Units.h"
// pi
#define _USE_MATH_DEFINES
#include <cmath>

namespace chromatracker {

static const char * NOTE_NAMES = "C-C#D-D#E-F-F#G-G#A-A#B-";


std::string pitch_to_string(int pitch) {
    char buf[4];
    int note_num = pitch % 12;
    memcpy(buf, &NOTE_NAMES[note_num * 2], 2);
    buf[2] = (pitch / 12) + '0';
    buf[3] = 0;
    return buf;
}

float volume_control_to_amplitude(float vol) {
    // https://www.dr-lex.be/info-stuff/volumecontrols.html
    // > Keep in mind that in situations where the maximum volume is rather
    // > quiet you may need a less ‘strong’ curve like x^3
    // TODO: revisit this
    return vol * vol * vol;
}

float amplitude_to_volume_control(float amp) {
    return powf(amp, 1.0f / 3.0f);
}

float panning_control_to_left_amplitude(float pan) {
    // https://www.cs.cmu.edu/~music/icm-online/readings/panlaws/
    // https://www.harmonycentral.com/articles/recording/the-truth-about-panning-laws-r501/
    // constant power panning, -3dB center (I think)
    // TODO: revisit this
    float angle = (pan + 1.0f) * static_cast<float>(M_PI / 4.0f);
    return cosf(angle);
}

float panning_control_to_right_amplitude(float pan) {
    float angle = (pan + 1.0f) * static_cast<float>(M_PI / 4.0f);
    return sinf(angle);
}

}