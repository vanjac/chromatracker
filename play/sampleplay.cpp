#include "sampleplay.h"
#include <cmath>

namespace chromatracker::play {

const Sample * SamplePlay::sample() const
{
    return _sample;
}

void SamplePlay::setSample(const Sample *sample)
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
    if (_sample) {
        std::shared_lock lock(_sample->mu);
        _velocity -= _sample->fadeOut;
        if (_velocity < 0)
            _velocity = 0;
    }
}

void SamplePlay::processTick(float *tickBuffer, frames tickFrames,
                             frames outFrameRate, float lAmp, float rAmp)
{
    if (!_sample)
        return;
    std::shared_lock lock(_sample->mu);
    if (_sample->channels.size() == 0) {
        _sample = nullptr;
        return;
    }

    // TODO: anti-click

    float pitchOffset = _pitch - MIDDLE_C + _sample->tune;
    float noteRate = exp2f(pitchOffset / OCTAVE);
    framesFine playbackRate = (framesFine)roundf(
        noteRate * (float)_sample->frameRate / outFrameRate * 65536.0f);
    if (backwards)
        playbackRate = -playbackRate;

    float monoAmp = velocityToAmplitude(_velocity) * _sample->volume;
    lAmp *= monoAmp;
    rAmp *= monoAmp;

    frames writeFrame = 0;
    while (_sample && writeFrame < tickFrames) {
        bool collision = false; // hit loop point

        // end of tick buffer, sample, or loop, whichever comes first
        framesFine maxPos =
            playbackPos + (tickFrames - writeFrame) * playbackRate;
        framesFine minPos = INT64_MIN;
        if (backwards) {
            minPos = maxPos;
            maxPos = INT64_MAX;
            framesFine startPos = framesToFine(_sample->loopStart);
            if (minPos < startPos) {
                minPos = startPos;
                collision = true;
            }
        } else {
            int64_t endPos = framesToFine(_sample->loopEnd);
            if (maxPos > endPos) {
                maxPos = endPos;
                collision = true;
            }
        }

        if (playbackPos >= maxPos || playbackPos <= minPos) {
            // invalid position, quit (this shouldn't happen)
            // TODO getting this error sometimes (eg. clock.it, d1993.it)
            cout << ":( " << _sample->name<< " " <<playbackPos<<
                " " <<maxPos<< " " <<minPos<< "\n";
            _sample = nullptr;
            break;
        }
        bool stereo = _sample->channels.size() > 1;
        // TODO prevent leaving sample data
        while (playbackPos < maxPos && playbackPos > minPos) {
            // TODO smooth (linear) interpolation
            frames frame = fineToFrames(playbackPos);
            // works for stereo and mono
            float left = _sample->channels[0][frame];
            float right = stereo ? _sample->channels[1][frame] : left;
            tickBuffer[writeFrame * 2] += left * lAmp;
            tickBuffer[writeFrame * 2 + 1] += right * rAmp;
            playbackPos += playbackRate;
            writeFrame++;
        }

        if (collision) {
            switch (_sample->loopMode) {
            case Sample::LoopMode::Once:
                _sample = nullptr;
                break;
            case Sample::LoopMode::Forward:
                playbackPos -= framesToFine(
                    _sample->loopEnd - _sample->loopStart);
                break;
            case Sample::LoopMode::PingPong:
                // "bounce" on edges of loop
                // TODO off by one?
                framesFine bouncePoint = backwards ?
                    framesToFine(_sample->loopStart)
                    : (framesToFine(_sample->loopEnd) - 1);
                playbackPos = bouncePoint * 2 - playbackPos;
                backwards = !backwards;
                playbackRate = -playbackRate;
                break;
            }
        }
    }
}

} // namespace
