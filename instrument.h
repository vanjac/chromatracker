#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include "chroma.h"

typedef struct {
    Sample * wave; // TODO allow mono
    int wave_len;
    float c5_freq;
    enum {SMP_NORMAL, SMP_LOOP, SMP_SUSTAIN} playback_mode;
    enum {LOOP_FORWARD, LOOP_PINGPONG} loop_type;
    int loop_start, loop_end;
} InstSample;

InstSample * new_inst_sample(void);
void delete_inst_sample(InstSample * sample);

#endif