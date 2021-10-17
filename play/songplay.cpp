#include "songplay.h"
#include <algorithm>

namespace chromatracker::play {

const int NUM_JAM_TRACKS = 8;

SongPlay::SongPlay()
{
    jamTracks.resize(NUM_JAM_TRACKS);
    jamTrackTouches.resize(NUM_JAM_TRACKS);
    std::fill(jamTrackTouches.begin(), jamTrackTouches.end(), -1);
}

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
    for (auto &track : jamTracks) {
        track.stop();
    }
    jamTouchTracks.clear();
    std::fill(jamTrackTouches.begin(), jamTrackTouches.end(), -1);
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

void SongPlay::queueJamEvent(const JamEvent &jam)
{
    jamEvents[(jamEventI + numJamEvents) % jamEvents.size()] = jam;
    if (numJamEvents == jamEvents.size()) {
        jamEventI++;
    } else {
        numJamEvents++;
    }
}

void SongPlay::processJamEvent(const JamEvent &jam)
{
    int trackIndex;
    if (jamTouchTracks.count(jam.touchId)) {
        trackIndex = jamTouchTracks[jam.touchId];
    } else {
        auto it = std::find(jamTrackTouches.begin(), jamTrackTouches.end(), -1);
        if (it == jamTrackTouches.end())
            return; // limit number of touches at once
        trackIndex = it - jamTrackTouches.begin();
        jamTouchTracks[jam.touchId] = trackIndex;
        jamTrackTouches[trackIndex] = jam.touchId;
        //cout << "assign " <<jam.touchId<< " to track " <<trackIndex<< "\n";
    }
    jamTracks[trackIndex].processEvent(jam.event);
    if (jam.event.special == Event::Special::FadeOut) {
        jamTouchTracks.erase(jam.touchId);
        jamTrackTouches[trackIndex] = -1;
    }
}

frames SongPlay::processTick(float *tickBuffer, frames maxFrames,
                             frames outFrameRate)
{
    Song *song = _cursor.song;
    if (!song)
        return 0;

    float amplitude;
    {
        // hold the song for the whole block to ensure num tracks doesn't change
        std::shared_lock songLock(song->mu);
        amplitude = song->volume;

        if (tracks.size() != song->tracks.size()) {
            tracks.resize(song->tracks.size());
            for (int i = 0; i < song->tracks.size(); i++) {
                tracks[i].setTrack(song->tracks[i]);
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
    }
    _cursor.playStep();
    if (!_cursor.section.lock()) { // section may have been cleared after move
        fadeAll();
    }

    for (int i = 0; i < numJamEvents; i++) {
        JamEvent *jam = &jamEvents[(i + jamEventI) % jamEvents.size()];
        if (jam->event.time == 0) {
            processJamEvent(*jam);
            jamEventI++;
            numJamEvents--;
            i--;
        } else {
            jam->event.time -= 1;
        }
    }

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
    for (auto &track : tracks) {
        track.processTick(tickBuffer, tickFrames, outFrameRate, amplitude);
    }
    for (auto &track : jamTracks) {
        track.processTick(tickBuffer, tickFrames, outFrameRate, amplitude);
    }

    return tickFrames;
}

} // namespace
