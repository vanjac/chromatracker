#pragma once
#include <common.h>

#include "sampleplay.h"
#include <song.h>

namespace chromatracker::play {

class TrackPlay
{
    static const int POLYPHONY = 8;

public:
    shared_ptr<const Sample> currentSample() const;
    Event::Special currentSpecial() const;

    void stop();
    void processEvent(const Event &event);
    // call after processEvent
    void setSlideTarget(const Event &event, ticks time);

    void processTick(float *tickBuffer, frames tickFrames,
                     frames outFrameRate, float lAmp, float rAmp);

private:
    SamplePlay samplePlay;
    Event::Special _special {Event::Special::None};

    Event slideTarget;
    float pitchSlide, velocitySlide;
};

} // namespace
