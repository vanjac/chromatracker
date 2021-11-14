#include "units.h"

#include <glm/glm.hpp>

namespace chromatracker {

static const char * NOTE_NAMES = "C-C#D-D#E-F-F#G-G#A-A#B-";

string pitchToString(int pitch) {
    char buf[4];
    int note = pitch % OCTAVE;
    memcpy(buf, &NOTE_NAMES[note * 2], 2);
    buf[2] = (pitch / OCTAVE) + '0';
    buf[3] = 0;
    return buf;
}

// https://www.cs.cmu.edu/~rbd/papers/velocity-icmc2006.pdf
// MIDI velocity to amplitude is typically a quadratic relationship
// (other sources suggest a 36dB exponential mapping)
// OpenMPT also uses quadratic function for "exponential" (actually Power):
// https://github.com/OpenMPT/openmpt/blob/master/tracklib/FadeLaws.h
float velocityToAmplitude(float velocity)
{
    return velocity * velocity;
}

float amplitudeToVelocity(float amplitude)
{
    return glm::sqrt(amplitude);
}

float panningToLeftAmplitude(float pan)
{
    // https://www.cs.cmu.edu/~music/icm-online/readings/panlaws/
    // https://www.harmonycentral.com/articles/recording/the-truth-about-panning-laws-r501/

    // constant power panning, -3dB center:
    // float angle = (pan + 1) * (PI / 4);
    // return glm::cos(angle);

    // linear panning, -6dB center:
    return (1 - pan) / 2;
}

float panningToRightAmplitude(float pan)
{
    // float angle = (pan + 1) * (PI / 4);
    // return glm::sin(angle);
    return (pan + 1) / 2;
}

} // namespace
