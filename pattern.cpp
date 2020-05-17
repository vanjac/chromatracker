#include "pattern.h"

int event_is_empty(Event event) {
    return !event.instrument[0] && event.p_effect == EFFECT_NONE
        && event.v_effect == EFFECT_NONE;
}

int instrument_is_special(Event event) {
    char c = event.instrument[0];
    return !( (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') );
}

Pattern::Pattern()
: events(NULL), num_events(0), alloc_events(0),
length(0) { }

Pattern::~Pattern() {
    delete [] events;
}
