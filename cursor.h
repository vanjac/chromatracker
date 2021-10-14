#pragma once
#include <common.h>

#include "song.h"

namespace chromatracker {

struct Cursor
{
    enum class Space
    {
        Song,
        Playback,
        SectionLoop,
    };

    Song *song;
    Section *section;
    ticks time; // in section

    Cursor();
    Cursor(Song *song);
    Cursor(Song *song, Section *section);

    bool valid() const;
    void playStep();

    // song mutex must be locked
    vector<shared_ptr<Section>>::iterator findSection() const;
    Section * nextSection() const; // could be null
    Section * prevSection() const; // could be null
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
