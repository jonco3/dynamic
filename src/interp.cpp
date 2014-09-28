#include "input.h"
#include "interp.h"
#include "frame.h"
#include "block.h"
#include "instr.h"
#include "test.h"
#include "repr.h"
#include "callable.h"

//#define TRACE_INTERP

bool Interpreter::interpret(Block* block, Value& valueOut)
{
    frame = new Frame(*this, nullptr, block->getLayout());
    instrp = block->startInstr();

    while (instrp) {
        Instr *instr = *instrp++;
#ifdef TRACE_INTERP
        cerr << "execute " << repr(instr) << endl;
#endif
        if (!instr->execute(*this, frame)) {
            cerr << "Error" << endl;
            return false;
        }
    }

    assert(stack.size() == 1);
    valueOut = popStack();
    return true;
}

Frame* Interpreter::pushFrame(Function *function)
{
    Frame *newFrame = new Frame(*this, frame, function->layout());
    instrp = function->startInstr();
    frame = newFrame;
    return newFrame;
}

void Interpreter::popFrame()
{
    assert(frame->stackPos() <= stack.size());
    stack.resize(frame->stackPos());
    instrp = frame->returnInstr();
    Frame* oldFrame = frame;
    frame = oldFrame->popFrame();
    delete oldFrame;
}

testcase(interp)
{
    unique_ptr<Block> block;
    Interpreter interp;
    Value v;

    block.reset(Block::buildTopLevel("return 3"));
    testEqual(repr(block.get()), "ConstInteger 3, Return");
    testTrue(interp.interpret(block.get(), v));
    testEqual(repr(v), "3");

    block.reset(Block::buildTopLevel("return 2 + 2"));
    testTrue(interp.interpret(block.get(), v));
    testEqual(repr(v), "4");

    block.reset(Block::buildTopLevel("return 2 ** 4 - 1"));
    testTrue(interp.interpret(block.get(), v));
    testEqual(repr(v), "15");

    block.reset(Block::buildTopLevel("foo = 2 + 3\nreturn foo"));
    testTrue(interp.interpret(block.get(), v));
    testEqual(repr(v), "5");

    block.reset(Block::buildTopLevel("foo = 3\nbar = 4\nreturn foo + bar"));
    testTrue(interp.interpret(block.get(), v));
    testEqual(repr(v), "7");

    // todo: should be: AttributeError: 'int' object has no attribute 'foo'
    block.reset(Block::buildTopLevel("foo = 0\nfoo.bar = 1\nreturn foo + foo.bar"));
    testTrue(interp.interpret(block.get(), v));
    testEqual(repr(v), "1");
}
