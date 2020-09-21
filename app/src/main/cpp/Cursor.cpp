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

    std::unique_lock<std::mutex> lock(this->pattern->mtx);

    const std::vector<Event> &events = this->pattern->events;

    // pattern length could have changed
    while (this->time >= this->pattern->length) {
        this->time -= this->pattern->length;
        this->event_i = 0;  // more likely than whatever it is now
    }
    if (this->event_i > events.size())
        this->event_i = events.size();

    // check if event_i is invalid
    // could happen if pattern has been modified elsewhere
    if (this->event_i < events.size()
        && events[this->event_i].time < this->time)
        search_current_event();
    else if (this->event_i > 0 && events[this->event_i - 1].time >= this->time)
        search_current_event();
    else if (this->event_i >= events.size() && !events.empty()
             && events.back().time >= this->time)
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
    return this->pattern == nullptr;
}

const Pattern * PatternCursor::get_pattern() const {
    return this->pattern;
}

const std::vector<Event> & PatternCursor::get_events() const {
    return this->pattern->events;
}

int PatternCursor::get_time() const {
    return this->time;
}

int PatternCursor::get_event_i() const {
    return this->event_i;
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
    this->time += amount;
    if (is_null() || amount == 0)
        return;
    const std::vector<Event> &events = this->pattern->events;

    if (amount > 0) {
        while (this->time >= this->pattern->length) {
            this->time -= this->pattern->length;
            this->event_i = 0;
        }
        if (events.empty())
            return;
        while (this->event_i < events.size()) {
            if (events[this->event_i].time < this->time)
                this->event_i++;
            else
                break;
        }
    } else {  // negative amount
        while (this->time < 0) {
            this->time += this->pattern->length;
            this->event_i = events.size();
        }
        if (events.empty())
            return;
        while (this->event_i > 0) {
            if (events[this->event_i - 1].time >= this->time)
                this->event_i--;
            else
                break;
        }
    }
}

void PatternCursor::search_current_event() {
    if (this->time == 0) {
        event_i = 0;
        return;
    }
    const std::vector<Event> &events = this->pattern->events;
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
        if (t < this->time) {
            min = i + 1;
        } else if (t > this->time) {
            max = i - 1;
        } else {
            this->event_i = i;
            LOGD("Cursor set to event %d of %d", this->event_i, events.size());
            return;
        }
    }

    // now min > max, pick the higher one
    this->event_i = min;
    LOGD("Cursor set to event %d of %d", this->event_i, events.size());
}

}