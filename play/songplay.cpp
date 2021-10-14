#include "songplay.h"

namespace chromatracker::play {

const int NUM_JAM_TRACKS = 8;

SongPlay::SongPlay()
{
    jamTracks.resize(NUM_JAM_TRACKS);
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
    _cursor.section = nullptr;
    for (auto &track : tracks) {
        track.stop();
    }
    for (auto &track : jamTracks) {
        track.stop();
    }
    jamTouches.clear();
}

void SongPlay::fadeAll()
{
    _cursor.section = nullptr;
    Event fadeEvent;
    fadeEvent.special = Event::Special::FadeOut;
    for (auto &track : tracks) {
        track.processEvent(fadeEvent);
    }
    for (auto &track : jamTracks) {
        track.processEvent(fadeEvent);
    }
    jamTouches.clear();
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
    if (jamTouches.count(jam.touchId)) {
        trackIndex = jamTouches[jam.touchId];
    } else {
        trackIndex = 0; // TODO cycle, check overwrite
        for (int i = 0; i < jamTracks.size(); i++) {
            if (!jamTracks[i].currentSample()
                || jamTracks[i].currentSpecial() == Event::Special::FadeOut) {
                trackIndex = i;
                break;
            }
        }
        jamTouches[jam.touchId] = trackIndex;
    }
    jamTracks[trackIndex].processEvent(jam.event);
    if (jam.event.special == Event::Special::FadeOut) {
        jamTouches.erase(jam.touchId);
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
                tracks[i].setTrack(song->tracks[i].get());
                tracks[i].stop();
            }
        }

        if (_cursor.valid()) {
            Section *section = _cursor.section;
            std::shared_lock sectionLock(section->mu);
            // process events
            TrackCursor tcur{_cursor};
            for (tcur.track; tcur.track < tracks.size(); tcur.track++) {
                auto eventIt = tcur.findEvent();
                if (eventIt != tcur.events().end()
                        && eventIt->time == _cursor.time)
                    tracks[tcur.track].processEvent(*eventIt);
            }
            if (section->tempo != Section::NO_TEMPO) {
                _tempo = section->tempo;
            }
        }
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

    _cursor.move(1, Cursor::Space::Playback);
    return tickFrames;
}

} // namespace
