#include "cursor.h"

namespace chromatracker::edit::ops {

void clearCell(Song *song, TrackCursor tcur, ticks size)
{
    if (!tcur.cursor.valid())
        return;
    std::unique_lock lock(tcur.cursor.section->mu);
    auto startIt = tcur.findEvent();
    tcur.cursor.time += size; // don't use move() -- keep in same section
    auto endIt = tcur.findEvent();
    tcur.events().erase(startIt, endIt);
}

void writeCell(Song *song, TrackCursor tcur, ticks size, Event event)
{
    if (!tcur.cursor.valid())
        return;
    clearCell(song, tcur, size);
    event.time = tcur.cursor.time;
    std::unique_lock lock(tcur.cursor.section->mu);
    auto insertIt = tcur.findEvent();
    tcur.events().insert(insertIt, event);
}

} // namespace
