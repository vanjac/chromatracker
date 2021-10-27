#pragma once
#include <common.h>

#include "jam.h"
#include "trackplay.h"
#include <cursor.h>
#include <song.h>
#include <mutex>

namespace chromatracker::play {

class SongPlay
{
public:
    Cursor cursor();
    void setCursor(Cursor cursor);

    int currentTempo() const;

    void stop();
    void fadeAll();

    // return tick length
    frames processTick(float *tickBuffer, frames maxFrames,
                       frames outFrameRate);

    mutable std::mutex mu;

    Jam jam;

private:
    Cursor _cursor;
    int _tempo {125};

    vector<TrackPlay> tracks;

    framesFine tickLenError {0}; // accumulated
};

} // namespace
