#include "Cursor.h"
#include "Log.h"

namespace chromatracker {

// TODO: test all of this!

PatternCursor::PatternCursor() :
        pattern(nullptr),
        time(0),
        event_i(0) {}

std::unique_lock<std::mutex> PatternCursor::lock() {
    if (is_null())
        return std::unique_lock<std::mutex>();  // no mutex

    std::unique_lock<std::mutex> lock(pattern->mtx);

    const std::vector<Event> &events = pattern->events;

    // pattern length could have changed
    while (time >= pattern->length) {
        time -= pattern->length;
        event_i = 0;  // more likely than whatever it is now
    }
    if (event_i > events.size())
        event_i = events.size();

    // check if event_i is invalid
    // could happen if pattern has been modified elsewhere
    if (event_i < events.size()
        && events[event_i].time < time)
        search_current_event();
    else if (event_i > 0 && events[event_i - 1].time >= time)
        search_current_event();
    else if (event_i >= events.size() && !events.empty()
             && events.back().time >= time)
        search_current_event();

    // event_i is in a good state
    return lock;
}

std::unique_lock<std::mutex> PatternCursor::set_lock(
        const Pattern *pattern, int time) {
    if (pattern == nullptr) {
        set(pattern, time);
        return std::unique_lock<std::mutex>();  // no mutex
    }
    std::unique_lock<std::mutex> lock(pattern->mtx);
    set(pattern, time);
    return lock;
}

bool PatternCursor::is_null() const {
    return pattern == nullptr;
}

const Pattern * PatternCursor::get_pattern() const {
    return pattern;
}

const std::vector<Event> & PatternCursor::get_events() const {
    return pattern->events;
}

int PatternCursor::get_time() const {
    return time;
}

int PatternCursor::get_event_i() const {
    return event_i;
}

void PatternCursor::set(const Pattern *pattern, int time) {
    this->pattern = pattern;
    this->time = time;
    if (pattern != nullptr) {
        this->time %= pattern->length;
        search_current_event();
    }
}

void PatternCursor::move(int amount) {
    time += amount;
    if (is_null() || amount == 0)
        return;
    const std::vector<Event> &events = pattern->events;

    if (amount > 0) {
        while (time >= pattern->length) {
            time -= pattern->length;
            event_i = 0;
        }
        if (events.empty())
            return;
        while (event_i < events.size()) {
            if (events[event_i].time < time)
                event_i++;
            else
                break;
        }
    } else {  // negative amount
        while (time < 0) {
            time += pattern->length;
            event_i = events.size();
        }
        if (events.empty())
            return;
        while (event_i > 0) {
            if (events[event_i - 1].time >= time)
                event_i--;
            else
                break;
        }
    }
}

void PatternCursor::search_current_event() {
    if (time == 0) {
        event_i = 0;
        return;
    }
    const std::vector<Event> &events = pattern->events;
    if (events.empty()) {
        event_i = 0;
        return;
    }
    LOGD("Lost cursor position, searching");

    // binary search
    // find the first index >= pattern_time
    int min = 0, max = events.size() - 1;
    while (min <= max) {
        int i = (min + max) / 2;
        int t = events[i].time;
        if (t < time) {
            min = i + 1;
        } else if (t > time) {
            max = i - 1;
        } else {
            event_i = i;
            LOGD("Cursor set to event %d of %d", event_i, events.size());
            return;
        }
    }

    // now min > max, pick the higher one
    event_i = min;
    LOGD("Cursor set to event %d of %d", event_i, events.size());
}



SongCursor::SongCursor(const Song *song) :
        song(song),
        page_itr(song->pages.end()),
        page_time(0) { }

bool SongCursor::is_null() {
    return page_itr == song->pages.end();
}

void SongCursor::set_begin() {
    page_itr = song->pages.begin();
    page_time = 0;
    this->tracks.clear();
    this->tracks.reserve(song->tracks.size());
    for (const auto &track : song->tracks)
        this->tracks.emplace_back();
    set_tracks();
}

void SongCursor::set_null() {
    page_itr = song->pages.begin();
    page_time = 0;
    this->tracks.clear();
}

bool SongCursor::move(int amount) {
    page_time += amount;
    if (is_null())
        return false;

    bool new_page = false;
    while (page_time >= page_itr->length) {
        page_time -= page_itr->length;
        page_itr++;
        if (page_itr == song->pages.end())
            page_itr = song->pages.begin();  // loop song
        new_page = true;
    }
    while (page_time < 0) {
        if (page_itr == song->pages.begin())
            page_itr = song->pages.end();
        page_itr--;
        page_time += page_itr->length;
        new_page = true;
    }

    if (new_page) {
        set_tracks();
    } else {
        for (auto &track : tracks)
            track.move(amount);
    }

    return new_page;
}

void SongCursor::set_tracks() {
    // TODO: what if tracks changed?
    for (int i = 0; i < tracks.size(); i++) {
        tracks[i].set_lock(page_itr->track_patterns[i], page_time);
    }
}


}