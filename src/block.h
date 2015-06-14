#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "gcdefs.h"
#include "token.h"

#include <cassert>
#include <vector>
#include <ostream>

using namespace std;

struct Input;
struct Instr;
struct Interpreter;
struct Layout;
struct Object;
struct Syntax;

#ifdef DEBUG
extern bool assertStackDepth;
#endif

typedef bool (*InstrFuncBase)(Traced<Instr*> self, Interpreter& interp);

template <typename T>
using InstrFunc = bool (*)(Traced<T*> self, Interpreter& interp);

struct InstrThunk
{
    InstrFuncBase func;
    Instr* data;
};

struct Block : public Cell
{
    static void buildModule(const Input& input, Traced<Object*> globals,
                            Root<Block*>& blockOut);

    Block(Traced<Layout*> layout);
    InstrThunk* startInstr() { return &instrs_[0]; }
    unsigned instrCount() { return instrs_.size(); }
    InstrThunk instr(unsigned i) { return instrs_.at(i); }
    Layout* layout() const { return layout_; }

    unsigned nextIndex() { return instrs_.size(); }
    void branchHere(unsigned source);
    int offsetFrom(unsigned source);
    int offsetTo(unsigned dest);

    template <typename T, typename... Args>
    inline unsigned append(Args&& ...args);

    unsigned append(InstrFuncBase func, Traced<Instr*> data);

    const InstrThunk& lastInstr() {
        assert(!instrs_.empty());
        return instrs_.back();
    }

    bool contains(InstrThunk* i) const {
        return i >= &instrs_.front() && i <= &instrs_.back();
    }

    virtual void traceChildren(Tracer& t) override;
    virtual void print(ostream& s) const;

    static Traced<Layout*> layout(Traced<Block*> block) {
        return Traced<Layout*>::fromTracedLocation(&block->layout_);
    }

    void setNextPos(const TokenPos& pos);
    TokenPos getPos(InstrThunk* instrp) const;

    // For unit tests
    InstrThunk* findInstr(unsigned type);

  private:
    Layout* layout_;
    vector<InstrThunk> instrs_;
    string file_;
    vector<pair<size_t, unsigned>> offsetLines_;
};

template <typename T, typename... Args>
unsigned Block::append(Args&& ...args)
{
    Root<Instr*> instr(gc.create<T>(forward<Args>(args)...));
    InstrFunc<T> func = T::execute;
    return append(reinterpret_cast<InstrFuncBase>(func), instr);
}

#endif
