#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "name.h"
#include "input.h"

#include <vector>
#include <ostream>

struct Instr;
struct Syntax;

struct Block
{
    static Block* buildTopLevel(const Input& input);

    ~Block();
    Instr** startInstr() { return &instrs[0]; }
    unsigned instrCount() { return instrs.size(); }
    Instr* instr(unsigned i) { return instrs.at(i); }

    void append(Instr* instr) { instrs.push_back(instr); }

  private:
    std::vector<Instr*> instrs;
};

std::ostream& operator<<(std::ostream& s, Block* block);

#endif
