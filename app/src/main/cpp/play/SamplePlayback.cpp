#include "SamplePlayback.h"
#include <cstdint>
#include <cmath>
#include "../Units.h"
#include "../Log.h"

namespace chromatracker::play {

SamplePlayback::SamplePlayback(const InstSample *sample) :
playing(true),
sample(sample),
note_on(true),
backwards(false),
playback_rate(0),
playback_pos(0) {
    if (sample->playback_mode == PlaybackMode::ONCE)
        this->playback_pos = static_cast<uint64_t>(sample->loop_start) << 16u;
}

void SamplePlayback::release_note() {
    this->note_on = false;
}

void SamplePlayback::set_pitch(float pitch, int out_frame_rate) {
    if (sample == nullptr)
        return;
    pitch -= static_cast<float>(sample->base_key);
    pitch += sample->finetune;
    float note_rate = exp2f(pitch / 12.0f);
    this->playback_rate = static_cast<uint32_t>(roundf(
            note_rate * static_cast<float>(sample->wave_frame_rate)
            / static_cast<float>(out_frame_rate) * 65536.0f));
}

bool SamplePlayback::is_playing() const {
    return this->playing;
}

int SamplePlayback::get_frame_pos() const {
    return this->playback_pos >> 16;
}

bool SamplePlayback::process_tick(float *tick_buffer, int tick_frames,
                                  float l_amp, float r_amp) {
    if (!this->playing)
        return false;

    float sample_amp = volume_control_to_amplitude(sample->volume);
    l_amp *= sample_amp * panning_control_to_left_amplitude(sample->panning);
    r_amp *= sample_amp * panning_control_to_right_amplitude(sample->panning);

    int32_t rate = this->playback_rate;  // signed!
    if (this->backwards)
        rate = -rate;

    int write_frame = 0;
    while (this->playing && write_frame < tick_frames) {
        bool loop = false;  // loop back to start after this iteration

        // end of tick buffer, sample, or loop, whichever comes first
        int64_t max_pos = this->playback_pos
                + (tick_frames - write_frame) * rate;
        int64_t min_pos = INT64_MIN;
        if (this->backwards) {
            // even if sustain loop is over, need to exit forwards
            min_pos = max_pos;
            max_pos = INT64_MAX;
            int64_t start_pos = static_cast<uint64_t>(sample->loop_start) << 16u;
            if (min_pos < start_pos) {
                min_pos = start_pos;
                loop = true;
            }
        } else if (sample->playback_mode == PlaybackMode::LOOP
            || (sample->playback_mode == PlaybackMode::SUSTAIN_LOOP
                && this->note_on)) {
            // loop at loop end
            int64_t end_pos = static_cast<uint64_t>(sample->loop_end) << 16u;
            if (max_pos > end_pos) {
                max_pos = end_pos;
                loop = true;
            }
        } else if (sample->playback_mode == PlaybackMode::ONCE) {
            // stop at loop end
            int64_t end_pos = static_cast<uint64_t>(sample->loop_end) << 16u;
            if (max_pos > end_pos) {
                max_pos = end_pos;
                this->playing = false;
            }
        } else {  // SUSTAIN_LOOP + note release
            // stop at wave end
            int64_t end_pos = static_cast<uint64_t>(sample->wave_frames) << 16u;
            if (max_pos > end_pos) {
                max_pos = end_pos;
                this->playing = false;
            }
        }

        if (this->playback_pos >= max_pos || this->playback_pos <= min_pos) {
            // invalid position, quit
            this->playing = false;
            break;
        }
        while (this->playback_pos < max_pos && this->playback_pos > min_pos) {
            auto index = (this->playback_pos >> 16u) * sample->wave_channels;
            // works for stereo and mono
            float left = sample->wave[index];
            float right = sample->wave[index + sample->wave_channels - 1];
            tick_buffer[write_frame * 2] += left * l_amp;
            tick_buffer[write_frame * 2 + 1] += right * r_amp;
            this->playback_pos += rate;
            write_frame++;
        }

        if (loop) {
            if (sample->loop_type == LoopType::PING_PONG) {
                // "bounce" on edges of loop
                uint64_t bounce_frame;
                if (this->backwards) {
                    bounce_frame = sample->loop_start;
                } else {
                    bounce_frame = sample->loop_end;
                }
                // TODO off by one?
                this->playback_pos = (bounce_frame << 16u)
                        * 2 - this->playback_pos;
                this->backwards = !this->backwards;
                rate = -rate;
            } else {  // forward
                this->playback_pos -= static_cast<uint64_t>(
                        sample->loop_end - sample->loop_start) << 16u;
            }
        }
    }  // while (this->playing && write_frame < tick_frames)

    return true;
}

}