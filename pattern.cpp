#include "pattern.h"

int event_is_empty(Event event) {
    return !event.instrument[0] && event.p_effect == EFFECT_NONE
        && event.v_effect == EFFECT_NONE;
}

int instrument_is_special(Event event) {
    char c = event.instrument[0];
    return !( (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') );
}

void init_pattern(Pattern * pattern) {
    pattern->events = NULL;
    pattern->num_events = 0;
    pattern->alloc_events = 0;
    pattern->length = 0;
}

void free_pattern(Pattern * pattern) {
    delete pattern->events;
    pattern->events = NULL;
}

void init_track(Track * track) {
    for (int i = 0; i < NUM_TRACK_PATTERNS; i++)
        init_pattern(&track->patterns[i]);
}

void free_track(Track * track) {
    for (int i = 0; i < NUM_TRACK_PATTERNS; i++)
        free_pattern(&track->patterns[i]);
}
