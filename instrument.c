#include "instrument.h"

InstSample * new_inst_sample(void) {
    InstSample * sample = malloc(sizeof(InstSample));
    sample->wave = NULL;
    sample->wave_len = 0;
    sample->c5_freq = 48000;
    sample->playback_mode = SMP_NORMAL;
    sample->loop_type = LOOP_FORWARD;
    sample->loop_start = 0;
    sample->loop_end = 0;
    return sample;
}

void delete_inst_sample(InstSample * sample) {
    if (sample->wave)
        free(sample->wave);
    free(sample);
}