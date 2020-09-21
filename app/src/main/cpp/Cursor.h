#ifndef CHROMATRACKER_CURSOR_H
#define CHROMATRACKER_CURSOR_H

#include <mutex>
#include <vector>
#include "Pattern.h"

namespace chromatracker {

// access Patterns in a thread-safe way
// PatternCursor itself is *not* thread-safe!
class PatternCursor {
public:
    PatternCursor();

    // unique_locks can be moved (incl returned from functions)
    std::unique_lock<std::mutex> lock();
    // faster than calling lock() + set() separately
    std::unique_lock<std::mutex> set_lock(const Pattern *pattern, int time);

    bool is_null() const;

    /* below methods are only safe to call with a lock!! */

    const Pattern *get_pattern() const;
    const std::vector<Event> &get_events() const;
    int get_time() const;
    int get_event_i() const;

    // time must be >= 0. will wrap automatically
    void set(const Pattern *pattern, int time);
    // wraps time automatically
    // works best for small increments, otherwise use set()
    void move(int amount);

private:
    // update event_i
    // time must be correct and pattern must not be null
    void search_current_event();

    const Pattern *pattern;
    // always >= 0
    // while locked, always < pattern.length
    int time;
    // always >= 0
    // while locked, always <= events.size()
    int event_i;
};

}

#endif //CHROMATRACKER_CURSOR_H
