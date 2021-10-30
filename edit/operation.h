#pragma once
#include <common.h>

#include <song.h>

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

struct EventRef
{
    shared_ptr<Section> section;
    int track;
    int index;
};

} // namespace
