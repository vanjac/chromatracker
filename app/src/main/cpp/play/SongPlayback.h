#ifndef CHROMATRACKER_SONGPLAYBACK_H
#define CHROMATRACKER_SONGPLAYBACK_H

#include <list>
#include "../Song.h"
#include "State.h"
#include "TrackPlayback.h"

namespace chromatracker::play {

class SongPlayback {
public:
    SongPlayback(const Song * song, int out_frame_rate);
    void play_from_beginning();
    void play_from(std::list<Page>::const_iterator itr, int time);
    void stop_playback();
    void stop_all_notes();
    std::list<Page>::const_iterator get_page() const;
    int get_page_time() const;
    void jam_event(const Event &event);
    // return number of frames written
    int process_tick(float *tick_buffer, int max_frames);

private:
    void update_page();

    const Song *const song;

    SongState state;

    unsigned tick_len;  // fp 16.16 frame length
    unsigned tick_len_error;  // fp 16.16 accumulated error in tick length

    std::list<Page>::const_iterator page_itr;  // end() if not playing
    int page_time;

    std::vector<TrackPlayback> tracks;
    TrackPlayback jam_track;
};

}

#endif //CHROMATRACKER_SONGPLAYBACK_H
