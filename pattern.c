#include "pattern.h"

int event_is_empty(Event event) {
    return !event.inst_control && event.pitch == NO_PITCH
        && event.velocity == NO_VELOCITY && !event.param;
}

void init_pattern(Pattern * pattern) {
    pattern->events = NULL;
    pattern->num_events = 0;
    pattern->alloc_events = 0;
    pattern->length = 0;
}

void free_pattern(Pattern * pattern) {
    if (pattern->events) {
        free(pattern->events);
        pattern->events = NULL;
    }
}

void init_track(Track * track) {
    for (int i = 0; i < NUM_TRACK_PATTERNS; i++)
        init_pattern(&track->patterns[i]);
}

void free_track(Track * track) {
    for (int i = 0; i < NUM_TRACK_PATTERNS; i++)
        free_pattern(&track->patterns[i]);
}
