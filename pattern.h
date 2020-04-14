#ifndef PATTERN_H
#define PATTERN_H

#include "chroma.h"

#define INST_MASK 0x07FF
#define EFFECT_MASK 0xF800

// these go in the instrument column
#define NOTE_RELEASE 0x07FF
#define NOTE_CUT 0x07FE
#define NOTE_FADE 0x07FD

typedef struct {
    Uint16 time; // in ticks
    Uint16 inst_effect;
    Sint8 pitch;
    Sint8 velocity; // negative is empty
    Uint16 param;
} Event;

typedef struct {
    Event * events;
    int num_events;
    int alloc_events;
    int length; // ticks
} Pattern;

void init_pattern(Pattern * pattern);
void free_pattern(Pattern * pattern);

#define NUM_TRACK_PATTERNS 99
#define MAX_PAGES 256
#define NO_PATTERN -1

typedef struct {
    Pattern patterns[NUM_TRACK_PATTERNS];
    Sint8 pages[MAX_PAGES]; // patter number selections
} Track;

void init_track(Track * track);
void free_track(Track * track);

#endif