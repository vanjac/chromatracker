#include "sampleplay.h"
#include <cmath>

namespace chromatracker::play {

shared_ptr<const Sample> SamplePlay::sample() const
{
    return _sample.lock();
}

void SamplePlay::setSample(shared_ptr<const Sample> sample)
{
    _sample = sample;
    playbackPos = 0;
    backwards = false;
}

float SamplePlay::pitch() const
{
    return _pitch;
}

void SamplePlay::setPitch(float pitch)
{
    _pitch = pitch;
}

float SamplePlay::velocity() const
{
    return _velocity;
}

void SamplePlay::setVelocity(float velocity)
{
    _velocity = velocity;
}

void SamplePlay::fadeOut()
{
    if (auto sampleP = _sample.lock()) {
        std::shared_lock lock(sampleP->mu);
        _velocity -= sampleP->fadeOut;
        if (_velocity < 0)
            _velocity = 0;
    }
}

void SamplePlay::processTick(float *tickBuffer, frames tickFrames,
                             frames outFrameRate, float lAmp, float rAmp)
{
    auto sampleP = _sample.lock();
    if (!sampleP)
        return;
    std::shared_lock lock(sampleP->mu);
    if (sampleP->channels.size() == 0) {
        _sample.reset();
        return;
    }

    // TODO: anti-click

    float pitchOffset = _pitch - MIDDLE_C + sampleP->tune;
    float noteRate = exp2f(pitchOffset / OCTAVE);
    framesFine playbackRate = (framesFine)roundf(
        noteRate * (float)sampleP->frameRate / outFrameRate * 65536.0f);
    if (backwards)
        playbackRate = -playbackRate;

    float monoAmp = velocityToAmplitude(_velocity) * sampleP->volume;
    lAmp *= monoAmp;
    rAmp *= monoAmp;

    frames writeFrame = 0;
    while (writeFrame < tickFrames) {
        bool collision = false; // hit loop point

        // end of tick buffer, sample, or loop, whichever comes first
        framesFine maxPos =
            playbackPos + (tickFrames - writeFrame) * playbackRate;
        framesFine minPos = INT64_MIN;
        if (backwards) {
            minPos = maxPos;
            maxPos = INT64_MAX;
            framesFine startPos = framesToFine(sampleP->loopStart);
            if (minPos < startPos) {
                minPos = startPos;
                collision = true;
            }
        } else {
            int64_t endPos = framesToFine(sampleP->loopEnd);
            if (maxPos > endPos) {
                maxPos = endPos;
                collision = true;
            }
        }

        if (playbackPos >= maxPos || playbackPos <= minPos) {
            // invalid position, quit (this shouldn't happen)
            // TODO getting this error sometimes (eg. clock.it, d1993.it)
            cout << ":( " << sampleP->name<< " " <<playbackPos<<
                " " <<maxPos<< " " <<minPos<< "\n";
            _sample.reset();
            break;
        }
        bool stereo = sampleP->channels.size() > 1;
        // TODO prevent leaving sample data
        while (playbackPos < maxPos && playbackPos > minPos) {
            // TODO smooth (linear) interpolation
            frames frame = fineToFrames(playbackPos);
            // works for stereo and mono
            float left = sampleP->channels[0][frame];
            float right = stereo ? sampleP->channels[1][frame] : left;
            tickBuffer[writeFrame * 2] += left * lAmp;
            tickBuffer[writeFrame * 2 + 1] += right * rAmp;
            playbackPos += playbackRate;
            writeFrame++;
        }

        if (collision) {
            switch (sampleP->loopMode) {
            case Sample::LoopMode::Once:
                _sample.reset();
                writeFrame = tickFrames; // exit loop
                break;
            case Sample::LoopMode::Forward:
                playbackPos -= framesToFine(
                    sampleP->loopEnd - sampleP->loopStart);
                break;
            case Sample::LoopMode::PingPong:
                // "bounce" on edges of loop
                // TODO off by one?
                framesFine bouncePoint = backwards ?
                    framesToFine(sampleP->loopStart)
                    : (framesToFine(sampleP->loopEnd) - 1);
                playbackPos = bouncePoint * 2 - playbackPos;
                backwards = !backwards;
                playbackRate = -playbackRate;
                break;
            }
        }
    }
}

} // namespace
