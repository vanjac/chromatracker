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
    pitchSlide = 0;
    velocitySlide = 0;
}

void TrackPlay::setSlideTarget(const Event &event, ticks time)
{
    slideTarget = event;
    if (event.pitch != Event::NO_PITCH)
        pitchSlide = ((float)event.pitch - samplePlay.pitch()) / time;
    if (event.velocity != Event::NO_VELOCITY)
        velocitySlide = (event.velocity - samplePlay.velocity()) / time;
}

void TrackPlay::processTick(float *tickBuffer, frames tickFrames,
                            frames outFrameRate, float lAmp, float rAmp)
{
    samplePlay.processTick(tickBuffer, tickFrames, outFrameRate, lAmp, rAmp);
    
    switch (_special) {
    case Event::Special::FadeOut:
        samplePlay.fadeOut();
        break;
    case Event::Special::Slide:
        {
            float pitch = samplePlay.pitch() + pitchSlide;
            if ((pitchSlide > 0 && pitch > slideTarget.pitch)
                    || (pitchSlide < 0 && pitch < slideTarget.pitch))
                pitch = slideTarget.pitch;
            samplePlay.setPitch(pitch);

            float velocity = samplePlay.velocity() + velocitySlide;
            if ((velocitySlide > 0 && velocity > slideTarget.velocity)
                    || (velocitySlide < 0 && velocity < slideTarget.velocity))
                velocity = slideTarget.velocity;
            samplePlay.setVelocity(velocity);
        }
        break;
    }
}

} // namespace
