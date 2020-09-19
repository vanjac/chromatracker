#ifndef CHROMATRACKER_TRACKPLAYBACK_H
#define CHROMATRACKER_TRACKPLAYBACK_H

#include "../Pattern.h"
#include "../Song.h"
#include "InstrumentPlayback.h"
#include <vector>

namespace chromatracker::play {

class TrackPlayback {
public:
    // track can be null
    TrackPlayback(const Track *track,
            std::default_random_engine *random);
    void set_pattern(const Pattern *pattern, int time);  // null to stop pattern
    const Pattern *get_pattern() const;
    int get_pattern_time() const;
    void process_tick(float *tick_buffer, int tick_frames, int out_frame_rate,
            float amp);
    void execute_event(const Event &event, int out_frame_rate);
    void release_note();
    void stop_all();

private:
    void process_pattern_tick(int out_frame_rate);
    void search_current_event();

    const Track *const track;

    std::default_random_engine *random;

    const Pattern *current_pattern;
    int pattern_time;
    int event_i;  // always >= 0
    InstrumentPlayback held_note;
    std::vector<InstrumentPlayback> released_notes;
};

}

#endif //CHROMATRACKER_TRACKPLAYBACK_H
