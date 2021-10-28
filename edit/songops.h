#pragma once
#include <common.h>

#include "operation.h"
#include <cursor.h>
#include <event.h>
#include <units.h>

namespace chromatracker::edit::ops {

class SetSongVolume : public SongOp
{
public:
    SetSongVolume(float volume);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    float volume;
};

class SetTrackMute : public SongOp
{
public:
    SetTrackMute(int track, bool mute);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    const int track;
    bool mute;
};

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

class AddSection : public SongOp
{
public:
    AddSection(int index, shared_ptr<Section> section);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    const int index;
    const shared_ptr<Section> section;
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
