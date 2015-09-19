/*
 * Python wrappers for various internal objects.
 */

#ifndef __REFLECT_H__
#define __REFLECT_H__

#include "block.h"
#include "object.h"

#include <memory>
#include <ostream>

struct ParseTree : public Object
{
    static GlobalRoot<Class*> ObjectClass;

    ParseTree(unique_ptr<Syntax> syntax);

    void dump(ostream& s) const override;

  private:
    unique_ptr<Syntax> syntax_;
};

struct CodeObject : public Object
{
    static GlobalRoot<Class*> ObjectClass;

    CodeObject(Traced<Block*> block);

    Block* block() { return block_; }

    void dump(ostream& s) const override;

    void traceChildren(Tracer& t) override;

  private:
    Heap<Block*> block_;
};

extern void initReflect();

#endif
