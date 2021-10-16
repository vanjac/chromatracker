#include "trackplay.h"
#include <mutex>

namespace chromatracker::play {

shared_ptr<const Track> TrackPlay::track() const
{
    return _track.lock();
}

void TrackPlay::setTrack(shared_ptr<const Track> track)
{
    _track = track;
}

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
                            frames outFrameRate, float globalAmp)
{
    float lAmp = 0, rAmp = 0;
    if (auto trackP = _track.lock()) {
        std::shared_lock lock(trackP->mu);
        if (!trackP->mute) {
            globalAmp *= trackP->volume;
            lAmp = globalAmp * panningToLeftAmplitude(trackP->pan);
            rAmp = globalAmp * panningToRightAmplitude(trackP->pan);
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
