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

class SetTrackVolume : public SetObjectValue<Track, float>
{
public:
    SetTrackVolume(shared_ptr<Track> track, float volume);
private:
    float & objectValue() override;
};

class SetTrackPan : public SetObjectValue<Track, float>
{
public:
    SetTrackPan(shared_ptr<Track> track, float pan);
private:
    float & objectValue() override;
};

class SetTrackMute : public SetObjectValue<Track, bool>
{
public:
    SetTrackMute(shared_ptr<Track> track, bool mute);
private:
    bool & objectValue() override;
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

class SetSectionTempo : public SetObjectValue<Section, int>
{
public:
    SetSectionTempo(shared_ptr<Section> section, int tempo);
private:
    int & objectValue() override;
};

class SetSectionMeter : public SetObjectValue<Section, int>
{
public:
    SetSectionMeter(shared_ptr<Section> section, int meter);
private:
    int & objectValue() override;
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

class SetSampleVolume : public SetObjectValue<Sample, float>
{
public:
    SetSampleVolume(shared_ptr<Sample> sample, float volume);
private:
    float & objectValue() override;
};

class SetSampleTune : public SetObjectValue<Sample, float>
{
public:
    SetSampleTune(shared_ptr<Sample> sample, float tune);
private:
    float & objectValue() override;
};

class SetSampleFadeOut : public SetObjectValue<Sample, float>
{
public:
    SetSampleFadeOut(shared_ptr<Sample> sample, float fadeOut);
private:
    float & objectValue() override;
};

} // namespace
