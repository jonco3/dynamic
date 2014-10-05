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

static void testInterp(const string& input, const string& expected)
{
    unique_ptr<Block> block(Block::buildTopLevel(input));
    Interpreter interp;
    Value result;
    testTrue(interp.interpret(block.get(), result));
    testEqual(repr(result), expected);
}

testcase(interp)
{
    testInterp("return 3", "3");
    testInterp("return 2 + 2", "4");
    testInterp("return 2 ** 4 - 1", "15");
    testInterp("foo = 2 + 3\n"
               "return foo", "5");
    testInterp("foo = 3\n"
               "bar = 4\n"
               "return foo + bar", "7");
    // todo: should be: AttributeError: 'int' object has no attribute 'foo'
    testInterp("foo = 0\n"
               "foo.bar = 1\n"
               "return foo + foo.bar", "1");
    testInterp("return 1 | 8", "9");
    testInterp("return 3 ^ 5", "6");
    testInterp("return 3 & 5", "1");
    testInterp("return 2 > 3", "False");
    testInterp("return 3 > 3", "False");
    testInterp("return 3 > 2", "True");
    testInterp("return 2 == 3", "False");
    testInterp("return 3 == 3", "True");
    testInterp("return 3 == 2", "False");
}
