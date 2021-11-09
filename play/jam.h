#pragma once
#include <common.h>

#include "trackplay.h"
#include <event.h>
#include <array>
#include <unordered_map>

namespace chromatracker::play {

struct JamEvent
{
    Event event;
    int touchId {0}; // 0 should not be used
};

class Jam
{
public:
    Jam();

    void stop();
    // event time is interpreted as delay
    void queueJamEvent(const JamEvent &jam);
    void processJamEvent(const JamEvent &jam);
    void processTick(float *tickBuffer, frames tickFrames,
                     frames outFrameRate, float globalAmp);

private:
    std::array<JamEvent, 32> jamEvents; // circular buffer
    int jamEventI {0}, numJamEvents {0};

    vector<TrackPlay> jamTracks;

    std::unordered_map<int, int8_t> jamTouchTracks;
    vector<int> jamTrackTouches;
};

} // namespace
