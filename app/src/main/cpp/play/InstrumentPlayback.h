#ifndef CHROMATRACKER_INSTRUMENTPLAYBACK_H
#define CHROMATRACKER_INSTRUMENTPLAYBACK_H

#include <vector>
#include <random>
#include "../Util.h"
#include "../Instrument.h"
#include "SamplePlayback.h"
#include "ModulationPlayback.h"

namespace chromatracker::play {

// really a "note playback"
// reusable for multiple notes/instruments
class InstrumentPlayback : public noncopyable {
public:
    InstrumentPlayback(std::default_random_engine *random);

    void start_note(const Instrument *instrument,
            int init_pitch, int out_frame_rate);
    void release_note();
    void stop_note();
    void set_velocity(float new_velocity);
    void slide_velocity(float target_velocity, int ticks);
    void glide(int target_pitch);
    void new_note_action();
    bool is_playing() const;
    // tick_buffer has OUT_CHANNELS channels
    // return false if no data was written because instrument is not playing
    bool process_tick(float *tick_buffer, int tick_frames, int out_frame_rate,
            float amp);

private:
    void update_pitch(int out_frame_rate);

    std::default_random_engine *random;

    const Instrument *instrument;  // null when not playing, careful!
    std::vector<SamplePlayback> samples;  // TODO: use global pool?

    ADSRPlayback volume_adsr;

    float velocity;
    float velocity_slide;  // signed
    float velocity_target;

    float pitch;
    float glide_target;
};

}

#endif //CHROMATRACKER_INSTRUMENTPLAYBACK_H
