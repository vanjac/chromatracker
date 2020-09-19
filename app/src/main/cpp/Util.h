#ifndef CHROMATRACKER_UTIL_H
#define CHROMATRACKER_UTIL_H

namespace chromatracker {

class noncopyable {
protected:
    noncopyable() = default;
    ~noncopyable() = default;

    noncopyable(noncopyable const &) = delete;
    noncopyable(noncopyable &&) = default;
    void operator=(noncopyable const &) = delete;
    noncopyable &operator=(noncopyable &&) = default;
};

}

#endif //CHROMATRACKER_UTIL_H
