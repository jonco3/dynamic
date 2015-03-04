#include "instr.h"

const char* BinaryOpMethodNames[] = {
    "__add__",
    "__sub__",
    "__mul__",
    "__div__",
    "__floordiv__",
    "__mod__",
    "__pow__",
    "__or__",
    "__xor__",
    "__and__",
    "__lshift__",
    "__rshift__"
};
static_assert(sizeof(BinaryOpMethodNames)/sizeof(*BinaryOpMethodNames) == CountBinaryOp,
              "Number of method names must match number of binary operations");

const char* AugAssignMethodNames[] = {
    "__iadd__",
    "__isub__",
    "__imul__",
    "__idiv__",
    "__ifloordiv__",
    "__imod__",
    "__ipow__",
    "__ior__",
    "__ixor__",
    "__iand__",
    "__ilshift__",
    "__irshift__"
};
static_assert(sizeof(AugAssignMethodNames)/sizeof(*AugAssignMethodNames) == CountBinaryOp,
              "Number of method names must match number of binary operations");


bool InstrConst::execute(Interpreter& interp)
{
    interp.pushStack(value);
    return true;
}

bool IdentInstrBase::raiseAttrError(Traced<Value> value, Interpreter& interp) {
    const Class* cls = value.toObject()->getClass();
    string message = "'" + cls->name() + "' object has no attribute '" + ident + "'";
    interp.pushStack(gc::create<Exception>("AttributeError", message));
    return false;
}

bool InstrGetLocal::execute(Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    interp.pushStack(frame->getAttr(ident));
    return true;
}

bool InstrSetLocal::execute(Interpreter& interp)
{
    Root<Value> value(interp.peekStack(0));
    interp.getFrame()->setAttr(ident, value);
    return true;
}

bool InstrGetLexical::execute(Interpreter& interp)
{
    Frame* frame = interp.getFrame(frameIndex);
    interp.pushStack(frame->getAttr(ident));
    return true;
}

bool InstrSetLexical::execute(Interpreter& interp)
{
    Root<Value> value(interp.peekStack(0));
    interp.getFrame(frameIndex)->setAttr(ident, value);
    return true;
}

bool InstrGetGlobal::execute(Interpreter& interp)
{
    interp.pushStack(global->getAttr(ident));
    return true;
}

bool InstrSetGlobal::execute(Interpreter& interp)
{
    Root<Value> value(interp.peekStack(0));
    global->setAttr(ident, value);
    return true;
}

bool InstrGetAttr::execute(Interpreter& interp)
{
    Root<Value> value(interp.popStack());
    Root<Value> result;
    if (!value.maybeGetAttr(ident, result))
        return raiseAttrError(value, interp);
    assert(result != Value(UninitializedSlot));
    interp.pushStack(result);
    return true;
}

bool InstrSetAttr::execute(Interpreter& interp)
{
    Root<Value> value(interp.peekStack(1));
    interp.popStack().toObject()->setAttr(ident, value);
    return true;
}

bool InstrGetMethod::execute(Interpreter& interp)
{
    Root<Value> value(interp.popStack());
    Root<Value> result;
    if (!value.maybeGetAttr(ident, result))
        return raiseAttrError(value, interp);
    assert(result != Value(UninitializedSlot));
    interp.pushStack(result);
    interp.pushStack(value);

    if (value.isInt32())
        interp.replaceInstr(gc::create<InstrGetMethodInt>(ident, result), this);
    else
        interp.replaceInstr(gc::create<InstrGetMethodFallback>(ident), this);

    return true;
}

bool InstrGetMethodInt::execute(Interpreter& interp)
{
    Root<Value> value(interp.popStack());
    if (!value.isInt32()) {
        interp.replaceInstr(gc::create<InstrGetMethodFallback>(ident), this);
        Root<Value> result;
        if (!value.maybeGetAttr(ident, result))
            return raiseAttrError(value, interp);
        assert(result != Value(UninitializedSlot));
        interp.pushStack(result);
    } else {
        interp.pushStack(result_);
    }

    interp.pushStack(value);
    return true;
}

