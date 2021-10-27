#include "songplay.h"

namespace chromatracker::play {

Cursor SongPlay::cursor()
{
    return _cursor;
}

void SongPlay::setCursor(Cursor cursor)
{
    if (cursor.song != _cursor.song) {
        tracks.clear();
    }
    _cursor = cursor;
}

int SongPlay::currentTempo() const
{
    return _tempo;
}

void SongPlay::stop()
{
    _cursor.section.reset();
    for (auto &track : tracks) {
        track.stop();
    }
    jam.stop();
}

void SongPlay::fadeAll()
{
    _cursor.section.reset();
    Event fadeEvent;
    fadeEvent.special = Event::Special::FadeOut;
    for (auto &track : tracks) {
        track.processEvent(fadeEvent);
    }
}

frames SongPlay::processTick(float *tickBuffer, frames maxFrames,
                             frames outFrameRate)
{
    Song *song = _cursor.song;
    if (!song)
        return 0;

    framesFine tickLen = framesToFine(outFrameRate) * 60l
        / (framesFine)_tempo / (framesFine)TICKS_PER_BEAT;

    int tickFrames = fineToFrames(tickLen);
    tickLenError += tickLen & 0xFFFF;
    while (tickLenError >= framesToFine(1)) {
        tickLenError -= framesToFine(1);
        tickFrames++;
    }
    if (tickFrames > maxFrames)
        tickFrames = maxFrames;

    for (int i = 0; i < tickFrames * 2; i++) {
        tickBuffer[i] = 0;
    }

    float amplitude;
    {
        // hold the song for the whole block to ensure num tracks doesn't change
        std::shared_lock songLock(song->mu);
        amplitude = song->volume;

        if (tracks.size() != song->tracks.size()) {
            tracks.resize(song->tracks.size());
            for (int i = 0; i < song->tracks.size(); i++) {
                tracks[i].stop();
            }
        }

        if (auto sectionP = _cursor.section.lock()) {
            std::shared_lock sectionLock(sectionP->mu);
            // process events
            TrackCursor tcur{_cursor};
            for (tcur.track; tcur.track < tracks.size(); tcur.track++) {
                auto eventIt = tcur.findEvent();
                if (eventIt != tcur.events().end()
                        && eventIt->time == _cursor.time)
                    tracks[tcur.track].processEvent(*eventIt);
            }
            if (sectionP->tempo != Section::NO_TEMPO) {
                _tempo = sectionP->tempo;
            }
        }

        for (int i = 0; i < song->tracks.size(); i++) {
            float lAmp = 0, rAmp = 0;
            {
                auto track = song->tracks[i];
                std::shared_lock lock(track->mu);
                if (!track->mute) {
                    float tAmp = amplitude * track->volume;
                    lAmp = tAmp * panningToLeftAmplitude(track->pan);
                    rAmp = tAmp * panningToRightAmplitude(track->pan);
                }
            }
            tracks[i].processTick(tickBuffer, tickFrames,
                                  outFrameRate, lAmp, rAmp);
        }
    }
    jam.processTick(tickBuffer, tickFrames, outFrameRate, amplitude);

    _cursor.playStep();
    if (!_cursor.section.lock()) { // section may have been cleared after move
        fadeAll();
    }

    return tickFrames;
}

} // namespace
