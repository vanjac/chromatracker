#include "InstrumentPlayback.h"
#include <cmath>
#include "../Units.h"
#include "../Log.h"

namespace chromatracker::play {

InstrumentPlayback::InstrumentPlayback(std::default_random_engine *random) :
instrument(nullptr),
random(random),
velocity(1.0f),
velocity_slide(0.0f),
velocity_target(1.0f),
pitch(MIDDLE_C),
glide_target(MIDDLE_C)
{ }

void InstrumentPlayback::start_note(const Instrument *instrument,
        int init_pitch, int out_frame_rate) {
    this->instrument = instrument;

    this->pitch = static_cast<float>(init_pitch);
    this->pitch += static_cast<float>(instrument->transpose);
    this->pitch += instrument->finetune;
    this->glide_target = this->pitch;

    this->volume_adsr.start_note(&this->instrument->volume_adsr);

    this->samples.clear();
    if (instrument->sample_overlap_mode == SampleOverlapMode::RANDOM) {
        // pick a sample that matches the key
        int num_matching = 0;
        for (const auto &sample : instrument->samples) {
            if (init_pitch < sample.key_start || init_pitch > sample.key_end)
                continue;
            num_matching++;
        }
        if (num_matching == 0)
            return;
        std::uniform_int_distribution<std::default_random_engine::result_type>
                dist(0, num_matching - 1);
        int random_sample = dist(*random);
        // find the sample with index random_sample
        num_matching = 0;
        for (const auto &sample : instrument->samples) {
            if (init_pitch < sample.key_start || init_pitch > sample.key_end)
                continue;
            if (num_matching == random_sample) {
                this->samples.emplace_back(&sample);
                break;
            }
            num_matching++;
        }
    } else {  // MIX
        this->samples.reserve(instrument->samples.size());
        for (const auto &sample : instrument->samples) {
            if (init_pitch < sample.key_start || init_pitch > sample.key_end)
                continue;
            this->samples.emplace_back(&sample);
        }
    }

    update_pitch(out_frame_rate);
}

void InstrumentPlayback::release_note() {
    for (auto &sample : this->samples) {
        sample.release_note();
    }
    this->volume_adsr.release_note();
}

void InstrumentPlayback::stop_note() {
    instrument = nullptr;
    this->samples.clear();
}

void InstrumentPlayback::set_velocity(float new_velocity) {
    this->velocity = new_velocity;
    this->velocity_target = new_velocity;
}

void InstrumentPlayback::slide_velocity(float target_velocity, int ticks) {
    this->velocity_target = target_velocity;
    this->velocity_slide = (target_velocity - this->velocity)
            / static_cast<float>(ticks);
}

void InstrumentPlayback::glide(int target_pitch) {
    this->glide_target = static_cast<float>(target_pitch);
}

void InstrumentPlayback::new_note_action() {
    if (instrument == nullptr)
        return;
    switch (instrument->new_note_action) {
        case NewNoteAction::CUT:
            stop_note();
            break;
        case NewNoteAction::OFF:
            release_note();
            break;
        case NewNoteAction::CONTINUE:
            break;
    }
}

bool InstrumentPlayback::is_playing() const {
    return instrument != nullptr;
}

bool InstrumentPlayback::process_tick(float *tick_buffer, int tick_frames,
        int out_frame_rate, float amp) {
    if (instrument == nullptr)
        return false;
    amp *= volume_control_to_amplitude(this->velocity)
        * volume_control_to_amplitude(instrument->volume)
        * volume_control_to_amplitude(this->volume_adsr.process_tick());
    float l_amp = amp * panning_control_to_left_amplitude(instrument->panning);
    float r_amp = amp * panning_control_to_right_amplitude(instrument->panning);
    bool playing = false;
    for (auto &sample : this->samples) {
        if (sample.process_tick(tick_buffer, tick_frames, l_amp, r_amp))
            playing = true;
    }
    if (!playing) {
        instrument = nullptr;
        return false;
    }

    this->velocity += this->velocity_slide;
    if (this->velocity_slide > 0) {
        if (this->velocity > this->velocity_target)
            this->velocity = this->velocity_target;
    } else if (this->velocity_slide < 0) {
        if (this->velocity < this->velocity_target)
            this->velocity = this->velocity_target;
    }

    if (this->glide_target != this->pitch) {
        this->pitch += (this->glide_target - this->pitch)
                * (1 - instrument->glide);
        update_pitch(out_frame_rate);
    }

    if (!this->volume_adsr.is_active()) {
        instrument = nullptr;
        return false;
    }
    return true;
}

void InstrumentPlayback::update_pitch(int out_frame_rate) {
    for (auto &sample_play : this->samples) {
        sample_play.set_pitch(this->pitch, out_frame_rate);
    }
}

}
