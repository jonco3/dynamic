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
struct Object;
struct Syntax;

struct Block : public Cell
{
    static Block* buildModule(const Input& input, Traced<Object*> globals);

    Block(Traced<Layout*> layout);
    Instr** startInstr() { return &instrs_[0]; }
    unsigned instrCount() { return instrs_.size(); }
    Instr* instr(unsigned i) { return instrs_.at(i); }
    Layout* layout() const { return layout_; }

    unsigned nextIndex() { return instrs_.size(); }
    unsigned append(Instr* instr);
    void branchHere(unsigned source);
    int offsetTo(unsigned dest);

    const Instr* lastInstr() {
        assert(!instrs_.empty());
        return instrs_.back();
    }

    bool contains(Instr** i) const {
        return i >= &instrs_.front() && i <= &instrs_.back();
    }

    virtual void traceChildren(Tracer& t);
    virtual size_t size() const { return sizeof(*this); }
    virtual void print(ostream& s) const;

    static Traced<Layout*> layout(Traced<Block*> block) {
        return Traced<Layout*>::fromTracedLocation(&block->layout_);
    }

  private:
    Layout* layout_;
    vector<Instr*> instrs_;
};

#endif
