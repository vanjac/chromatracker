#ifndef CHROMATRACKER_MODULATIONPLAYBACK_H
#define CHROMATRACKER_MODULATIONPLAYBACK_H

#include "../Modulation.h"

namespace chromatracker::play {

class ADSRPlayback {
public:
    ADSRPlayback();
    void start_note(const ADSR *adsr);
    void release_note();
    float process_tick();
    bool is_active() const;
    float current_value() const;
private:
    const ADSR *adsr;
    bool note_on;
    int state_time;  // attack or release time
    float release_level;
};

}

#endif //CHROMATRACKER_MODULATIONPLAYBACK_H
