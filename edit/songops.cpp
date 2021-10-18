#include "songops.h"
#include <algorithm>

namespace chromatracker::edit::ops {

SetTrackMute::SetTrackMute(int track, bool mute)
    : track(track)
    , mute(mute)
{}

bool SetTrackMute::doIt(Song *song)
{
    shared_ptr<Track> t;
    {
        std::unique_lock songLock(song->mu);
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
    auto it = tcur.findEvent();
    tcur.events().erase(it);
    ClearCell::undoIt(song);
}

AddSection::AddSection(int index, ticks length)
    : index(index)
    , section(new Section)
{
    section->length = length;
}

bool AddSection::doIt(Song *song)
{
    std::unique_lock songLock(song->mu);

    song->sections.insert(song->sections.begin() + index, section);

    std::unique_lock sectionLock(section->mu);
    section->trackEvents.clear();
    section->trackEvents.insert(section->trackEvents.end(),
                                song->tracks.size(), vector<Event>());
    
    if (index != song->sections.size() - 1) {
        section->next = song->sections[index + 1];
    }
    if (index != 0) {
        if (song->sections.front()->next.lock() == section->next.lock())
            song->sections.front()->next = section;
    }

    section->deleted = false;
    return true;
}

void AddSection::undoIt(Song *song)
{
    std::unique_lock songLock(song->mu);
    section->deleted = true;
    song->sections.erase(song->sections.begin() + index);

    if (index != 0) {
        if (song->sections.front()->next.lock() == section) {
            if (index != song->sections.size()) {
                song->sections.front()->next = song->sections[index];
            } else {
                song->sections.front()->next.reset();
            }
        }
    }
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
    section->deleted = true;
    auto it = std::find(song->sections.begin(), song->sections.end(), section);
    index = it - song->sections.begin();
    song->sections.erase(it);

    // fix sections which point to the deleted section
    for (int i = 0; i < song->sections.size(); i++) {
        auto &other = song->sections[i];
        std::unique_lock otherLock(other->mu);
        if (other->next.lock() == section) {
            prevLinks.push_back(other);
            if (i != song->sections.size() - 1)
                other->next = song->sections[i + 1];
            else
                other->next.reset();
        }
    }
    return true;
}

void DeleteSection::undoIt(Song *song)
{
    if (index < 0)
        return;
    std::unique_lock songLock(song->mu);
    song->sections.insert(song->sections.begin() + index, section);

    for (auto &link : prevLinks) {
        std::unique_lock linkLock(link->mu);
        link->next = section;
    }

    section->deleted = false;
    prevLinks.clear();
}

} // namespace
