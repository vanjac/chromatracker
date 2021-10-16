#pragma once
#include <common.h>

#include <cursor.h>
#include <event.h>
#include "operation.h"
#include <units.h>

namespace chromatracker::edit::ops {

class ClearCell : public SongOp
{
public:
    ClearCell(TrackCursor tcur, ticks size);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    const TrackCursor tcur;
    const ticks size;
    const shared_ptr<Section> section;
private:
    vector<Event> clearedEvents;
};

class WriteCell : public ClearCell
{
public:
    WriteCell(TrackCursor tcur, ticks size, Event event);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    Event event;
};

class DeleteSection : public SongOp
{
public:
    DeleteSection(shared_ptr<Section> section);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    const shared_ptr<Section> section;
private:
    int index {-1};
    vector<shared_ptr<Section>> prevLinks;
};

} // namespace
