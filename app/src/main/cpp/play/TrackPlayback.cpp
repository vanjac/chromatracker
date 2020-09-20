#include "TrackPlayback.h"
#include <utility>
#include "../Log.h"

namespace chromatracker::play {

TrackPlayback::TrackPlayback(const Track *track) :
        track(track),
        current_pattern(nullptr),
        pattern_time(0),
        event_i(0) { }

void TrackPlayback::set_pattern(const Pattern *pattern, int time) {
    this->current_pattern = pattern;
    this->pattern_time = time;
    search_current_event();
}

const Pattern * TrackPlayback::get_pattern() const {
    return this->current_pattern;
}

int TrackPlayback::get_pattern_time() const {
    return this->pattern_time;
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
    if (this->current_pattern == nullptr)
        return;

    const std::vector<Event> &events = this->current_pattern->events;

    // check if event_i is invalid
    // this would happen if the events list changed while playing
    // assumes events are in order
    if (this->event_i < events.size()
            && events[this->event_i].time < this->pattern_time)
        search_current_event();
    else if (this->event_i > 0 && this->event_i <= events.size()
             && events[this->event_i - 1].time >= this->pattern_time)
        search_current_event();
    else if (this->event_i >= events.size() && !events.empty()
            && events.back().time >= this->pattern_time)
        search_current_event();

    while (this->event_i < events.size()) {
        const Event &next_event = events[this->event_i];
        if (next_event.time == this->pattern_time) {
            execute_event(next_event, state);
            this->event_i++;
        } else {
            break;
        }
    }

    this->pattern_time++;
    // loop
    if (this->pattern_time >= this->current_pattern->length) {
        this->pattern_time = 0;
        this->event_i = 0;
    }
}

void TrackPlayback::search_current_event() {
    // TODO: test!
    if (this->current_pattern == nullptr)
        return;
    if (this->pattern_time == 0) {
        event_i = 0;
        return;
    }
    const std::vector<Event> &events = this->current_pattern->events;
    if (events.empty()) {
        event_i = 0;
        return;
    }
    LOGD("Lost playback position, searching");

    // binary search
    // find the first index >= pattern_time
    int min = 0, max = events.size() - 1;
    while (min <= max) {
        int i = (min + max) / 2;
        int time = events[i].time;
        if (time > this->pattern_time) {
            min = i + 1;
        } else if (time < this->pattern_time) {
            max = i - 1;
        } else {
            this->event_i = i;
            LOGD("%d", this->event_i);
            return;
        }
    }

    // now min > max, pick the higher one
    this->event_i = min;
    LOGD("%d", this->event_i);
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
