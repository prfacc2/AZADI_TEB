// ============================================================================
//  undo.h  — header-only generic undo/redo ring buffer (release 1.4.0)
//  Used by the print designer (§11). State is copied by value on push; the
//  template parameter must be copyable. Depth is capped (ring buffer) so a
//  long editing session never grows unbounded.
// ============================================================================
#pragma once
#include <vector>
#include <cstddef>

template<class State>
class UndoStack {
public:
    explicit UndoStack(size_t maxDepth = 100)
        : cap_(maxDepth ? maxDepth : 1) {}

    // Commit a new state. Anything that was "redoable" is discarded.
    void push(const State& s){
        // drop redo tail
        if(cur_ + 1 < buf_.size()) buf_.resize(cur_ + 1);
        buf_.push_back(s);
        cur_ = buf_.size() - 1;
        // enforce ring cap
        while(buf_.size() > cap_){
            buf_.erase(buf_.begin());
            if(cur_ > 0) --cur_;
        }
    }

    // Move one step back. Returns false if nothing to undo. On success,
    // outPrev receives the now-current state.
    bool undo(State& outPrev){
        if(cur_ == 0 || buf_.empty()) return false;
        --cur_;
        outPrev = buf_[cur_];
        return true;
    }

    // Move one step forward. Returns false if nothing to redo.
    bool redo(State& outNext){
        if(buf_.empty() || cur_ + 1 >= buf_.size()) return false;
        ++cur_;
        outNext = buf_[cur_];
        return true;
    }

    bool canUndo() const { return cur_ > 0; }
    bool canRedo() const { return !buf_.empty() && cur_ + 1 < buf_.size(); }
    size_t depth() const { return buf_.size(); }

    void clear(){ buf_.clear(); cur_ = 0; }

    // Seed the stack with the initial state (so the first edit has a baseline).
    void seed(const State& s){ buf_.clear(); buf_.push_back(s); cur_ = 0; }

private:
    std::vector<State> buf_;
    size_t cur_ = 0;
    size_t cap_;
};
