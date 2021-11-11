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

class AddTrack : public SongOp
{
public:
    AddTrack(int index, shared_ptr<Track> track);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    int index;
    shared_ptr<Track> track;
};

class DeleteTrack : public SongOp
{
public:
    DeleteTrack(shared_ptr<Track> track);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    shared_ptr<Track> track;
private:
    int index {-1};
    vector<vector<Event>> clearedEvents;
};

class SetTrackMute : public SongOp
{
public:
    SetTrackMute(shared_ptr<Track> track, bool mute);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    shared_ptr<Track> track;
    bool mute;
};

class SetTrackSolo : public SongOp
{
public:
    SetTrackSolo(shared_ptr<Track> track, bool solo);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    shared_ptr<Track> track;
    bool solo;
    vector<bool> trackMute;
};

class ClearCell : public SongOp
{
public:
    ClearCell(TrackCursor tcur, ticks size);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    TrackCursor tcur;
    ticks size;
    shared_ptr<Section> section;
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

class MergeEvent : public SongOp
{
public:
    MergeEvent(TrackCursor tcur, Event event, Event::Mask mask);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    TrackCursor tcur;
    Event event;
    Event::Mask mask;
    shared_ptr<Section> section;
protected:
    Event prevEvent;
};

class AddSection : public SongOp
{
public:
    AddSection(int index, shared_ptr<Section> section);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    int index;
    shared_ptr<Section> section;
};

class DeleteSection : public SongOp
{
public:
    DeleteSection(shared_ptr<Section> section);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    shared_ptr<Section> section;
private:
    int index {-1};
    vector<shared_ptr<Section>> prevLinks;
};

class SliceSection : public SongOp
{
public:
    SliceSection(shared_ptr<Section> section, ticks pos);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    shared_ptr<Section> section;
    ticks pos;
};

class AddSample : public SongOp
{
public:
    AddSample(int index, shared_ptr<Sample> sample);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    int index;
    shared_ptr<Sample> sample;
};

class DeleteSample : public SongOp
{
public:
    DeleteSample(shared_ptr<Sample> sample);
    bool doIt(Song *song) override;
    void undoIt(Song *song) override;
protected:
    shared_ptr<Sample> sample;
private:
    int index {-1};
    vector<EventRef> sampleEvents;
};

} // namespace