bool InstrGetMethodFallback::execute(Interpreter& interp)
{
    Root<Value> value(interp.popStack());
    Root<Value> result;
    if (!value.maybeGetAttr(ident, result))
        return raiseAttrError(value, interp);
    assert(result != Value(UninitializedSlot));
    interp.pushStack(result);
    interp.pushStack(value);
    return true;
}

bool InstrCall::execute(Interpreter& interp)
{
    Root<Value> target(interp.peekStack(count));
    TracedVector<Value> args = interp.stackSlice(count - 1, count);
    interp.popStack(count + 1);
    return interp.startCall(target, args);
}

bool InstrReturn::execute(Interpreter& interp)
{
    Value value = interp.popStack();
    interp.popFrame();
    interp.pushStack(value);
    return true;
}

bool InstrIn::execute(Interpreter& interp)
{
    // todo: implement this
    // https://docs.python.org/2/reference/expressions.html#membership-test-details

    Root<Object*> container(interp.popStack().toObject());
    Root<Value> value(interp.popStack());
    Root<Value> contains;
    if (!container->maybeGetAttr("__contains__", contains)) {
        interp.pushStack(gc::create<Exception>("TypeError", "Argument is not iterable"));
        return false;
    }

    RootVector<Value> args;
    args.push_back(Value(container));
    args.push_back(value);
    return interp.startCall(contains, args);
}

bool InstrIs::execute(Interpreter& interp)
{
    Value b = interp.popStack();
    Value a = interp.popStack();
    bool result = a.toObject() == b.toObject();
    interp.pushStack(Boolean::get(result));
    return true;
}

bool InstrNot::execute(Interpreter& interp)
{
    // the following values are interpreted as false: False, None, numeric zero
    // of all types, and empty strings and containers (including strings,
    // tuples, lists, dictionaries, sets and frozensets).

    Object* obj = interp.popStack().toObject();
    interp.pushStack(Boolean::get(!obj->isTrue()));
    return true;
}

bool InstrBranchAlways::execute(Interpreter& interp) {
    assert(offset_);
    interp.branch(offset_);
    return true;
}

bool InstrBranchIfTrue::execute(Interpreter& interp)
{
    assert(offset_);
    Object *x = interp.popStack().toObject();
    if (x->isTrue())
        interp.branch(offset_);
    return true;
}

bool InstrBranchIfFalse::execute(Interpreter& interp)
{
    assert(offset_);
    Object *x = interp.popStack().toObject();
    if (!x->isTrue())
        interp.branch(offset_);
    return true;
}

bool InstrOr::execute(Interpreter& interp)
{
    // The expression |x or y| first evaluates x; if x is true, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    assert(offset_);
    Object *x = interp.peekStack(0).toObject();
    if (x->isTrue()) {
        interp.branch(offset_);
        return true;
    }
    interp.popStack();
    return true;
}

bool InstrAnd::execute(Interpreter& interp)
{
    // The expression |x and y| first evaluates x; if x is false, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    assert(offset_);
    Object *x = interp.peekStack(0).toObject();
    if (!x->isTrue()) {
        interp.branch(offset_);
        return true;
    }
    interp.popStack();
    return true;
}

bool InstrLambda::execute(Interpreter& interp)
{
    Root<Block*> block(block_);
    Root<Frame*> frame(interp.getFrame(0));
    interp.pushStack(gc::create<Function>(params_, block, frame));
    return true;
}

bool InstrDup::execute(Interpreter& interp)
{
    interp.pushStack(interp.peekStack(index_));
    return true;
}

bool InstrPop::execute(Interpreter& interp)
{
    interp.popStack();
    return true;
}

bool InstrSwap::execute(Interpreter& interp)
{
    interp.swapStack();
    return true;
}

bool InstrTuple::execute(Interpreter& interp)
{
    Tuple* tuple = Tuple::get(interp.stackSlice(size - 1, size));
    interp.popStack(size);
    interp.pushStack(tuple);
    return true;
}

bool InstrList::execute(Interpreter& interp)
{
    List* list = gc::create<List>(interp.stackSlice(size - 1, size));
    interp.popStack(size);
    interp.pushStack(list);
    return true;
}

