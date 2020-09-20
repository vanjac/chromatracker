#ifndef CHROMATRACKER_PATTERN_H
#define CHROMATRACKER_PATTERN_H

#include <string>
#include <variant>
#include <vector>
#include "Instrument.h"

namespace chromatracker {

// these go in instrument column
const Instrument *const EVENT_NOTE_GLIDE = reinterpret_cast<Instrument *>(0);
const Instrument *const EVENT_NOTE_OFF = reinterpret_cast<Instrument *>(1);

const int PITCH_NONE = -1;
const float VELOCITY_NONE = -1.0f;

struct NoteEventData {
    NoteEventData();  // empty glide event
    // note on
    NoteEventData(Instrument *instrument, int pitch, float velocity);

    bool is_empty();
    bool is_note_on();

    const Instrument * instrument;
    int pitch;
    float velocity;
    bool velocity_slide;
};

struct LabelEventData {
    LabelEventData();
    std::string text;
};

struct Event {
    Event();
    Event(int time, NoteEventData data);
    Event(int time, LabelEventData data);
    int time;  // in ticks
    std::variant<NoteEventData, LabelEventData> data;
};

struct Pattern {
    Pattern();
    char id[2];
    std::vector<Event> events;
    int length;  // ticks
};

}

#endif //CHROMATRACKER_PATTERN_H
