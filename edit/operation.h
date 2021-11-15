#pragma once
#include <common.h>

#include <song.h>
#include <mutex>

namespace chromatracker::edit {

template <typename T>
class Operation
{
public:
    virtual ~Operation() = default;
    // perform the operation. return false if it had no effect.
    // doIt and undoIt must be called in alternating order, starting with doIt
    virtual bool doIt(T target) = 0;
    // reverse the operation
    virtual void undoIt(T target) = 0;
};

using SongOp = Operation<Song *>;

// utils for operations:

template <typename ObjT, typename ValT>
class SetObjectValue : public SongOp
{
private:
    virtual ValT & objectValue() = 0;

public:
    SetObjectValue(shared_ptr<ObjT> obj, ValT value)
        : obj(obj)
        , value(value)
    {}

    bool doIt(Song *song) override
    {
        std::unique_lock lock(obj->mu);
        std::swap(value, objectValue());
        return value != objectValue();
    }

    void undoIt(Song *song) override
    {
        doIt(song);
    }

protected:
    shared_ptr<ObjT> obj;
    ValT value;
};

struct EventRef
{
    shared_ptr<Section> section;
    int track;
    int index;
};

} // namespace
