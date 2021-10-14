#pragma once
#include <common.h>

#include <cursor.h>
#include <event.h>
#include <units.h>

namespace chromatracker::edit::ops {

// TODO: add undo to all of these!

void clearCell(Song *song, TrackCursor tcur, ticks size);
void writeCell(Song *song, TrackCursor tcur, ticks size, Event event);

} // namespace
