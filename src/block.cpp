#include "block.h"

#include "frame.h"
#include "instr.h"

Block::Block(Traced<Block*> parent,
             Traced<Env*> global,
             Traced<Layout*> layout,
             unsigned argCount,
             bool createEnv)
  : parent_(parent),
    global_(global),
    layout_(layout),
    argCount_(argCount),
    maxStackDepth_(0),
    createEnv_(createEnv)
{}

unsigned Block::stackLocalCount() const
{
    assert(layout()->slotCount() >= argCount());
    return createEnv() ? 0 : layout()->slotCount() - argCount();
}

void Block::setMaxStackDepth(unsigned stackDepth)
{
    assert(maxStackDepth_ == 0);
    assert(stackDepth > 0);
    maxStackDepth_ = stackDepth;
}

unsigned Block::append(Traced<Instr*> data)
{
    assert(data);
    unsigned index = nextIndex();
    instrs_.emplace_back(data);
    return index;
}

int Block::offsetFrom(unsigned source)
{
    assert(source < instrs_.size() - 1);
    return instrs_.size() - source;
}

int Block::offsetTo(unsigned dest)
{
    assert(dest < instrs_.size() - 1);
    return dest - instrs_.size();
}

void Block::branchHere(unsigned source)
{
    assert(source < instrs_.size() - 1);
    BranchInstr* b = instrs_[source].data->as<BranchInstr>();
    b->setOffset(offsetFrom(source));
}

void Block::print(ostream& s) const {
    for (auto i = instrs_.begin(); i != instrs_.end(); ++i) {
        if (i != instrs_.begin())
            s << ", ";
        i->data->print(s);
    }
}

void Block::traceChildren(Tracer& t)
{
    gc.trace(t, &parent_);
    gc.trace(t, &global_);
    gc.trace(t, &layout_);
    for (auto i = instrs_.begin(); i != instrs_.end(); ++i)
        gc.trace(t, &i->data);
}

TokenPos Block::getPos(const InstrThunk* instrp) const
{
    assert(contains(instrp));

    // We don't have any information for manually created blocks.
    if (offsetLines_.empty())
        return TokenPos();

    size_t offset = instrp - &instrs_.front();
    for (int i = offsetLines_.size() - 1; i >= 0; --i) {
        if (offsetLines_[i].first <= offset)
            return TokenPos(file_, offsetLines_[i].second, 0);
    }
    assert(false);
    return TokenPos();
}

void Block::setNextPos(const TokenPos& pos)
{
    if (offsetLines_.empty()) {
        file_ = pos.file;
        offsetLines_.push_back({0, pos.line});
        return;
    }

    assert(file_ == pos.file);
    if (offsetLines_.back().second != pos.line)
        offsetLines_.emplace_back(instrs_.size(), pos.line);
}

InstrThunk* Block::findInstr(unsigned code)
{
    for (auto i = instrs_.begin(); i != instrs_.end(); ++i) {
        if (i->data->code() == code)
            return &(*i);
    }
    return nullptr;
}
