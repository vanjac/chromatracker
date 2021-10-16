#include "cursor.h"
#include <algorithm>

namespace chromatracker {

Cursor::Cursor()
    : song(nullptr)
    , time(0)
{}

Cursor::Cursor(Song *song)
    : song(song)
    , time(0)
{}

Cursor::Cursor(Song *song, shared_ptr<Section> section)
    : song(song)
    , section(section)
    , time(0)
{}

void Cursor::playStep()
{
    time++;

    while (auto sectionP = section.lock()) {
        std::shared_lock lock(sectionP->mu);
        if (time >= sectionP->length) {
            section = sectionP->next;
            time = 0;
        } else {
            break;
        }
    }
}

vector<shared_ptr<Section>>::iterator Cursor::findSection() const
{
    return std::find(song->sections.begin(), song->sections.end(),
                     section.lock());
}

shared_ptr<Section> Cursor::nextSection() const
{
    std::shared_lock lock(song->mu);
    auto it = findSection();
    if (it == song->sections.end())
        return nullptr;
    it++;
    if (it == song->sections.end())
        return nullptr;
    return *it;
}

shared_ptr<Section> Cursor::prevSection() const
{
    std::shared_lock lock(song->mu);
    auto it = findSection();
    if (it == song->sections.end() || it == song->sections.begin())
        return nullptr;
    return *(--it);
}

vector<Event> & TrackCursor::events() const
{
    // TODO should be already locked??
    return cursor.section.lock()->trackEvents[track];
}

vector<Event>::iterator TrackCursor::findEvent() const
{
    auto &events = this->events();
    if (time == 0 || events.empty())
        return events.begin();

    // binary search
    // find the first index >= time
    int min = 0, max = events.size() - 1;
    while (min <= max) {
        int i = (min + max) / 2;
        int t = events[i].time;
        if (t < cursor.time) {
            min = i + 1;
        } else if (t > cursor.time) {
            max = i - 1;
        } else {
            return events.begin() + i;
        }
    }

    // now min > max, pick the higher one
    return events.begin() + min;
}

} // namespace
