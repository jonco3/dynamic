#include "instr.h"

#include "builtin.h"
#include "generator.h"

bool InstrConst::execute(Interpreter& interp)
{
    interp.pushStack(value);
    return true;
}

// GetLocal: name was present when compiled, but may have been deleted.

bool InstrGetLocal::execute(Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    if (frame->layout() != layout_) {
        Root<InstrGetLocal*> self(this);
        return interp.replaceInstrAndRestart(
            this, gc.create<InstrGetLocalFallback>(self));
    }

    interp.pushStack(frame->getSlot(slot_));
    return true;
}

bool InstrGetLocalFallback::execute(Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    Root<Value> value;
    if (!frame->maybeGetAttr(ident, value))
        return interp.raiseNameError(ident);
    interp.pushStack(value);
    return true;
}

bool InstrSetLocal::execute(Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    if (frame->layout() != layout_) {
        Root<InstrSetLocal*> self(this);
        return interp.replaceInstrAndRestart(
            this, gc.create<InstrSetLocalFallback>(self));
    }

    Root<Value> value(interp.peekStack(0));
    interp.getFrame()->setSlot(slot_, value);
    return true;
}

bool InstrSetLocalFallback::execute(Interpreter& interp)
{
    Root<Value> value(interp.peekStack(0));
    interp.getFrame()->setAttr(ident, value);
    return true;
}

bool InstrDelLocal::execute(Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    // Name was present when compiled, but may have been deleted.
    if (!frame->maybeDelOwnAttr(ident))
        return interp.raiseNameError(ident);
    return true;
}

bool InstrGetLexical::execute(Interpreter& interp)
{
    Frame* frame = interp.getFrame(frameIndex);
    Root<Value> value;
    // Name was present when compiled, but may have been deleted.
    if (!frame->maybeGetAttr(ident, value))
        return interp.raiseNameError(ident);
    interp.pushStack(value);
    return true;
}

bool InstrSetLexical::execute(Interpreter& interp)
{
    Root<Value> value(interp.peekStack(0));
    interp.getFrame(frameIndex)->setAttr(ident, value);
    return true;
}

bool InstrDelLexical::execute(Interpreter& interp)
{
    Frame* frame = interp.getFrame(frameIndex);
    if (!frame->maybeDelOwnAttr(ident))
        return interp.raiseNameError(ident);
    return true;
}

bool InstrGetGlobal::execute(Interpreter& interp)
{
    Root<Value> value;
    if (global->maybeGetAttr(ident, value)) {
        assert(value.toObject());
        interp.pushStack(value);
        return true;
    }

    Root<Value> builtins;
    if (global->maybeGetAttr("__builtins__", builtins) &&
        builtins.maybeGetAttr(ident, value))
    {
        assert(value.toObject());
        interp.pushStack(value);
        return true;
    }

    return interp.raiseNameError(ident);
}

bool InstrSetGlobal::execute(Interpreter& interp)
{
    Root<Value> value(interp.peekStack(0));
    global->setAttr(ident, value);
    return true;
}

bool InstrDelGlobal::execute(Interpreter& interp)
{
    if (!global->maybeDelOwnAttr(ident))
        return interp.raiseNameError(ident);
    return true;
}

