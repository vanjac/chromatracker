#ifndef CHROMATRACKER_SAMPLEPLAYBACK_H
#define CHROMATRACKER_SAMPLEPLAYBACK_H

#include "../InstSample.h"
#include "State.h"

namespace chromatracker::play {

class SamplePlayback {
public:
    SamplePlayback(const InstSample *sample);

    void release_note();
    // call after start_note
    void set_pitch(float pitch, SongState *state);
    bool is_playing() const;
    int get_frame_pos() const;
    // tick_buffer has OUT_CHANNELS channels
    // return false if no data was written because sample is not playing
    bool process_tick(float *tick_buffer, int tick_frames,
        float l_amp, float r_amp);

private:
    bool playing;  // false: don't process this
    const InstSample *const sample;
    bool note_on;  // false: note release
    bool backwards;  // ping-pong mode

    uint32_t playback_rate;  // fp 16.16 frame rate, calculated from pitch
    int64_t playback_pos;  // fp 32.16 frame num
};

}

#endif //CHROMATRACKER_SAMPLEPLAYBACK_H
