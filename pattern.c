#include "pattern.h"

Pattern * new_pattern(void) {
    Pattern * pattern = malloc(sizeof(Pattern));
    pattern->events = NULL;
    pattern->num_events = 0;
    pattern->events_alloc = 0;
    pattern->length = 0;
}

void delete_pattern(Pattern * pattern) {
    if (pattern->events)
        free(pattern->events);
    free(pattern);
}
