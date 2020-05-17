#ifndef PATTERN_H
#define PATTERN_H

#include "chroma.h"

// these go in the instrument column
enum Events {
    EVENT_NOTE_CHANGE       = 0,
    EVENT_CANCEL_EFFECTS    = 'x',
    EVENT_NOTE_OFF          = '=',
    EVENT_NOTE_CUT          = '/',
    EVENT_NOTE_FADE         = '~',
    EVENT_REPLAY            = '+',
    EVENT_COMBINE           = '<',
    EVENT_PLAYBACK          = '#'
};

enum Effects {
    EFFECT_NONE             = 0,
    EFFECT_PITCH            = 1, // pitch column only
    EFFECT_VELOCITY         = 2, // velocity column only

    EFFECT_PITCH_SLIDE_UP   = 'U',
    EFFECT_PITCH_SLIDE_DOWN = 'D',
    EFFECT_GLIDE            = 'G',
    EFFECT_TUNE             = 'F',
    EFFECT_VIBRATO          = 'V',
    EFFECT_VEL_SLIDE_UP     = 'I',
    EFFECT_VEL_SLIDE_DOWN   = 'O',
    EFFECT_TREMOLO          = 'T',
    EFFECT_PAN              = 'P',
    EFFECT_PAN_SLIDE_LEFT   = 'L',
    EFFECT_PAN_SLIDE_RIGHT  = 'R',
    EFFECT_AUTO_PAN         = 'N',
    EFFECT_SAMPLE_OFFSET    = 'S',
    EFFECT_BACKWARDS        = 'B',

    // special playback events, pitch column only
    EFFECT_TEMPO             = 3,
    EFFECT_PAUSE             = 4,
    EFFECT_JUMP              = 5,
    EFFECT_REPEAT            = 6,
    EFFECT_MASTER_VOLUME     = 7
};

#define MIDDLE_C (5*12)
#define MAX_VELOCITY 0x80

struct Event {
    Uint16 time; // in ticks
    char instrument[2];
    char p_effect; // pitch column
    Uint8 p_value;
    char v_effect; // velocity column
    Uint8 v_value;
};

int event_is_empty(Event event);
int instrument_is_special(Event event);

struct Pattern {
    Event * events;
    int num_events;
    int alloc_events;
    int length; // ticks
};

void init_pattern(Pattern * pattern);
void free_pattern(Pattern * pattern);

#define NUM_TRACK_PATTERNS 256
#define MAX_PAGES 256

struct Track {
    Pattern patterns[NUM_TRACK_PATTERNS];
    Uint8 pages[MAX_PAGES]; // patter number selections
};

void init_track(Track * track);
void free_track(Track * track);

#endif