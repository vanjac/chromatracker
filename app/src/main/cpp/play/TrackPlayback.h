#ifndef CHROMATRACKER_TRACKPLAYBACK_H
#define CHROMATRACKER_TRACKPLAYBACK_H

#include <vector>
#include "../Pattern.h"
#include "../Song.h"
#include "State.h"
#include "InstrumentPlayback.h"

namespace chromatracker::play {

class TrackPlayback {
public:
    // track can be null
    TrackPlayback(const Track *track);
    void set_pattern(const Pattern *pattern, int time);  // null to stop pattern
    const Pattern *get_pattern() const;
    int get_pattern_time() const;
    void process_tick(float *tick_buffer, int tick_frames, SongState *state,
            float amp);
    void execute_event(const Event &event, SongState *state);
    void release_note();
    void stop_all();

private:
    void process_pattern_tick(SongState *state);
    void search_current_event();

    const Track *const track;

    const Pattern *current_pattern;
    int pattern_time;
    int event_i;  // always >= 0
    InstrumentPlayback held_note;
    std::vector<InstrumentPlayback> released_notes;
};

}

#endif //CHROMATRACKER_TRACKPLAYBACK_H
