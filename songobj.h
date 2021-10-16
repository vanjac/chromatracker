#pragma once
#include <common.h>

namespace chromatracker {

// SongObject must be referenced with shared_ptr or ObjWeakPtr
class SongObject
{
public:
    bool deleted {false};
};

// replacement for weak_ptr, handles deleted objects correctly
template<typename T>
class ObjWeakPtr
{
public:
    ObjWeakPtr() {}
    ObjWeakPtr(const ObjWeakPtr &r) : ptr(r.ptr) {}
    template<typename Y>
    ObjWeakPtr(const ObjWeakPtr<Y> &r) : ptr(r.ptr) {}
    template<typename Y>
    ObjWeakPtr(const shared_ptr<Y> &r) : ptr(r) {}

    ObjWeakPtr & operator=(const ObjWeakPtr &r)
    {
        ptr = r.ptr;
        return *this;
    }
    template<typename Y>
    ObjWeakPtr & operator=(const ObjWeakPtr<Y> &r)
    {
        ptr = r.ptr;
        return *this;
    }
    template<typename Y>
    ObjWeakPtr & operator=(const shared_ptr<Y> &r)
    {
        ptr = r;
        return *this;
    }

    // null if deleted
    std::shared_ptr<T> lock() const
    {
        auto p = ptr.lock();
        if (p && p->deleted)
            return nullptr;
        return p;
    }

    // returns pointer even if deleted
    std::shared_ptr<T> lockDeleted() const
    {
        return ptr.lock();
    }

    void reset()
    {
        ptr.reset();
    }
private:
    std::weak_ptr<T> ptr;
};

} // namespace