bool InstrDict::execute(Interpreter& interp)
{
    Dict* dict = gc::create<Dict>(interp.stackSlice(size * 2 - 1, size * 2));
    interp.popStack(size * 2);
    interp.pushStack(dict);
    return true;
}

bool InstrAssertionFailed::execute(Interpreter& interp)
{
    Object* obj = interp.popStack().toObject();
    assert(obj->is<String>() || obj == None);
    string message = obj != None ? obj->as<String>()->value() : "";
    interp.pushStack(gc::create<Exception>("AssertionError", message));
    return false;
}

bool InstrMakeClassFromFrame::execute(Interpreter& interp)
{
    Root<Frame*> frame(interp.getFrame());
    vector<Name> names;
    for (Layout* l = frame->layout(); l != Frame::InitialLayout; l = l->parent())
        names.push_back(l->name());
    Root<Layout*> layout(Object::InitialLayout);
    for (auto i = names.begin(); i != names.end(); i++)
        layout = layout->addName(*i);
    Class* cls = gc::create<Class>(ident, layout);
    Root<Value> value;
    for (auto i = names.begin(); i != names.end(); i++) {
        value = frame->getAttr(*i);
        cls->setAttr(*i, value);
    }
    interp.pushStack(cls);
    return true;
}

bool InstrDestructure::execute(Interpreter& interp)
{
    Root<Value> seq(interp.popStack());
    Root<Value> lenFunc;
    if (!seq.get().maybeGetAttr("__len__", lenFunc)) {
        interp.pushStack(gc::create<Exception>("TypeError",
                                               "Argument is not iterable"));
        return false;
    }

    Root<Value> getitemFunc;
    if (!seq.get().maybeGetAttr("__getitem__", getitemFunc)) {
        interp.pushStack(gc::create<Exception>("TypeError",
                                               "Argument is not iterable"));
        return false;
    }

    RootVector<Value> args;
    args.push_back(seq);
    Root<Value> result;
    if (!interp.call(lenFunc, args, result)) {
        interp.pushStack(result);
        return false;
    }

    if (!result.get().isInt32()) {
        interp.pushStack(gc::create<Exception>("TypeError",
                                               "__len__ didn't return an integer"));
        return false;
    }

    if (result.get().asInt32() != count_) {
        interp.pushStack(gc::create<Exception>("ValueError",
                                               "too many values to unpack"));
        return false;
    }

    args.resize(2);
    for (unsigned i = count_; i != 0; i--) {
        args[1] = Integer::get(i - 1);
        bool ok = interp.call(getitemFunc, args, result);
        interp.pushStack(result);
        if (!ok)
            return false;
    }

    return true;
}

bool InstrRaise::execute(Interpreter& interp)
{
    // todo: exceptions must be old-style classes or derived from BaseException
    return false;
}

bool InstrIteratorNext::execute(Interpreter& interp)
{
    // The stack is already set up with next method and target on top
    Root<Value> target(interp.peekStack(1));
    TracedVector<Value> args = interp.stackSlice(0, 1);
    Root<Value> result;
    bool ok = interp.call(target, args, result);
    if (!ok) {
        Root<Object*> exc(result.get().toObject());
        if (exc->is<StopIteration>()) {
            interp.pushStack(Boolean::False);
            return true;
        }
    }
    interp.pushStack(result);
    if (ok)
        interp.pushStack(Boolean::True);
    return ok;
}

bool InstrAugAssignLocal::execute(Interpreter& interp)
{
    Root<Value> value(interp.getFrame()->getAttr(ident));
    
    Root<Value> value(interp.peekStack(0));
    
    interp.getFrame()->setAttr(ident, value);
    return true;
}

bool InstrAugAssignLexical::execute(Interpreter& interp)
{
    Root<Value> value(interp.peekStack(0));
    interp.getFrame(frameIndex)->setAttr(ident, value);
    return true;
}

bool InstrAugAssignGlobal::execute(Interpreter& interp)
{
    Root<Value> value(interp.peekStack(0));
    global->setAttr(ident, value);
    return true;
}
