#pragma once
#include <common.h>

#include <atomic>

namespace chromatracker {

// ugly hack. initializes atomic bool to false and skips copying
// TODO does this even need to be atomic?
class DeletedFlag : public std::atomic<bool>
{
public:
    DeletedFlag() : std::atomic<bool>(false) {}
    DeletedFlag(const DeletedFlag &rhs) {} // don't copy
    DeletedFlag & operator=(const DeletedFlag &rhs) { return *this; }
    bool operator=(bool desired) {return std::atomic<bool>::operator=(desired);}
};

// SongObject must be referenced with shared_ptr or ObjWeakPtr
class SongObject
{
public:
    DeletedFlag deleted;
};

// replacement for weak_ptr which automatically resets if the object is deleted
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
        if (p && p->deleted) {
            ptr.reset();
            return nullptr;
        }
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
    // TODO mutable is a bad hack :(
    mutable std::weak_ptr<T> ptr;
};

} // namespace
