#include "instrument.h"
#include "pattern.h"

InstSample::InstSample()
: wave(NULL), wave_len(0),
c5_freq(48000),
playback_mode(SMP_NORMAL),
loop_type(LOOP_FORWARD), loop_start(0), loop_end(0), num_slices(0),
default_pitch(MIDDLE_C), default_velocity(MAX_VELOCITY) { }

InstSample::~InstSample() {
    delete [] wave;
}
