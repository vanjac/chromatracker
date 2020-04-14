#include "instrument.h"

void init_inst_sample(InstSample * sample) {
    sample->wave = NULL;
    sample->wave_len = 0;
    sample->c5_freq = 48000;
    sample->playback_mode = SMP_NORMAL;
    sample->loop_type = LOOP_FORWARD;
    sample->loop_start = 0;
    sample->loop_end = 0;
}

void free_inst_sample(InstSample * sample) {
    if (sample->wave) {
        free(sample->wave);
        sample->wave = NULL;
    }
}
