#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "gc.h"

#include <cassert>
#include <vector>
#include <ostream>

using namespace std;

struct Input;
struct Instr;
struct Layout;
struct Syntax;

struct Block : public Cell
{
    static Block* buildTopLevel(const Input& input);

    Block(Layout* layout);
    Instr** startInstr() { return &instrs_[0]; }
    unsigned instrCount() { return instrs_.size(); }
    Instr* instr(unsigned i) { return instrs_.at(i); }
    Layout* layout() const { return layout_; }

    unsigned append(Instr* instr);
    void branchHere(unsigned source);

    Instr* lastInstr() {
        assert(!instrs_.empty());
        return instrs_.back();
    }

    Instr** nextInstr() {
        return &instrs_.back() + 1;
    }

    bool contains(Instr** i) const {
        return i >= &instrs_.front() && i <= &instrs_.back();
    }

    virtual void traceChildren(Tracer& t);
    virtual size_t size() const { return sizeof(*this); }
    virtual void print(ostream& s) const;

  private:
    Layout* layout_;
    vector<Instr*> instrs_;
};

#endif
