#include "TrackPlayback.h"
#include <utility>
#include "../Log.h"

namespace chromatracker::play {

TrackPlayback::TrackPlayback(const Track *track) :
        track(track) { }

void TrackPlayback::set_pattern(const Pattern *pattern, int time) {
    this->cursor.set_lock(pattern, time);
}

const PatternCursor TrackPlayback::get_cursor() const {
    return this->cursor;
}

void TrackPlayback::process_tick(float *tick_buffer, int tick_frames,
        SongState *state, float amp) {
    process_pattern_tick(state);

    held_note.process_tick(tick_buffer, tick_frames, state, amp);

    if (track != nullptr && track->mute)
        amp = 0.0f;

    for (int i = 0; i < released_notes.size(); i++) {
        if (!released_notes[i].process_tick(
                tick_buffer, tick_frames, state, amp)) {
            // note is no longer playing. remove from released notes
            unsigned end_i = released_notes.size() - 1;
            if (i != end_i) {
                // the default move constructor will move the vector instead
                // of copying each element
                released_notes[i] = std::move(released_notes[end_i]);
            }
            released_notes.pop_back();
            i--;
            //LOGI("REMOVE %u", released_notes.size());
        }
    }
}

void TrackPlayback::process_pattern_tick(SongState *state) {
    if (this->cursor.is_null())
        return;

    auto lock = this->cursor.lock();
    const std::vector<Event> &events = this->cursor.get_events();
    int event_i = this->cursor.get_event_i();
    if (event_i < events.size()) {
        const Event &next_event = events[event_i];
        if (next_event.time == this->cursor.get_time())
            execute_event(next_event, state);
    }

    this->cursor.move(1);
}

void TrackPlayback::execute_event(const Event &event, SongState *state) {
    if (std::holds_alternative<NoteEventData>(event.data)) {
        const auto &data = std::get<NoteEventData>(event.data);

        if (data.instrument == EVENT_NOTE_GLIDE) {
            if (data.pitch != PITCH_NONE)
                held_note.glide(data.pitch);  // does nothing if not playing
        } else if (data.instrument == EVENT_NOTE_OFF) {
            held_note.release_note();
            if (data.pitch != PITCH_NONE)
                held_note.glide(data.pitch);
        } else {  // note on
            held_note.new_note_action();  // does nothing if not playing
            if (held_note.is_playing()) {
                // see above about move constructor
                released_notes.push_back(std::move(held_note));
                //LOGI("ADD %u", released_notes.size());
            }
            held_note.start_note(data.instrument, data.pitch, state);
        }

        if (data.velocity != VELOCITY_NONE) {
            held_note.set_velocity(data.velocity);
        }
        // TODO: velocity slide
    }
}

void TrackPlayback::release_note() {
    held_note.release_note();
    if (held_note.is_playing()) {
        released_notes.push_back(std::move(held_note));
    }
}

void TrackPlayback::stop_all() {
    held_note.stop_note();
    released_notes.clear();
}

}
