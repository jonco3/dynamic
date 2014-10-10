#ifndef __BLOCK_H__
#define __BLOCK_H__

#include <cassert>
#include <vector>
#include <ostream>

using namespace std;

struct Input;
struct Instr;
struct Layout;
struct Syntax;

struct Block
{
    static Block* buildTopLevel(const Input& input);

    Block(Layout* layout);
    ~Block();
    Instr** startInstr() { return &instrs_[0]; }
    unsigned instrCount() { return instrs_.size(); }
    Instr* instr(unsigned i) { return instrs_.at(i); }
    const Layout* layout() const { return layout_; }

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

  private:
    Layout* layout_;
    vector<Instr*> instrs_;
};

ostream& operator<<(ostream& s, Block* block);

#endif
