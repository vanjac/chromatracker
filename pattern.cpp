#include "pattern.h"
#include <stdio.h>

static const char * NOTE_NAMES = "C-C#D-D#E-F-F#G-G#A-A#B-";
static const char PLAYBACK_EVENT_NAMES[][4] = {
    "   ", "Pit", "Vel",
    "Tmp", "Pau", "Jmp", "Rep", "Vol" };

static bool effect_is_redundant(Uint8 prev_effect, Uint8 prev_value,
    Uint8 cur_effect, Uint8 cur_value);
// types of effects defined in Song Specification
static bool effect_is_instant(Uint8 effect);
static bool effect_is_parameter(Uint8 effect);
static bool effect_is_continuous(Uint8 effect);
static void effect_to_string(char effect, Uint8 value, char * str);


bool event_is_empty(Event event) {
    char event_type = event.instrument[0];
    // events that do nothing without effects
    return (event_type == EVENT_NOTE_CHANGE || event_type == EVENT_PLAYBACK || event_type == EVENT_COMBINE)
        && event.p_effect == EFFECT_NONE && event.v_effect == EFFECT_NONE;
}

void clear_event(Event * event) {
    event->instrument[0] = event->instrument[1] = EVENT_NOTE_CHANGE;
    event->p_effect = event->v_effect = EFFECT_NONE;
    event->p_value = event->v_value = 0;
}

bool event_is_redundant(Event prev_event, Event cur_event) {
    char cur_event_type = cur_event.instrument[0];
    char prev_event_type = prev_event.instrument[0];

    if (!instrument_is_special(cur_event) || cur_event_type == EVENT_REPLAY
            || cur_event_type == EVENT_COMBINE || cur_event_type == EVENT_PLAYBACK)
        return false;
    if (cur_event_type != EVENT_NOTE_CHANGE && cur_event_type != prev_event_type)
        return false;
    return effect_is_redundant(prev_event.p_effect, prev_event.p_value,
        cur_event.p_effect, cur_event.p_value)
        && effect_is_redundant(prev_event.v_effect, prev_event.v_value,
        cur_event.v_effect, cur_event.v_value);
}

bool effect_is_redundant(Uint8 prev_effect, Uint8 prev_value,
        Uint8 cur_effect, Uint8 cur_value) {
    if (cur_effect == EFFECT_NONE) {
        // wouldn't interrupt previous effect
        return !effect_is_continuous(prev_effect);
    } else if (effect_is_instant(cur_effect) || effect_is_parameter(cur_effect)) {
        // parameters could be changed by other effects
        return false;
    } else {
        // continuous
        return cur_effect == prev_effect && cur_value == prev_value;
    } 
}

bool effect_is_instant(Uint8 effect) {
    return effect == EFFECT_TUNE || effect == EFFECT_SAMPLE_OFFSET;
}

bool effect_is_parameter(Uint8 effect) {
    return effect == EFFECT_PITCH || effect == EFFECT_VELOCITY
        || effect == EFFECT_PAN || effect == EFFECT_BACKWARDS;
}

bool effect_is_continuous(Uint8 effect) {
    return effect != EFFECT_NONE && !effect_is_instant(effect) && !effect_is_parameter(effect);
}

bool instrument_is_special(Event event) {
    char c = event.instrument[0];
    return !( (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') );
}

void event_to_string(Event e, char * str) {
    if (e.instrument[0] == EVENT_NOTE_CHANGE)
        memcpy(str, "  ", 2);
    else
        memcpy(str, e.instrument, 2);
    str[2] = ' ';
    if (e.instrument[0] == EVENT_PLAYBACK)
        memcpy(&str[3], PLAYBACK_EVENT_NAMES[e.p_effect], 3);
    else
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
