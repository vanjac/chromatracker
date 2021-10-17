#pragma once
#include <common.h>

#include <cursor.h>
#include <song.h>
#include "trackplay.h"
#include <array>
#include <mutex>
#include <unordered_map>

namespace chromatracker::play {

struct JamEvent
{
    Event event;
    int touchId {-1};
};

class SongPlay
{
public:
    SongPlay();

    Cursor cursor();
    void setCursor(Cursor cursor);

    int currentTempo() const;

    void stop();
    void fadeAll();

    // event time is interpreted as delay
    void queueJamEvent(const JamEvent &jam);
    void processJamEvent(const JamEvent &jam);
    // return tick length
    frames processTick(float *tickBuffer, frames maxFrames,
                       frames outFrameRate);

    mutable std::mutex mu;

private:
    Cursor _cursor;
    int _tempo {125};

    vector<TrackPlay> tracks;

    vector<TrackPlay> jamTracks;
    std::unordered_map<int, int8_t> jamTouchTracks;
    vector<int> jamTrackTouches;
    std::array<JamEvent, 32> jamEvents; // circular buffer
    int jamEventI {0}, numJamEvents {0};

    framesFine tickLenError {0}; // accumulated
};

} // namespace
