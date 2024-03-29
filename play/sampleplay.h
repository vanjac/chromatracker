#pragma once
#include <common.h>

#include "playunits.h"
#include <sample.h>

namespace chromatracker::play {

class SamplePlay
{
public:
    shared_ptr<const Sample> sample() const;
    void setSample(shared_ptr<const Sample> sample);
    float pitch() const;
    void setPitch(float pitch); // note pitch
    float velocity() const;
    void setVelocity(float velocity); // note velocity
    
    // special effects (call every tick)
    void fadeOut();

    void processTick(float *tickBuffer, frames tickFrames,
                     frames outFrameRate, float lAmp, float rAmp);

private:
    ObjWeakPtr<const Sample> _sample; // null for no sample
    float _pitch {MIDDLE_C};
    float _velocity {1.0f};

    framesFine playbackPos;
    bool backwards {false}; // ping-pong
};

} // namespace
