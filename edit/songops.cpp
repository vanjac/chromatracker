#include "songops.h"
#include <algorithm>

namespace chromatracker::edit::ops {

/*
Order is important!
For deleting an object:
- Lock song
- Remove all links to the object
- Remove from songobj list
- Set deleted flag

If the deleted flag is set too early, links could be broken if they are accessed
elsewhere.

For adding/undeleting an object:
- Lock song
- Clear deleted flag
- Insert to songobj list
- Restore all links to the object
*/

SetSongVolume::SetSongVolume(float volume)
    : volume(volume)
{}

bool SetSongVolume::doIt(Song *song)
{
    std::unique_lock lock(song->mu);
    std::swap(volume, song->volume);
    return volume != song->volume;
}

void SetSongVolume::undoIt(Song *song)
{
    doIt(song);
}

AddTrack::AddTrack(int index, shared_ptr<Track> track)
    : index(index)
    , track(track)
{}

bool AddTrack::doIt(Song *song)
{
    std::unique_lock songLock(song->mu);
    track->deleted = false;
    song->tracks.insert(song->tracks.begin() + index, track);

    for (auto &section : song->sections) {
        std::unique_lock sectionLock(section->mu);
        section->trackEvents.insert(section->trackEvents.begin() + index,
                                    vector<Event>());
    }

    return true;
}

void AddTrack::undoIt(Song *song)
{
    std::unique_lock songLock(song->mu);

    for (auto &section : song->sections) {
        std::unique_lock sectionLock(section->mu);
        section->trackEvents.erase(section->trackEvents.begin() + index);
    }

    song->tracks.erase(song->tracks.begin() + index);
    track->deleted = true;
}

DeleteTrack::DeleteTrack(shared_ptr<Track> track)
    : track(track)
{}

bool DeleteTrack::doIt(Song *song)
{
    std::unique_lock songLock(song->mu);
    if (song->tracks.size() <= 1) {
        index = -1;
        return false; // don't delete the last track
    }

    auto it = std::find(song->tracks.begin(), song->tracks.end(), track);
    index = it - song->tracks.begin();

    clearedEvents.reserve(song->sections.size());
    for (auto &section : song->sections) {
        std::unique_lock sectionLock(section->mu);
        clearedEvents.push_back(section->trackEvents[index]);
        section->trackEvents.erase(section->trackEvents.begin() + index);
    }

    song->tracks.erase(song->tracks.begin() + index);
    track->deleted = true;
    return true;
}

void DeleteTrack::undoIt(Song *song)
{
    if (index < 0)
        return;
    std::unique_lock songLock(song->mu);
    track->deleted = false;
    song->tracks.insert(song->tracks.begin() + index, track);

    for (int i = 0; i < song->sections.size(); i++) {
        auto &section = song->sections[i];
        std::unique_lock sectionLock(section->mu);
        section->trackEvents.insert(section->trackEvents.begin() + index,
                                    clearedEvents[i]);
    }
    clearedEvents.clear();
}

SetTrackMute::SetTrackMute(int track, bool mute)
    : track(track)
    , mute(mute)
{}

bool SetTrackMute::doIt(Song *song)
{
    shared_ptr<Track> t;
    {
        std::shared_lock songLock(song->mu);
        t = song->tracks[track];
    }
    std::unique_lock trackLock(t->mu);
    std::swap(mute, t->mute);
    return mute != t->mute;
}

void SetTrackMute::undoIt(Song *song)
{
    doIt(song);
}

SetTrackSolo::SetTrackSolo(int track, bool solo)
    : track(track)
    , solo(solo)
{}

bool SetTrackSolo::doIt(Song *song)
{
    std::shared_lock songLock(song->mu);
    trackMute.resize(song->tracks.size());
    for (int i = 0; i < song->tracks.size(); i++) {
        auto &t = song->tracks[i];
        std::unique_lock trackLock(t->mu);
        trackMute[i] = t->mute;
        t->mute = (solo && i != track);
    }
    return true;
}

void SetTrackSolo::undoIt(Song *song)
{
    std::shared_lock songLock(song->mu);
    for (int i = 0; i < trackMute.size(); i++) {
        auto &t = song->tracks[i];
        std::unique_lock trackLock(t->mu);
        t->mute = trackMute[i];
    }
    trackMute.clear();
}

ClearCell::ClearCell(TrackCursor tcur, ticks size)
    : tcur(tcur)
    , size(size)
    , section(tcur.cursor.section.lockDeleted())
{}

bool ClearCell::doIt(Song *song)
{
    std::unique_lock lock(section->mu);
    TrackCursor endCur = tcur;
    endCur.cursor.time += size;
    auto startIt = tcur.findEvent();
    auto endIt = endCur.findEvent();
    clearedEvents = vector<Event>(startIt, endIt);
    tcur.events().erase(startIt, endIt);
    return !clearedEvents.empty();
}

void ClearCell::undoIt(Song *song)
{
    std::unique_lock lock(section->mu);
    auto insertIt = tcur.findEvent();
    tcur.events().insert(insertIt, clearedEvents.begin(), clearedEvents.end());
    clearedEvents.clear();
}

WriteCell::WriteCell(TrackCursor tcur, ticks size, Event event)
    : event(event)
    , ClearCell(tcur, size)
{
    this->event.time = tcur.cursor.time;
}

bool WriteCell::doIt(Song *song)
{
    ClearCell::doIt(song);
    std::unique_lock lock(section->mu);
    auto insertIt = tcur.findEvent();
    tcur.events().insert(insertIt, event);
    return true;
}

void WriteCell::undoIt(Song *song)
{
    {
        std::unique_lock lock(section->mu);
        auto it = tcur.findEvent();
        tcur.events().erase(it);
    }
    ClearCell::undoIt(song);
}

MergeEvent::MergeEvent(TrackCursor tcur, Event event, Event::Mask mask)
    : tcur(tcur)
    , mask(mask)
    , section(tcur.cursor.section.lockDeleted())
{
    this->event = event.masked(mask);
    this->event.time = tcur.cursor.time;
}

bool MergeEvent::doIt(Song *song)
{
    std::unique_lock lock(section->mu);
    auto it = tcur.findEvent();
    if (it == tcur.events().end() || it->time != event.time) {
        if (event.empty())
            return false;
        tcur.events().insert(it, event);
    } else if (it->time == event.time) {
        prevEvent = *it;
        it->merge(event, mask);
        if (it->empty()) {
            tcur.events().erase(it);
        }
    }
    return true;
}

void MergeEvent::undoIt(Song *song)
{
    std::unique_lock lock(section->mu);
    auto it = tcur.findEvent();
    if (it == tcur.events().end() || it->time != event.time) {
        if (!prevEvent.empty())
            tcur.events().insert(it, prevEvent);
    } else if (!prevEvent.empty()) {
        *it = prevEvent;
    } else {
        tcur.events().erase(it);
    }
    prevEvent = Event();
}

AddSection::AddSection(int index, shared_ptr<Section> section)
    : index(index)
    , section(section)
{}

bool AddSection::doIt(Song *song)
{
    std::unique_lock songLock(song->mu);
    section->deleted = false;
    song->sections.insert(song->sections.begin() + index, section);

    std::unique_lock sectionLock(section->mu);
    if (index != song->sections.size() - 1) {
        section->next = song->sections[index + 1];
    }
    if (index != 0) {
        if (song->sections[index - 1]->next.lockDeleted() ==
                section->next.lockDeleted())
            song->sections[index - 1]->next = section;
    }

    return true;
}

void AddSection::undoIt(Song *song)
{
    std::unique_lock songLock(song->mu);

    if (index != 0) {
        if (song->sections[index - 1]->next.lockDeleted() == section) {
            song->sections[index - 1]->next = section->next;
        }
    }

    song->sections.erase(song->sections.begin() + index);
    section->deleted = true;
}

DeleteSection::DeleteSection(shared_ptr<Section> section)
    : section(section)
{}

bool DeleteSection::doIt(Song *song)
{
    std::unique_lock songLock(song->mu);
    if (song->sections.size() <= 1) {
        index = -1;
        return false; // don't delete the last section
    }

    // fix sections which point to the deleted section
    for (int i = 0; i < song->sections.size(); i++) {
        auto &other = song->sections[i];
        std::unique_lock otherLock(other->mu);
        if (other->next.lockDeleted() == section) {
            prevLinks.push_back(other);
            other->next = section->next;
        }
    }

    auto it = std::find(song->sections.begin(), song->sections.end(), section);
    index = it - song->sections.begin();
    song->sections.erase(it);
    section->deleted = true;
    return true;
}

void DeleteSection::undoIt(Song *song)
{
    if (index < 0)
        return;
    std::unique_lock songLock(song->mu);
    section->deleted = false;
    song->sections.insert(song->sections.begin() + index, section);

    for (auto &link : prevLinks) {
        std::unique_lock linkLock(link->mu);
        link->next = section;
    }
    prevLinks.clear();
}

SliceSection::SliceSection(shared_ptr<Section> section, ticks pos)
    : section(section)
    , pos(pos)
{}

bool SliceSection::doIt(Song *song)
{
    shared_ptr<Section> secondHalf(new Section);

    std::unique_lock sectionLock(section->mu);
    secondHalf->length = section->length - pos;
    section->length = pos;
    secondHalf->next = section->next;
    section->next = secondHalf;

    int numTracks = section->trackEvents.size();
    secondHalf->trackEvents.reserve(numTracks);

    TrackCursor tcur {Cursor(song, section, pos)};
    for (tcur.track = 0; tcur.track < numTracks; tcur.track++) {
        auto &srcEvents = tcur.events();
        auto splitPoint = tcur.findEvent();
        auto &dstEvents = secondHalf->trackEvents.emplace_back(
            splitPoint, srcEvents.end()); // copy range to new vector
        srcEvents.erase(splitPoint, srcEvents.end());
        for (auto &event : dstEvents) {
            event.time -= pos;
        }
    }

    // keep section locked...
    std::unique_lock songLock(song->mu);
    auto it = std::find(song->sections.begin(), song->sections.end(), section);
    song->sections.insert(it + 1, secondHalf);
    return true;
}

void SliceSection::undoIt(Song *song)
{
    std::unique_lock sectionLock(section->mu);
    auto secondHalf = section->next.lockDeleted();
    std::unique_lock secondLock(secondHalf->mu);
    section->length += secondHalf->length;
    section->next = secondHalf->next;

    for (int t = 0; t < section->trackEvents.size(); t++) {
        auto &srcEvents = secondHalf->trackEvents[t];
        auto &dstEvents = section->trackEvents[t];
        for (auto &event : srcEvents) {
            event.time += pos;
        }
        dstEvents.insert(dstEvents.end(), srcEvents.begin(), srcEvents.end());
        srcEvents.clear();
    }

    std::unique_lock songLock(song->mu);
    auto it = std::find(song->sections.begin(), song->sections.end(),
                        secondHalf);
    // second half will probably be completely deleted bc nothing references it
    song->sections.erase(it);
    secondHalf->deleted = true;
}

AddSample::AddSample(int index, shared_ptr<Sample> sample)
    : index(index)
    , sample(sample)
{}

bool AddSample::doIt(Song *song)
{
    std::unique_lock songLock(song->mu);
    sample->deleted = false;
    song->samples.insert(song->samples.begin() + index, sample);
    return true;
}

void AddSample::undoIt(Song *song)
{
    std::unique_lock songLock(song->mu);
    song->samples.erase(song->samples.begin() + index);
    sample->deleted = true;
}

DeleteSample::DeleteSample(shared_ptr<Sample> sample)
    : sample(sample)
{}

bool DeleteSample::doIt(Song *song)
{
    std::unique_lock songLock(song->mu);

    // clear all events that reference this sample
    for (auto &section : song->sections) {
        std::unique_lock sectionLock(section->mu);
        for (int t = 0; t < section->trackEvents.size(); t++) {
            auto &events = section->trackEvents[t];
            for (int e = 0; e < events.size(); e++) {
                if (events[e].sample.lockDeleted() == sample) {
                    sampleEvents.push_back({section, t, e});
                    events[e].sample.reset();
                }
            }
        }
    }

    auto it = std::find(song->samples.begin(), song->samples.end(), sample);
    index = it - song->samples.begin();
    song->samples.erase(it);
    sample->deleted = true;
    return true;
}

void DeleteSample::undoIt(Song *song)
{
    std::unique_lock songLock(song->mu);
    sample->deleted = false;
    song->samples.insert(song->samples.begin() + index, sample);

    for (auto &eventRef : sampleEvents) {
        std::unique_lock sectionLock(eventRef.section->mu);
        eventRef.section->trackEvents[eventRef.track][eventRef.index].sample
            = sample;
    }
}

} // namespace
