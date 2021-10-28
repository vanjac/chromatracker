#pragma once
#include <common.h>

#include "song.h"

namespace chromatracker {

struct Cursor
{
    Song *song;
    ObjWeakPtr<Section> section; // null means the cursor is in null state
    ticks time; // in section

    Cursor();
    Cursor(Song *song);
    Cursor(Song *song, shared_ptr<Section> section);

    void playStep();

    // song mutex must be locked (TODO could this be enforced?)
    vector<shared_ptr<Section>>::iterator findSection() const;

    shared_ptr<Section> nextSection() const; // could be null
    shared_ptr<Section> prevSection() const; // could be null
};

struct TrackCursor
{
    Cursor cursor;
    int track {0};

    // get track events
    // section mutex must be locked
    vector<Event> & events() const;
    // get the first event at or after the cursor
    // return end() if past the last event
    // section mutex must be locked
    vector<Event>::iterator findEvent() const;
};

} // namespace
