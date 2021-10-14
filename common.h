#pragma once

#include <memory>
using std::unique_ptr;
using std::shared_ptr;

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <iostream>
using std::cout;

class noncopyable {
protected:
    noncopyable() = default;
    ~noncopyable() = default;

    noncopyable(noncopyable const &) = delete;
    void operator=(noncopyable const &) = delete;
};

// https://stackoverflow.com/a/3457575
template <typename T>
struct nevercopy : public T {
    nevercopy() {}
    nevercopy(const nevercopy<T> &rhs) {}
    nevercopy<T> & operator=(const nevercopy<T> &rhs) { return *this; }
};
