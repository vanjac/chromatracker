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

void Cursor::move(ticks amount, Space space)
{
    if (!valid())
        return;
    time += amount;

    switch (space) {
    case Space::Song:
        if (amount >= 0) {
            while (section) {
                std::shared_lock lock(section->mu);
                if (time >= section->length) {
                    std::shared_lock lock(song->mu);
                    auto it = findSection();
                    if (it == song->sections.end()) {
                        section = nullptr;
                        break;
                    }
                    it++;
                    if (it == song->sections.end()) {
                        time = section->length;
                        break;
                    } else {
                        time -= section->length;
                        section = it->get();
                    }
                } else {
                    break;
                }
            }
        } else {
            while (section && time < 0) {
                std::shared_lock songLock(song->mu);
                auto it = findSection();
                if (it == song->sections.end()) {
                    section = nullptr;
                    break;
                } else if (it == song->sections.begin()) {
                    time = 0;
                    break;
                } else {
                    section = (--it)->get();
                    std::shared_lock sectionLock(section->mu);
                    time += section->length;
                }
            }
        }
        break;
    case Space::Playback:
        if (amount >= 0) {
            while (section) {
                std::shared_lock lock(section->mu);
                if (time >= section->length) {
                    time -= section->length;
                    section = section->next;
                } else {
                    break;
                }
            }
        } else if (time < 0) {
            // this should never happen
            time = 0;
        }
        break;
    case Space::SectionLoop:
        if (amount >= 0) {
            std::shared_lock lock(section->mu);
            time %= section->length;
        } else if (time < 0) {
            std::shared_lock lock(section->mu);
            time %= section->length;
            time += section->length;
        }
        break;
    }
}

vector<unique_ptr<Section>>::iterator Cursor::findSection() const
{
    return std::find_if(song->sections.begin(), song->sections.end(),
        [&](std::unique_ptr<Section> &elem) {
            return elem.get() == section;
        });
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
