#include "cursor.h"
#include <algorithm>

namespace chromatracker {

Cursor::Cursor()
    : song(nullptr)
    , section(nullptr)
    , time(0)
{}

Cursor::Cursor(Song *song)
    : song(song)
    , section(nullptr)
    , time(0)
{}

Cursor::Cursor(Song *song, Section *section)
    : song(song)
    , section(section)
    , time(0)
{}

bool Cursor::valid() const
{
    return song != nullptr && section != nullptr;
}

void Cursor::playStep()
{
    time++;

    while (section) {
        std::shared_lock lock(section->mu);
        if (time >= section->length) {
            section = section->next;
            time = 0;
        } else {
            break;
        }
    }
}

vector<unique_ptr<Section>>::iterator Cursor::findSection() const
{
    return std::find_if(song->sections.begin(), song->sections.end(),
        [&](std::unique_ptr<Section> &elem) {
            return elem.get() == section;
        });
}

Section * Cursor::nextSection() const
{
    std::shared_lock lock(song->mu);
    auto it = findSection();
    if (it == song->sections.end())
        return nullptr;
    it++;
    if (it == song->sections.end())
        return nullptr;
    return it->get();
}

Section * Cursor::prevSection() const
{
    std::shared_lock lock(song->mu);
    auto it = findSection();
    if (it == song->sections.end() || it == song->sections.begin())
        return nullptr;
    return (--it)->get();
}

vector<Event> & TrackCursor::events() const
{
    return cursor.section->trackEvents[track];
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
