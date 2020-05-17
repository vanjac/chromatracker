#include "instrument.h"
#include "pattern.h"

void init_inst_sample(InstSample * sample) {
    sample->wave = NULL;
    sample->wave_len = 0;
    sample->c5_freq = 48000;
    sample->playback_mode = SMP_NORMAL;
    sample->loop_type = LOOP_FORWARD;
    sample->loop_start = 0;
    sample->loop_end = 0;
    sample->num_slices = 0;

    sample->default_pitch = MIDDLE_C;
    sample->default_velocity = MAX_VELOCITY;
}

void free_inst_sample(InstSample * sample) {
    delete sample->wave;
    sample->wave = NULL;
}
