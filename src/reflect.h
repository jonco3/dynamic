/*
 * Python wrappers for various internal objects.
 */

#ifndef __REFLECT_H__
#define __REFLECT_H__

#include "block.h"
#include "object.h"

struct CodeObject : public Object
{
    static GlobalRoot<Class*> ObjectClass;

    CodeObject(Traced<Block*> block);

    Block* block() { return block_; }

    void dump() const;

    void traceChildren(Tracer& t) override;

  private:
    Block* block_;
};

extern void initReflect();

#endif
