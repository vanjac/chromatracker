#ifndef PATTERN_H
#define PATTERN_H

#include "chroma.h"

#define INST_MASK       0x07FF
#define CONTROL_MASK    0xF800

// these go in the instrument column
#define NOTE_RELEASE    0x07FF
#define NOTE_CUT        0x07FE
#define NOTE_FADE       0x07FD

// these go in the control command column
#define CTL_NONE        0x0000
// this bit can be added to any slide command or None
#define CTL_MODULATION  0x0800
// slide commands:
#define CTL_PORT_UP     0x1000
#define CTL_PORT_DOWN   0x2000
#define CTL_FPORT_UP    0x3000  // fine
#define CTL_FPORT_DOWN  0x4000
#define CTL_VEL_UP      0x5000
#define CTL_VEL_DOWN    0x6000
#define CTL_PORT_NOTE   0x7000
// other commands: (could use modulation bit for something else)
#define CTL_SLICE       0x8000
#define CTL_FINETUNE    0x9000

#define NO_PITCH ((Sint8)-1)
#define NO_VELOCITY ((Sint8)-1)

#define PARAM_IS_NUM   (1<<11) // specifies that param value is numeric (not ID)
#define PARAM_NUM_MASK  0x7F

typedef struct {
    Uint16 time; // in ticks
    Uint16 inst_control;
    Sint8 pitch;
    Sint8 velocity; // negative is empty
    Uint16 param;
} Event;

int event_is_empty(Event event);

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