bool InstrGetAttr::execute(Interpreter& interp)
{
    Root<Value> value(interp.popStack());
    Root<Value> result;
    if (!value.maybeGetAttr(ident, result))
        return interp.raiseAttrError(value, ident);
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

bool InstrDelAttr::execute(Interpreter& interp)
{
    Root<Value> value(interp.popStack());
    if (!value.toObject()->maybeDelOwnAttr(ident))
        return interp.raiseAttrError(value, ident);
    return true;
}

bool InstrGetMethod::execute(Interpreter& interp)
{
    Root<Value> value(interp.popStack());
    Root<Value> result;
    if (!value.maybeGetAttr(ident, result))
        return interp.raiseAttrError(value, ident);
    assert(result != Value(UninitializedSlot));
    interp.pushStack(result);
    interp.pushStack(value);

    if (value.isInt32())
        interp.replaceInstr(this, gc.create<InstrGetMethodInt>(ident, result));
    else
        interp.replaceInstr(this, gc.create<InstrGetMethodFallback>(ident));

    return true;
}

bool InstrGetMethodInt::execute(Interpreter& interp)
{
    Root<Value> value(interp.peekStack(0));
    if (!value.isInt32()) {
        Instr *fallback = gc.create<InstrGetMethodFallback>(ident);
        return interp.replaceInstrAndRestart(this, fallback);
    }

    interp.popStack();
    interp.pushStack(result_);
    interp.pushStack(value);
    return true;
}

bool InstrGetMethodFallback::execute(Interpreter& interp)
{
    Root<Value> value(interp.popStack());
    Root<Value> result;
    if (!value.maybeGetAttr(ident, result))
        return interp.raiseAttrError(value, ident);
    assert(result != Value(UninitializedSlot));
    interp.pushStack(result);
    interp.pushStack(value);
    return true;
}

bool InstrCall::execute(Interpreter& interp)
{
    Root<Value> target(interp.peekStack(count));
    RootVector<Value> args(count);
    for (unsigned i = 0; i < count; i++)
        args[i] = interp.peekStack(count - i - 1);
    interp.popStack(count + 1);
    return interp.startCall(target, args);
}

bool InstrReturn::execute(Interpreter& interp)
{
    Value value = interp.popStack();
    interp.returnFromFrame(value);
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
        interp.pushStack(gc.create<TypeError>("Argument is not iterable"));
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

InstrLambda::InstrLambda(Name name, const vector<Name>& paramNames,
                         Traced<Block*> block, unsigned defaultCount,
                         bool takesRest, bool isGenerator)
  : funcName_(name),
    info_(gc.create<FunctionInfo>(paramNames, block, defaultCount, takesRest,
                                  isGenerator))
{}

bool InstrLambda::execute(Interpreter& interp)
{
    Root<FunctionInfo*> info(info_);
    Root<Block*> block(this->block());
    Root<Frame*> frame(interp.getFrame(0));
    TracedVector<Value> defaults(
        interp.stackSlice(defaultCount()));
    Object* obj = gc.create<Function>(funcName_, info, defaults, frame);
    interp.popStack(defaultCount());
    interp.pushStack(Value(obj));
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
    Tuple* tuple = Tuple::get(interp.stackSlice(size));
    interp.popStack(size);
    interp.pushStack(tuple);
    return true;
}

bool InstrList::execute(Interpreter& interp)
{
    List* list = gc.create<List>(interp.stackSlice(size));
    interp.popStack(size);
    interp.pushStack(list);
    return true;
}

bool InstrDict::execute(Interpreter& interp)
{
    Dict* dict = gc.create<Dict>(interp.stackSlice(size * 2));
    interp.popStack(size * 2);
    interp.pushStack(dict);
    return true;
}

bool InstrSlice::execute(Interpreter& interp)
{
    Slice* slice = gc.create<Slice>(interp.stackSlice(3));
    interp.popStack(3);
    interp.pushStack(slice);
    return true;
}

bool InstrAssertionFailed::execute(Interpreter& interp)
{
    Object* obj = interp.popStack().toObject();
    assert(obj->is<String>() || obj == None);
    string message = obj != None ? obj->as<String>()->value() : "";
    interp.pushStack(gc.create<AssertionError>(message));
    return false;
}

bool InstrMakeClassFromFrame::execute(Interpreter& interp)
{
    Root<Frame*> frame(interp.getFrame());
    vector<Name> names;
    for (Layout* l = frame->layout();
         l != Frame::InitialLayout;
         l = l->parent())
    {
        Name name = l->name();
        if (name != "__bases__")
            names.push_back(l->name());
    }
    Root<Layout*> layout(Class::InitialLayout);
    for (auto i = names.begin(); i != names.end(); i++)
        layout = layout->addName(*i);
    Root<Value> bases;
    if (!frame->maybeGetAttr("__bases__", bases)) {
        interp.pushStack(gc.create<AttributeError>("Missing __bases__"));
        return false;
    }
    if (!bases.toObject()->is<Tuple>()) {
        interp.pushStack(gc.create<TypeError>("__bases__ is not a tuple"));
        return false;
    }
    Root<Tuple*> tuple(bases.toObject()->as<Tuple>());
    Root<Class*> base(Object::ObjectClass);
    if (tuple->len() > 1) {
        interp.pushStack(gc.create<NotImplementedError>(
                             "Multiple inheritance not NYI"));
        return false;
    } else if (tuple->len() == 1) {
        Root<Value> value(tuple->getitem(0));
        if (!value.toObject()->is<Class>()) {
            interp.pushStack(gc.create<TypeError>("__bases__[0] is not a class"));
            return false;
        }
        base = value.toObject()->as<Class>();
    }
    Class* cls = gc.create<Class>(ident, base, layout);
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
    if (!seq.maybeGetAttr("__len__", lenFunc)) {
        interp.pushStack(gc.create<TypeError>("Argument is not iterable"));
        return false;
    }

    Root<Value> getitemFunc;
    if (!seq.maybeGetAttr("__getitem__", getitemFunc)) {
        interp.pushStack(gc.create<TypeError>("Argument is not iterable"));
        return false;
    }

    RootVector<Value> args;
    args.push_back(seq);
    Root<Value> result;
    if (!interp.call(lenFunc, args, result)) {
        interp.pushStack(result);
        return false;
    }

    if (!result.isInt32()) {
        interp.pushStack(gc.create<TypeError>("__len__ didn't return an integer"));
        return false;
    }

    int32_t len = result.asInt32();
    if (len < 0) {
        interp.pushStack(gc.create<ValueError>("__len__ returned negative value"));
        return false;
    }
    if (size_t(len) != count_) {
        interp.pushStack(gc.create<ValueError>("too many values to unpack"));
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
    TracedVector<Value> args = interp.stackSlice(1);
    Root<Value> result;
    bool ok = interp.call(target, args, result);
    if (!ok) {
        Root<Object*> exc(result.toObject());
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

void BinaryOpInstr::print(ostream& s) const
{
    s << name() << " " << BinaryOpNames[op()];
}

bool InstrBinaryOp::execute(Interpreter& interp)
{
    Root<Value> right(interp.peekStack(0));
    Root<Value> left(interp.peekStack(1));

    if (left.isInt32() && right.isInt32()) {
        Root<Value> method(
            Integer::ObjectClass->getAttr(BinaryOpMethodNames[op()]));
        Instr* instr = gc.create<InstrBinaryOpInt>(op(), method);
        return interp.replaceInstrAndRestart(this, instr);
    } else {
        Instr* instr = gc.create<InstrBinaryOpFallback>(op());
        return interp.replaceInstrAndRestart(this, instr);
    }
}

bool InstrBinaryOpInt::execute(Interpreter& interp)
{
    Root<Value> right(interp.peekStack(0));
    Root<Value> left(interp.peekStack(1));

    if (!left.isInt32() || !right.isInt32()) {
        Instr* instr = gc.create<InstrBinaryOpFallback>(op());
        return interp.replaceInstrAndRestart(this, instr);
    }

    interp.popStack(2);
    RootVector<Value> args;
    args.push_back(left);
    args.push_back(right);
    Root<Value> result;
    Root<Value> method(method_);
    bool r = interp.call(method, args, result);
    assert(r);
    assert(result != Value(NotImplemented));
    interp.pushStack(result);
    return r;
}

static bool maybeCallBinaryOp(Interpreter& interp,
                              Traced<Value> obj,
                              Name name,
                              Traced<Value> left,
                              Traced<Value> right,
                              bool& successOut)
{
    Root<Value> method;
    if (!obj.maybeGetAttr(name, method))
        return false;

    Root<Value> result;
    RootVector<Value> args;
    args.push_back(left);
    args.push_back(right);
    if (!interp.call(method, args, result)) {
        interp.pushStack(result);
        successOut = false;
        return true;
    }

    if (result == Value(NotImplemented))
        return false;

    interp.pushStack(result);
    successOut = true;
    return true;
}

bool InstrBinaryOpFallback::execute(Interpreter& interp)
{
    Root<Value> right(interp.popStack());
    Root<Value> left(interp.popStack());

    // todo: "If the right operand's type is a subclass of the left operand's
    // type and that subclass provides the reflected method for the operation,
    // this method will be called before the left operand's non-reflected
    // method. This behavior allows subclasses to override their ancestors'
    // operations."

    Root<Value> method;
    bool success;
    if (maybeCallBinaryOp(interp, left, BinaryOpMethodNames[op()],
                          left, right, success))
    {
        return success;
    }

    if (maybeCallBinaryOp(interp, right, BinaryOpReflectedMethodNames[op()],
                          right, left, success))
    {
        return success;
    }

    interp.pushStack(gc.create<TypeError>(
                         "unsupported operand type(s) for binary operation"));
    return false;
}

bool InstrAugAssignUpdate::execute(Interpreter& interp)
{
    Root<Value> update(interp.popStack());
    Root<Value> value(interp.popStack());

    Root<Value> method;
    Root<Value> result;
    if (value.maybeGetAttr(AugAssignMethodNames[op()], method)) {
        RootVector<Value> args;
        args.push_back(value);
        args.push_back(update);
        if (!interp.call(method, args, result)) {
            interp.pushStack(result);
            return false;
        }
        interp.pushStack(result);
        return true;
    } else if (value.maybeGetAttr(BinaryOpMethodNames[op()], method)) {
        RootVector<Value> args;
        args.push_back(value);
        args.push_back(update);
        if (!interp.call(method, args, result)) {
            interp.pushStack(result);
            return false;
        }
        interp.pushStack(result);
        return true;
    } else {
        interp.pushStack(gc.create<TypeError>(
                             "unsupported operand type(s) for augmented assignment"));
        return false;
    }
}

bool InstrResumeGenerator::execute(Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    GeneratorIter *gen = frame->getAttr("self").asObject()->as<GeneratorIter>();
    return gen->resume(interp);
}

bool InstrLeaveGenerator::execute(Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    GeneratorIter *gen = frame->getAttr("%gen").asObject()->as<GeneratorIter>();
    return gen->leave(interp);
}

bool InstrSuspendGenerator::execute(Interpreter& interp)
{
    Root<Value> value(interp.popStack());
    Frame* frame = interp.getFrame();
    GeneratorIter *gen = frame->getAttr("%gen").asObject()->as<GeneratorIter>();
    gen->suspend(interp, value);
    return true;
}

bool InstrEnterCatchRegion::execute(Interpreter& interp)
{
    interp.pushExceptionHandler(ExceptionHandler::CatchHandler, offset_);
    return true;
}

bool InstrLeaveCatchRegion::execute(Interpreter& interp)
{
    interp.popExceptionHandler(ExceptionHandler::CatchHandler);
    return true;
}

bool InstrMatchCurrentException::execute(Interpreter& interp)
{
    Root<Object*> obj(interp.popStack().toObject());
    Root<Exception*> exception(interp.currentException());
    bool match = obj->is<Class>() && exception->isInstanceOf(obj->as<Class>());
    if (match) {
        interp.finishHandlingException();
        interp.pushStack(exception);
    }
    interp.pushStack(Boolean::get(match));
    return true;
}

bool InstrHandleCurrentException::execute(Interpreter& interp)
{
    interp.finishHandlingException();
    return true;
}

bool InstrEnterFinallyRegion::execute(Interpreter& interp)
{
    interp.pushExceptionHandler(ExceptionHandler::FinallyHandler, offset_);
    return true;
}

bool InstrLeaveFinallyRegion::execute(Interpreter& interp)
{
    interp.popExceptionHandler(ExceptionHandler::FinallyHandler);
    return true;
}

bool InstrFinishExceptionHandler::execute(Interpreter& interp)
{
    return interp.maybeContinueHandlingException();
}

bool InstrLoopControlJump::execute(Interpreter& interp)
{
    interp.loopControlJump(finallyCount_, target_);
    return true;
}

bool InstrAssertStackDepth::execute(Interpreter& interp)
{
#ifdef DEBUG
    unsigned actual = interp.frameStackDepth();
    if (actual != expected_) {
        cerr << "Excpected stack depth " << dec << expected_;
        cerr << " but got " << actual << " in: " << endl;
        cerr << *interp.getFrame()->block() << endl;
        assert(actual == expected_);
    }
#endif
    return true;
}
