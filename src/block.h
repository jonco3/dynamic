#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "assert.h"
#include "gcdefs.h"
#include "instr.h"
#include "token.h"

#include <vector>
#include <ostream>

using namespace std;

struct Input;
struct Interpreter;
struct Layout;
struct Object;
struct Syntax;

struct Block : public Cell
{
    Block(Traced<Block*> parent,
          Traced<Env*> global,
          Traced<Layout*> layout,
          unsigned argCount,
          bool createEnv);

    Block* parent() const { return parent_; }
    Env* global() const { return global_; }
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

    template <InstrCode Code, typename... Args>
    inline unsigned append(Args&& ...args);

    unsigned append(Traced<Instr*> data);

    const InstrThunk& lastInstr() {
        assert(!instrs_.empty());
        return instrs_.back();
    }

    bool contains(const InstrThunk* i) const {
        return i >= &instrs_.front() && i <= &instrs_.back();
    }

    void traceChildren(Tracer& t) override;
    void print(ostream& s) const override;

    void setNextPos(const TokenPos& pos);
    TokenPos getPos(const InstrThunk* instrp) const;

    // For unit tests
    InstrThunk* findInstr(unsigned code);

  private:
    Heap<Block*> parent_;
    Heap<Env*> global_;
    Heap<Layout*> layout_;
    unsigned argCount_;
    unsigned maxStackDepth_;
    bool createEnv_;
    vector<InstrThunk> instrs_;
    string file_;
    vector<pair<size_t, unsigned>> offsetLines_;
};

template <InstrCode Code, typename... Args>
unsigned Block::append(Args&& ...args)
{
    Stack<Instr*> instr(InstrFactory<Code>::get(forward<Args>(args)...));
    return append(instr);
}

#endif
