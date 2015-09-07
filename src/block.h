#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "gcdefs.h"
#include "instr.h"
#include "token.h"

#include <cassert>
#include <vector>
#include <ostream>

using namespace std;

struct Input;
struct Interpreter;
struct Layout;
struct Object;
struct Syntax;

#ifdef DEBUG
extern bool assertStackDepth;
extern bool logCompile;
#endif

typedef bool (*InstrFuncBase)(Traced<Instr*> self, Interpreter& interp);

template <typename T>
using InstrFunc = bool (*)(Traced<T*> self, Interpreter& interp);

struct InstrThunk
{
    InstrType type;
    Instr* data;
};

struct Block : public Cell
{
    Block(Traced<Layout*> layout, unsigned argCount, bool createEnv);

    Layout* layout() const { return layout_; }
    unsigned argCount() const { return argCount_; }
    unsigned stackLocalCount() const;
    unsigned maxStackDepth() const { return maxStackDepth_; }
    bool createEnv() const { return createEnv_; }
    InstrThunk* startInstr() { return &instrs_[0]; }
    unsigned instrCount() { return instrs_.size(); }
    InstrThunk instr(unsigned i) { return instrs_.at(i); }

    void setMaxStackDepth(unsigned stackDepth);

    unsigned nextIndex() { return instrs_.size(); }
    void branchHere(unsigned source);
    int offsetFrom(unsigned source);
    int offsetTo(unsigned dest);

    template <typename T, typename... Args>
    inline unsigned append(Args&& ...args);

    unsigned append(Traced<Instr*> data);

    const InstrThunk& lastInstr() {
        assert(!instrs_.empty());
        return instrs_.back();
    }

    bool contains(InstrThunk* i) const {
        return i >= &instrs_.front() && i <= &instrs_.back();
    }

    virtual void traceChildren(Tracer& t) override;
    virtual void print(ostream& s) const;

    void setNextPos(const TokenPos& pos);
    TokenPos getPos(InstrThunk* instrp) const;

    // For unit tests
    InstrThunk* findInstr(unsigned type);

  private:
    Layout* layout_;
    unsigned argCount_;
    unsigned maxStackDepth_;
    bool createEnv_;
    vector<InstrThunk> instrs_;
    string file_;
    vector<pair<size_t, unsigned>> offsetLines_;
};

template <typename T, typename... Args>
unsigned Block::append(Args&& ...args)
{
    Stack<Instr*> instr(gc.create<T>(forward<Args>(args)...));
    return append(instr);
}

#endif
