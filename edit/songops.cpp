#include "songops.h"
#include <algorithm>

namespace chromatracker::edit::ops {

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
    auto it = std::find(song->sections.begin(), song->sections.end(), section);
    index = it - song->sections.begin();
    song->sections.erase(it);
    {
        std::unique_lock sectionLock(section->mu);
        section->deleted = true;
    }

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
    {
        std::unique_lock sectionLock(section->mu);
        section->deleted = false;
    }

    for (auto &link : prevLinks) {
        std::unique_lock linkLock(link->mu);
        link->next = section;
    }
    prevLinks.clear();
}

} // namespace
