#include "ModulationPlayback.h"

namespace chromatracker::play {

ADSRPlayback::ADSRPlayback() :
        adsr(nullptr), note_on(false), state_time(0), release_level(0.0f) { }

void ADSRPlayback::start_note(const ADSR *adsr) {
    this->adsr = adsr;
    this->note_on = true;
    this->state_time = 0;
}

void ADSRPlayback::release_note() {
    if (this->note_on) {
        this->release_level = current_value();
        this->note_on = false;
        this->state_time = 0;
    }
}

float ADSRPlayback::process_tick() {
    float value = current_value();
    this->state_time++;
    return value;
}

bool ADSRPlayback::is_active() const {
    if (this->adsr == nullptr) {
        return false;
    } else if (this->note_on) {
        return true;
    } else {
        return this->state_time < adsr->release;
    }
}

float ADSRPlayback::current_value() const {
    if (this->adsr == nullptr) {
        return 0.0f;
    }
    if (this->note_on) {
        if (this->state_time < adsr->attack) {
            return static_cast<float>(this->state_time)
                / static_cast<float>(adsr->attack);
        } else if (this->state_time < adsr->attack + adsr->decay) {
            return 1.0f - (static_cast<float>(this->state_time - adsr->attack)
                / static_cast<float>(adsr->decay)) * (1.0f - adsr->sustain);
        } else {
            return adsr->sustain;
        }
    } else {
        if (this->state_time < adsr->release) {
            return this->release_level
                * (1.0f - static_cast<float>(this->state_time)
                   / static_cast<float>(adsr->release));
        } else {
            return 0.0f;
        }
    }
}

}