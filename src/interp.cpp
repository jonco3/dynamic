#include "interp.h"
#include "frame.h"
#include "block.h"
#include "instr.h"
#include "test.h"
#include "utility.h"
#include "callable.h"

//#define TRACE_INTERP

bool Interpreter::interpret(Block* block, Value& valueOut)
{
    frame = new Frame(*this, nullptr);
    instrp = block->startInstr();

    while (instrp) {
        Instr *instr = *instrp++;
#ifdef TRACE_INTERP
        std::cerr << "execute " << repr(instr) << std::endl;
#endif
        if (!instr->execute(*this, frame)) {
            std::cerr << "Error" << std::endl;
            return false;
        }
    }

    assert(stack.size() == 1);
    valueOut = popStack();
    return true;
}

Frame* Interpreter::pushFrame(Function *function)
{
    Frame *newFrame = new Frame(*this, frame);
    instrp = function->startInstr();
    frame = newFrame;
    return newFrame;
}

void Interpreter::popFrame()
{
    assert(frame->stackPos() < stack.size());
    stack.resize(frame->stackPos());
    instrp = frame->returnInstr();
    Frame* oldFrame = frame;
    frame = oldFrame->popFrame();
    delete oldFrame;
}

testcase(interp)
{
    Interpreter interp;

    Block* block = Block::buildTopLevel("3");
    Value v;
    testTrue(interp.interpret(block, v));
    testEqual(repr(v), "3");
    delete block;

    block = Block::buildTopLevel("2 + 2");
    testTrue(interp.interpret(block, v));
    testEqual(repr(v), "4");
    delete block;
}
