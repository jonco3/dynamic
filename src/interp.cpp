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
    frame = new Frame(nullptr, block);
    instrp = block->startInstr();

    while (instrp) {
        assert(frame->block()->contains(instrp));
        Instr *instr = *instrp++;
#ifdef TRACE_INTERP
        cerr << "stack:";
        for (auto i = stack.begin(); i != stack.end(); ++i)
            cerr << " " << *i;
        cerr << endl;
        cerr << "  execute " << repr(instr) << endl;
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
    Block* block = function->block();
    Frame *newFrame = new Frame(frame, block);
    instrp = block->startInstr();
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
}

void Interpreter::branch(int offset)
{
    Instr** target = instrp + offset - 1;
    assert(frame->block()->contains(target));
    instrp = target;
}

static void testInterp(const string& input, const string& expected)
{
    //gc::collect(); // Check necessary roots are in place
    Root<Block*> block(Block::buildTopLevel(input));
    Interpreter interp;
    Value result;
    testTrue(interp.interpret(block, result));
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
    testInterp("return 1 or 2", "1");
    testInterp("return 0 or 2", "2");
    testInterp("return 1 and 2", "2");
    testInterp("return 0 and 2", "0");
    testInterp("return -2", "-2");
    testInterp("return --2", "2");
    testInterp("return -2 + 1", "-1");
    testInterp("return 2 - -1", "3");
    testInterp("return 1 if 2 < 3 else 0", "1");
    testInterp("return 1 if 2 > 3 else 2 + 2", "4");
    testInterp("return (lambda: 2 + 2)()", "4");
    testInterp("return (lambda x: x + 1)(1)", "2");
    testInterp("x = 0\nreturn (lambda x: x + 1)(1)", "2");
    testInterp("x = 0\nreturn (lambda y: x + 1)(1)", "1");
}
