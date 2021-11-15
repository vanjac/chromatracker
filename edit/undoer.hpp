#pragma once
#include <common.h>

#include "edit/operation.h"

namespace chromatracker::edit {

template <typename TargetT>
class Undoer
{
private:
    TargetT target;
    vector<unique_ptr<Operation<TargetT>>> undoStack;
    vector<unique_ptr<Operation<TargetT>>> redoStack;
    // should either be back of undo stack or null
    Operation<TargetT> *continuousOp {nullptr};

public:
    void reset(TargetT target)
    {
        this->target = target;
        undoStack.clear();
        redoStack.clear();
        continuousOp = nullptr;
    }

    // op is by value not by reference for move semantics since operations are
    // typically constructed in place
    // (I think??? is there a better way to do this? TODO)
    template<typename OpT>
    bool doOp(OpT op)
    {
        continuousOp = nullptr;
        // remember that there will be less to copy/move before doIt is called
        // than after
        auto uniqueOp = std::make_unique<OpT>(std::move(op));
        if (uniqueOp->doIt(target)) {
            undoStack.push_back(std::move(uniqueOp));
            redoStack.clear();
            return true;
        }
        return false;
    }

    template<typename OpT>
    void doOp(OpT op, bool continuous)
    {
        if (!continuous) {
            doOp(std::move(op));
        } else if (auto prevOp = dynamic_cast<OpT*>(continuousOp)) {
            prevOp->undoIt(target);
            *prevOp = op;
            prevOp->doIt(target);
        } else {
            if (doOp(std::move(op)))
                continuousOp = undoStack.back().get();
        }
    }

    void endContinuous()
    {
        continuousOp = nullptr;
    }

    void undo()
    {
        if (!undoStack.empty()) {
            undoStack.back()->undoIt(target);
            redoStack.push_back(std::move(undoStack.back()));
            undoStack.pop_back();
            continuousOp = nullptr;
        } else {
            cout << "Nothing to undo\n";
        }
    }

    void redo()
    {
        if (!redoStack.empty()) {
            redoStack.back()->doIt(target);
            undoStack.push_back(std::move(redoStack.back()));
            redoStack.pop_back();
            continuousOp = nullptr;
        } else {
            cout << "Nothing to redo\n";
        }
    }
};

} // namespace
