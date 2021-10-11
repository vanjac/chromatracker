#include "trackplay.h"
#include <mutex>

namespace chromatracker::play {

const Track * TrackPlay::track() const
{
    return _track;
}

void TrackPlay::setTrack(const Track *track)
{
    _track = track;
}

const Sample * TrackPlay::currentSample() const
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
    if (event.sample)
        samplePlay.setSample(event.sample); // TODO new note action
    if (event.pitch != Event::NO_PITCH)
        samplePlay.setPitch(event.pitch);
    if (event.velocity != Event::NO_VELOCITY)
        samplePlay.setVelocity(event.velocity);
    _special = event.special;
}

void TrackPlay::processTick(float *tickBuffer, frames tickFrames,
                            frames outFrameRate, float globalAmp)
{
    float lAmp = 0, rAmp = 0;
    if (_track) {
        std::shared_lock lock(_track->mu);
        if (!_track->mute) {
            globalAmp *= _track->volume;
            lAmp = globalAmp * panningToLeftAmplitude(_track->pan);
            rAmp = globalAmp * panningToRightAmplitude(_track->pan);
        }
    } else {
        lAmp = rAmp = globalAmp; // jam track
    }

    samplePlay.processTick(tickBuffer, tickFrames, outFrameRate, lAmp, rAmp);
    
    switch (_special) {
    case Event::Special::FadeOut:
        samplePlay.fadeOut();
        break;
    // TODO slide
    }
}

} // namespace
