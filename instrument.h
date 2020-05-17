#ifndef INSTRUMENT_H
#define INSTRUMENT_H

#include "chroma.h"

#define MAX_SLICES 100

typedef enum {SMP_NORMAL, SMP_LOOP, SMP_SUSTAIN} PlaybackMode;
typedef enum {LOOP_FORWARD, LOOP_PINGPONG} LoopType;

typedef struct {
    Sample * wave; // TODO allow mono
    int wave_len;
    float c5_freq;
    PlaybackMode playback_mode;
    LoopType loop_type;
    int loop_start, loop_end;
    int num_slices;
    int slices[MAX_SLICES];

    // future instrument properties
    Uint8 default_pitch;
    Uint8 default_velocity;
} InstSample;

void init_inst_sample(InstSample * sample);
void free_inst_sample(InstSample * sample);

#endif