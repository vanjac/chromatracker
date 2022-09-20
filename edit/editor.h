#pragma once
#include <common.h>

#include "undoer.hpp"
#include <cursor.h>
#include <song.h>

namespace chromatracker::edit {

struct Editor
{
    Song song;

    TrackCursor editCur;
    ticks cellSize {TICKS_PER_BEAT / 4};

    Event selected {0, {}, MIDDLE_C, 1.0f, Event::Special::None};

    // mode
    bool record {true};
    bool overwrite {true};
    bool followPlayback {true};

    edit::Undoer<Song *> undoer;

    void reset(); // call when new song created

    void resetCursor();
    void snapToGrid();
    void nextCell();
    void prevCell();

    void select(const Event &event);
    int selectedSampleIndex(); // song must be locked

    // return if playing
    void writeEvent(bool playing, const Event &event, Event::Mask mask,
                    bool continuous=false);
};

} // namespace
