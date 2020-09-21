#include "Pattern.h"

namespace chromatracker {

NoteEventData::NoteEventData() :
        instrument(EVENT_NOTE_GLIDE),
        pitch(PITCH_NONE),
        velocity(VELOCITY_NONE),
        velocity_slide(false) { }

NoteEventData::NoteEventData(Instrument *instrument,
        int pitch, float velocity) :
        instrument(instrument),
        pitch(pitch),
        velocity(velocity),
        velocity_slide(false) { }

bool NoteEventData::is_empty() {
    return instrument == EVENT_NOTE_GLIDE && pitch == PITCH_NONE
            && velocity == VELOCITY_NONE && (!velocity_slide);
}

bool NoteEventData::is_note_on() {
    return instrument != EVENT_NOTE_GLIDE && instrument != EVENT_NOTE_OFF;
}

LabelEventData::LabelEventData() :
        text("") { }

Event::Event() :
        time(0) { }

Event::Event(int time, NoteEventData data) :
        time(time), data(data) { }

Event::Event(int time, LabelEventData data) :
        time(time), data(data) { }

Pattern::Pattern() :
        id{'A', '1'}, length(0) { }

}