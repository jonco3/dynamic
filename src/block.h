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

    Block();
    ~Block();
    Instr** startInstr() { return &instrs[0]; }
    unsigned instrCount() { return instrs.size(); }
    Instr* instr(unsigned i) { return instrs.at(i); }
    const Layout* getLayout() const { return layout; }

    void append(Instr* instr) {
        assert(instr);
        instrs.push_back(instr);
    }

    Instr* lastInstr() {
        assert(!instrs.empty());
        return instrs.back();
    }

    Instr** nextInstr() {
        return &instrs.back() + 1;
    }

    bool contains(Instr** i) const {
        return i >= &instrs.front() && i <= &instrs.back();
    }

  private:
    const Layout *layout;
    vector<Instr*> instrs;
};

ostream& operator<<(ostream& s, Block* block);

#endif
