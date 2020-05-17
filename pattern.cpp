#include "pattern.h"
#include <stdio.h>

static const char * NOTE_NAMES = "C-C#D-D#E-F-F#G-G#A-A#B-";

static void effect_to_string(char effect, Uint8 value, char * str);


int event_is_empty(Event event) {
    return !event.instrument[0] && event.p_effect == EFFECT_NONE
        && event.v_effect == EFFECT_NONE;
}

int instrument_is_special(Event event) {
    char c = event.instrument[0];
    return !( (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') );
}

void event_to_string(Event e, char * str) {
    if (e.instrument[0] == EVENT_NOTE_CHANGE)
        memcpy(str, "  ", 2);
    else
        memcpy(str, e.instrument, 2);
    str[2] = ' ';
    effect_to_string(e.p_effect, e.p_value, &str[3]);
    str[6] = ' ';
    effect_to_string(e.v_effect, e.v_value, &str[7]);
    str[10] = 0;
}

static void effect_to_string(char effect, Uint8 value, char * str) {
    if (effect == EFFECT_NONE)
        memcpy(str, "   ", 3);
    else if (effect == EFFECT_PITCH) {
        int note_num = value % 12;
        memcpy(str, &NOTE_NAMES[note_num * 2], 2);
        str[2] = (value / 12) + '0';
    } else {
        if (effect == EFFECT_VELOCITY)
            str[0] = ' ';
        else
            str[0] = effect;
        sprintf(&str[1], "%.2X", value);
    }
}


Pattern::Pattern()
: length(0) { }
