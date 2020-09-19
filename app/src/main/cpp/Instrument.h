#ifndef CHROMATRACKER_INSTRUMENT_H
#define CHROMATRACKER_INSTRUMENT_H

#include <string>
#include <list>
#include "InstSample.h"
#include "Modulation.h"

namespace chromatracker {

enum class NewNoteAction {
    OFF, CUT, CONTINUE
};

enum class SampleOverlapMode {
    MIX, RANDOM
};

typedef uint32_t Color;

// each component 0 - 255
inline Color color_rgb(int r, int g, int b) {
    // same as https://developer.android.com/reference/android/graphics/Color
    return (r & 0xff) << 16 | (g & 0xff) << 8 | (b & 0xff);
}

struct Instrument {
    Instrument();

    char id[2];
    std::string name;
    Color color;

    // elements referenced by SamplePlayback
    std::list<InstSample> samples;
    SampleOverlapMode sample_overlap_mode;

    NewNoteAction new_note_action;
    int random_delay;

    /* Volume */
    float volume;
    ADSR volume_adsr;

    /* Panning */
    float panning;

    /* Pitch */
    int transpose;
    float finetune;
    float glide;  // response, scales with tempo
};

}

#endif //CHROMATRACKER_INSTRUMENT_H
