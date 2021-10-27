#include "trackplay.h"
#include <mutex>

namespace chromatracker::play {

shared_ptr<const Sample> TrackPlay::currentSample() const
{
    return samplePlay.sample();
}

Event::Special TrackPlay::currentSpecial() const
{
    return _special;
}

void TrackPlay::stop()
{
    samplePlay.setSample(nullptr);
    _special = Event::Special::None;
}

void TrackPlay::processEvent(const Event &event)
{
    if (auto sampleP = event.sample.lock())
        samplePlay.setSample(sampleP); // TODO new note action
    if (event.pitch != Event::NO_PITCH)
        samplePlay.setPitch(event.pitch);
    if (event.velocity != Event::NO_VELOCITY)
        samplePlay.setVelocity(event.velocity);
    _special = event.special;
}

void TrackPlay::processTick(float *tickBuffer, frames tickFrames,
                            frames outFrameRate, float lAmp, float rAmp)
{
    samplePlay.processTick(tickBuffer, tickFrames, outFrameRate, lAmp, rAmp);
    
    switch (_special) {
    case Event::Special::FadeOut:
        samplePlay.fadeOut();
        break;
    // TODO slide
    }
}

} // namespace
