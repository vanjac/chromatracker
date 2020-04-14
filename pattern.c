#include "pattern.h"

void free_pattern(Pattern * pattern) {
    if (pattern->events) {
        free(pattern->events);
        pattern->events = NULL;
    }
}

void free_track(Track * track) {
    for (int i = 0; i < NUM_TRACK_PATTERNS; i++)
        free_pattern(&track->patterns[i]);
}
