#include "instr.h"

#include "builtin.h"
#include "generator.h"

/* static */ bool InstrConst::execute(Traced<InstrConst*> self, Interpreter& interp)
{
    interp.pushStack(self->value);
    return true;
}

// GetLocal: name was present when compiled, but may have been deleted.

/* static */ bool InstrGetLocal::execute(Traced<InstrGetLocal*> self,
                                         Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    if (frame->layout() != self->layout_)
        return interp.replaceInstrFuncAndRestart(self, fallback);

    interp.pushStack(frame->getSlot(self->slot_));
    return true;
}

/* static */ bool InstrGetLocal::fallback(Traced<InstrGetLocal*> self,
                                          Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    Stack<Value> value;
    if (!frame->maybeGetAttr(self->ident, value))
        return interp.raiseNameError(self->ident);
    interp.pushStack(value);
    return true;
}

/* static */ bool InstrSetLocal::execute(Traced<InstrSetLocal*> self,
                                         Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    if (frame->layout() != self->layout_)
        return interp.replaceInstrFuncAndRestart(self, fallback);

    Stack<Value> value(interp.peekStack(0));
    interp.getFrame()->setSlot(self->slot_, value);
    return true;
}

/* static */ bool InstrSetLocal::fallback(Traced<InstrSetLocal*> self,
                                          Interpreter& interp)
{
    Stack<Value> value(interp.peekStack(0));
    interp.getFrame()->setAttr(self->ident, value);
    return true;
}

/* static */ bool InstrDelLocal::execute(Traced<InstrDelLocal*> self,
                                         Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    // Name was present when compiled, but may have been deleted.
    if (!frame->maybeDelOwnAttr(self->ident))
        return interp.raiseNameError(self->ident);
    return true;
}

/* static */ bool InstrGetLexical::execute(Traced<InstrGetLexical*> self,
                                           Interpreter& interp)
{
    Frame* frame = interp.getFrame(self->frameIndex);
    Stack<Value> value;
    // Name was present when compiled, but may have been deleted.
    if (!frame->maybeGetAttr(self->ident, value))
        return interp.raiseNameError(self->ident);
    interp.pushStack(value);
    return true;
}

/* static */ bool InstrSetLexical::execute(Traced<InstrSetLexical*> self,
                                           Interpreter& interp)
{
    Stack<Value> value(interp.peekStack(0));
    interp.getFrame(self->frameIndex)->setAttr(self->ident, value);
    return true;
}

/* static */ bool InstrDelLexical::execute(Traced<InstrDelLexical*> self,
                                           Interpreter& interp)
{
    Frame* frame = interp.getFrame(self->frameIndex);
    if (!frame->maybeDelOwnAttr(self->ident))
        return interp.raiseNameError(self->ident);
    return true;
}

/* static */ bool InstrGetGlobal::execute(Traced<InstrGetGlobal*> self,
                                          Interpreter& interp)
{
    Stack<Value> value;
    if (self->global->maybeGetAttr(self->ident, value)) {
        assert(value.toObject());
        interp.pushStack(value);
        return true;
    }

    Stack<Value> builtins;
    if (self->global->maybeGetAttr(Name::__builtins__, builtins) &&
        builtins.maybeGetAttr(self->ident, value))
    {
        assert(value.toObject());
        interp.pushStack(value);
        return true;
    }

    return interp.raiseNameError(self->ident);
}

/* static */ bool InstrSetGlobal::execute(Traced<InstrSetGlobal*> self,
                                          Interpreter& interp)
{
    Stack<Value> value(interp.peekStack(0));
    self->global->setAttr(self->ident, value);
    return true;
}

/* static */ bool InstrDelGlobal::execute(Traced<InstrDelGlobal*> self,
                                          Interpreter& interp)
{
    if (!self->global->maybeDelOwnAttr(self->ident))
        return interp.raiseNameError(self->ident);
    return true;
}

/* static */ bool InstrGetAttr::execute(Traced<InstrGetAttr*> self,
                                        Interpreter& interp)
{
    Stack<Value> value(interp.popStack());
    Stack<Value> result;
    bool ok = getAttr(value, self->ident, result);
    interp.pushStack(result);
    return ok;
}

/* static */ bool InstrSetAttr::execute(Traced<InstrSetAttr*> self,
                                        Interpreter& interp)
{
    Stack<Value> value(interp.peekStack(1));
    Stack<Object*> obj(interp.popStack().toObject());
    Stack<Value> result;
    if (!setAttr(obj, self->ident, value, result)) {
        interp.pushStack(result);
        return false;
    }

    return true;
}

/* static */ bool InstrDelAttr::execute(Traced<InstrDelAttr*> self,
                                        Interpreter& interp)
{
    Stack<Object*> obj(interp.popStack().toObject());
    Stack<Value> result;
    if (!delAttr(obj, self->ident, result)) {
        interp.pushStack(result);
        return false;
    }

    return true;
}

/* static */ bool InstrCall::execute(Traced<InstrCall*> self,
                                     Interpreter& interp)
{
    Stack<Value> target(interp.peekStack(self->count_));
    // todo: evil performance hack to avoid allocating memory
    if (self->count_ <= 6) {
        RootArray<Value, 6> args;
        for (unsigned i = 0; i < self->count_; i++)
            args[i] = interp.peekStack(self->count_ - i - 1);
        interp.popStack(self->count_ + 1);
        return interp.startCall(target,
                                TracedVector<Value>(args, 0, self->count_));
    } else {
        RootVector<Value> args(self->count_);
        for (unsigned i = 0; i < self->count_; i++)
            args[i] = interp.peekStack(self->count_ - i - 1);
        interp.popStack(self->count_ + 1);
        return interp.startCall(target, args);
    }
}

/*
 * GetMethod/CallMethod instrunctions are an optimisation of GetAttr/Call for
 * method invocation which avoids creating a method wrapper object for every
 * call.
 *
 * GetMethod looks up an attribute on a value like GetAttr, but if the attriubte
 * turns out to be a function or native descriptor it doesn't call its __get__
 * method.  We push a boolean flag value to indicate that this case, and also we
 * always push the input value.  CallMethod checks the flag and uses the input
 * value as the self parameter if necessary by simply incrementing the argument
 * count.
 */
/* static */ bool InstrGetMethod::execute(Traced<InstrGetMethod*> self,
                                          Interpreter& interp)
{
    Stack<Value> value(interp.peekStack(0));
    if (!fallback(self, interp))
        return false;

    if (value.isInt()) {
        Stack<Value> result(interp.peekStack(2));
        interp.replaceInstr(self,
                            InstrGetMethodInt::execute,
                            gc.create<InstrGetMethodInt>(self->ident, result));
    } else {
        interp.replaceInstrFunc(self, fallback);
    }

    return true;
}

/* static */ bool InstrGetMethod::fallback(Traced<InstrGetMethod*> self,
                                           Interpreter& interp)
{
    Stack<Value> value(interp.popStack());
    Stack<Value> result;
    bool isCallableDescriptor;
    if (!getMethodAttr(value, self->ident, result, isCallableDescriptor))
        return interp.raiseAttrError(value, self->ident);

    interp.pushStack(result);
    interp.pushStack(Boolean::get(isCallableDescriptor));
    interp.pushStack(value);
    return true;
}

/* static */ bool InstrGetMethodInt::execute(Traced<InstrGetMethodInt*> self,
                                             Interpreter& interp)
{
    Stack<Value> value(interp.peekStack(0));
    if (!value.isInt()) {
        return interp.replaceInstrAndRestart(
            self,
            InstrGetMethod::fallback,
            gc.create<InstrGetMethod>(self->ident));
    }

    interp.popStack();
    interp.pushStack(self->result_);
    interp.pushStack(Boolean::True);
    interp.pushStack(value);
    return true;
}

/* static */ bool InstrCallMethod::execute(Traced<InstrCallMethod*> self,
                                           Interpreter& interp)
{
    bool addSelf = interp.peekStack(self->count_ + 1).as<Boolean>()->value();
    Stack<Value> target(interp.peekStack(self->count_ + 2));
    unsigned argCount = self->count_ + (addSelf ? 1 : 0);
    // todo: evil performance hack to avoid allocating memory
    if (argCount <= 6) {
        RootArray<Value, 6> args;
        for (unsigned i = 0; i < argCount; i++)
            args[i] = interp.peekStack(argCount - i - 1);
        interp.popStack(self->count_ + 3);
        return interp.startCall(target, TracedVector<Value>(args, 0, argCount));
    } else {
        RootVector<Value> args(argCount);
        for (unsigned i = 0; i < argCount; i++)
            args[i] = interp.peekStack(argCount - i - 1);
        interp.popStack(self->count_ + 3);
        return interp.startCall(target, args);
    }
}

/* static */ bool InstrReturn::execute(Traced<InstrReturn*> self,
                                       Interpreter& interp)
{
    Value value = interp.popStack();
    interp.returnFromFrame(value);
    return true;
}

/* static */ bool InstrIn::execute(Traced<InstrIn*> self, Interpreter& interp)
{
    // todo: implement this
    // https://docs.python.org/2/reference/expressions.html#membership-test-details

    Stack<Object*> container(interp.popStack().toObject());
    Stack<Value> value(interp.popStack());
    Stack<Value> contains;
    if (!container->maybeGetAttr(Name::__contains__, contains)) {
        interp.pushStack(gc.create<TypeError>("Argument is not iterable"));
        return false;
    }

    RootArray<Value, 2> args;
    args[0] = Value(container);
    args[1] = value;
    return interp.startCall(contains, args);
}

/* static */ bool InstrIs::execute(Traced<InstrIs*> self, Interpreter& interp)
{
    Value b = interp.popStack();
    Value a = interp.popStack();
    bool result = a.toObject() == b.toObject();
    interp.pushStack(Boolean::get(result));
    return true;
}

/* static */ bool InstrNot::execute(Traced<InstrNot*> self, Interpreter& interp)
{
    // the following values are interpreted as false: False, None, numeric zero
    // of all types, and empty strings and containers (including strings,
    // tuples, lists, dictionaries, sets and frozensets).

    Object* obj = interp.popStack().toObject();
    interp.pushStack(Boolean::get(!obj->isTrue()));
    return true;
}

/* static */ bool InstrBranchAlways::execute(Traced<InstrBranchAlways*> self,
                                             Interpreter& interp) {
    assert(self->offset_);
    interp.branch(self->offset_);
    return true;
}

/* static */ bool InstrBranchIfTrue::execute(Traced<InstrBranchIfTrue*> self,
                                             Interpreter& interp)
{
    assert(self->offset_);
    Object *x = interp.popStack().toObject();
    if (x->isTrue())
        interp.branch(self->offset_);
    return true;
}

/* static */ bool InstrBranchIfFalse::execute(Traced<InstrBranchIfFalse*> self,
                                              Interpreter& interp)
{
    assert(self->offset_);
    Object *x = interp.popStack().toObject();
    if (!x->isTrue())
        interp.branch(self->offset_);
    return true;
}

/* static */ bool InstrOr::execute(Traced<InstrOr*> self, Interpreter& interp)
{
    // The expression |x or y| first evaluates x; if x is true, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    assert(self->offset_);
    Object *x = interp.peekStack(0).toObject();
    if (x->isTrue()) {
        interp.branch(self->offset_);
        return true;
    }
    interp.popStack();
    return true;
}

/* static */ bool InstrAnd::execute(Traced<InstrAnd*> self, Interpreter& interp)
{
    // The expression |x and y| first evaluates x; if x is false, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    assert(self->offset_);
    Object *x = interp.peekStack(0).toObject();
    if (!x->isTrue()) {
        interp.branch(self->offset_);
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

/* static */ bool InstrLambda::execute(Traced<InstrLambda*> self,
                                       Interpreter& interp)
{
    Stack<FunctionInfo*> info(self->info_);
    Stack<Block*> block(self->block());
    Stack<Frame*> frame(interp.getFrame(0));
    TracedVector<Value> defaults(
        interp.stackSlice(self->defaultCount()));
    Object* obj = gc.create<Function>(self->funcName_, info, defaults, frame);
    interp.popStack(self->defaultCount());
    interp.pushStack(Value(obj));
    return true;
}

/* static */ bool InstrDup::execute(Traced<InstrDup*> self, Interpreter& interp)
{
    interp.pushStack(interp.peekStack(self->index_));
    return true;
}

/* static */ bool InstrPop::execute(Traced<InstrPop*> self, Interpreter& interp)
{
    interp.popStack();
    return true;
}

/* static */ bool InstrSwap::execute(Traced<InstrSwap*> self,
                                     Interpreter& interp)
{
    interp.swapStack();
    return true;
}

/* static */ bool InstrTuple::execute(Traced<InstrTuple*> self,
                                      Interpreter& interp)
{
    Tuple* tuple = Tuple::get(interp.stackSlice(self->size));
    interp.popStack(self->size);
    interp.pushStack(tuple);
    return true;
}

/* static */ bool InstrList::execute(Traced<InstrList*> self,
                                     Interpreter& interp)
{
    List* list = gc.create<List>(interp.stackSlice(self->size));
    interp.popStack(self->size);
    interp.pushStack(list);
    return true;
}

/* static */ bool InstrDict::execute(Traced<InstrDict*> self,
                                     Interpreter& interp)
{
    Dict* dict = gc.create<Dict>(interp.stackSlice(self->size * 2));
    interp.popStack(self->size * 2);
    interp.pushStack(dict);
    return true;
}

/* static */ bool InstrSlice::execute(Traced<InstrSlice*> self,
                                      Interpreter& interp)
{
    Slice* slice = gc.create<Slice>(interp.stackSlice(3));
    interp.popStack(3);
    interp.pushStack(slice);
    return true;
}

/* static */ bool
InstrAssertionFailed::execute(Traced<InstrAssertionFailed*> self,
                              Interpreter& interp)
{
    Object* obj = interp.popStack().toObject();
    assert(obj->is<String>() || obj == None);
    string message = obj != None ? obj->as<String>()->value() : "";
    interp.pushStack(gc.create<AssertionError>(message));
    return false;
}

/* static */ bool
InstrMakeClassFromFrame::execute(Traced<InstrMakeClassFromFrame*> self,
                                 Interpreter& interp)
{
    Stack<Frame*> frame(interp.getFrame());
    vector<Name> names;
    for (Layout* l = frame->layout();
         l != Frame::InitialLayout;
         l = l->parent())
    {
        Name name = l->name();
        if (name != Name::__bases__)
            names.push_back(l->name());
    }
    Stack<Layout*> layout(Class::InitialLayout);
    for (auto i = names.begin(); i != names.end(); i++)
        layout = layout->addName(*i);
    Stack<Value> bases;
    if (!frame->maybeGetAttr(Name::__bases__, bases)) {
        interp.pushStack(gc.create<AttributeError>("Missing __bases__"));
        return false;
    }
    if (!bases.toObject()->is<Tuple>()) {
        interp.pushStack(gc.create<TypeError>("__bases__ is not a tuple"));
        return false;
    }
    Stack<Tuple*> tuple(bases.toObject()->as<Tuple>());
    Stack<Class*> base(Object::ObjectClass);
    if (tuple->len() > 1) {
        interp.pushStack(gc.create<NotImplementedError>(
                             "Multiple inheritance not NYI"));
        return false;
    } else if (tuple->len() == 1) {
        Stack<Value> value(tuple->getitem(0));
        if (!value.toObject()->is<Class>()) {
            interp.pushStack(gc.create<TypeError>("__bases__[0] is not a class"));
            return false;
        }
        base = value.toObject()->as<Class>();
    }
    Class* cls = gc.create<Class>(self->ident, base, layout);
    Stack<Value> value;
    for (auto i = names.begin(); i != names.end(); i++) {
        value = frame->getAttr(*i);
        cls->setAttr(*i, value);
    }
    interp.pushStack(cls);
    return true;
}

/* static */ bool InstrDestructure::execute(Traced<InstrDestructure*> self,
                                            Interpreter& interp)
{
    Stack<Value> seq(interp.popStack());
    Stack<Value> lenFunc;
    if (!seq.maybeGetAttr(Name::__len__, lenFunc)) {
        interp.pushStack(gc.create<TypeError>("Argument is not iterable"));
        return false;
    }

    Stack<Value> getitemFunc;
    if (!seq.maybeGetAttr(Name::__getitem__, getitemFunc)) {
        interp.pushStack(gc.create<TypeError>("Argument is not iterable"));
        return false;
    }

    Stack<Value> result;
    {
        RootArray<Value, 1> args;
        args[0] = seq;
        if (!interp.call(lenFunc, args, result)) {
            interp.pushStack(result);
            return false;
        }
    }

    if (!result.isInt()) {
        interp.pushStack(gc.create<TypeError>("__len__ didn't return an integer"));
        return false;
    }

    int32_t len = result.toInt();
    if (len < 0) {
        interp.pushStack(gc.create<ValueError>("__len__ returned negative value"));
        return false;
    }
    if (size_t(len) != self->count_) {
        interp.pushStack(gc.create<ValueError>("too many values to unpack"));
        return false;
    }

    {
        RootArray<Value, 2> args;
        args[0] = seq;
        for (unsigned i = self->count_; i != 0; i--) {
            args[1] = Integer::get(i - 1);
            bool ok = interp.call(getitemFunc, args, result);
            interp.pushStack(result);
            if (!ok)
                return false;
        }
    }

    return true;
}

/* static */ bool InstrRaise::execute(Traced<InstrRaise*> self,
                                      Interpreter& interp)
{
    // todo: exceptions must be old-style classes or derived from BaseException
    return false;
}

/* static */ bool InstrIteratorNext::execute(Traced<InstrIteratorNext*> self,
                                             Interpreter& interp)
{
    // The stack is already set up with next method and target on top
    Stack<Value> target(interp.peekStack(2));
    TracedVector<Value> args = interp.stackSlice(1);
    Stack<Value> result;
    bool ok = interp.call(target, args, result);
    if (!ok) {
        Stack<Object*> exc(result.toObject());
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

/* static */ bool InstrBinaryOp::execute(Traced<InstrBinaryOp*> self,
                                         Interpreter& interp)
{
    Stack<Value> right(interp.peekStack(0));
    Stack<Value> left(interp.peekStack(1));

    if (left.isInt() && right.isInt()) {
        Stack<Value> method(
            Integer::ObjectClass->getAttr(Name::binMethod[self->op()]));
        return interp.replaceInstrAndRestart(
            self,
            InstrBinaryOpInt::execute,
            gc.create<InstrBinaryOpInt>(self->op(), method));
    } else {
        return interp.replaceInstrFuncAndRestart(self, fallback);
    }
}

static bool maybeCallBinaryOp(Interpreter& interp,
                              Traced<Value> obj,
                              Name name,
                              Traced<Value> left,
                              Traced<Value> right,
                              bool& successOut)
{
    Stack<Value> method;
    if (!obj.maybeGetAttr(name, method))
        return false;

    Stack<Value> result;
    RootArray<Value, 2> args;
    args[0] = left;
    args[1] = right;
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

/* static */ bool InstrBinaryOp::fallback(Traced<InstrBinaryOp*> self,
                                          Interpreter& interp)
{
    Stack<Value> right(interp.popStack());
    Stack<Value> left(interp.popStack());

    // "If the right operand's type is a subclass of the left operand's type and
    // that subclass provides the reflected method for the operation, this
    // method will be called before the left operand's non-reflected
    // method. This behavior allows subclasses to override their ancestors'
    // operations."
    // todo: needs test

    bool success;
    Stack<Class*> ltype(left.type());
    Stack<Class*> rtype(right.type());
    BinaryOp op = self->op();
    const Name* names = Name::binMethod;
    const Name* rnames = Name::binMethodReflected;
    if (rtype != ltype && rtype->isDerivedFrom(ltype))
    {
        if (maybeCallBinaryOp(interp, right, rnames[op], right, left, success))
            return success;
        if (maybeCallBinaryOp(interp, left, names[op], left, right, success))
            return success;
    } else {
        if (maybeCallBinaryOp(interp, left, names[op], left, right, success))
            return success;
        if (maybeCallBinaryOp(interp, right, rnames[op], right, left, success))
            return success;
    }

    interp.pushStack(gc.create<TypeError>(
                         "unsupported operand type(s) for binary operation"));
    return false;
}

/* static */ bool InstrBinaryOpInt::execute(Traced<InstrBinaryOpInt*> self,
                                            Interpreter& interp)
{
    Stack<Value> right(interp.peekStack(0));
    Stack<Value> left(interp.peekStack(1));

    if (!left.isInt() || !right.isInt()) {
        return interp.replaceInstrAndRestart(
            self,
            InstrBinaryOp::fallback,
            gc.create<InstrBinaryOp>(self->op()));
    }

    interp.popStack(2);
    RootArray<Value, 2> args;
    args[0] = left;
    args[1] = right;
    Stack<Value> result;
    Stack<Value> method(self->method_);
    bool r = interp.call(method, args, result);
    assert(r);
    assert(result != Value(NotImplemented));
    interp.pushStack(result);
    return r;
}

/* static */ bool
InstrAugAssignUpdate::execute(Traced<InstrAugAssignUpdate*> self,
                              Interpreter& interp)
{
    Stack<Value> update(interp.popStack());
    Stack<Value> value(interp.popStack());

    Stack<Value> method;
    Stack<Value> result;
    RootArray<Value, 2> args;
    args[0] = value;
    args[1] = update;
    if (value.maybeGetAttr(Name::augAssignMethod[self->op()], method)) {
        if (!interp.call(method, args, result)) {
            interp.pushStack(result);
            return false;
        }
        interp.pushStack(result);
        return true;
    } else if (value.maybeGetAttr(Name::binMethod[self->op()], method)) {
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

/* static */ bool
InstrResumeGenerator::execute(Traced<InstrResumeGenerator*> self,
                              Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    GeneratorIter *gen = frame->getAttr("self").asObject()->as<GeneratorIter>();
    return gen->resume(interp);
}

/* static */ bool InstrLeaveGenerator::execute(Traced<InstrLeaveGenerator*> self,
                                               Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    GeneratorIter *gen = frame->getAttr("%gen").asObject()->as<GeneratorIter>();
    return gen->leave(interp);
}

/* static */ bool
InstrSuspendGenerator::execute(Traced<InstrSuspendGenerator*> self,
                               Interpreter& interp)
{
    Stack<Value> value(interp.popStack());
    Frame* frame = interp.getFrame();
    GeneratorIter *gen = frame->getAttr("%gen").asObject()->as<GeneratorIter>();
    gen->suspend(interp, value);
    return true;
}

/* static */ bool
InstrEnterCatchRegion::execute(Traced<InstrEnterCatchRegion*> self,
                               Interpreter& interp)
{
    interp.pushExceptionHandler(ExceptionHandler::CatchHandler, self->offset_);
    return true;
}

/* static */ bool
InstrLeaveCatchRegion::execute(Traced<InstrLeaveCatchRegion*> self,
                               Interpreter& interp)
{
    interp.popExceptionHandler(ExceptionHandler::CatchHandler);
    return true;
}

/* static */ bool
InstrMatchCurrentException::execute(Traced<InstrMatchCurrentException*> self,
                                    Interpreter& interp)
{
    Stack<Object*> obj(interp.popStack().toObject());
    Stack<Exception*> exception(interp.currentException());
    bool match = obj->is<Class>() && exception->isInstanceOf(obj->as<Class>());
    if (match) {
        interp.finishHandlingException();
        interp.pushStack(exception);
    }
    interp.pushStack(Boolean::get(match));
    return true;
}

/* static */ bool
InstrHandleCurrentException::execute(Traced<InstrHandleCurrentException*> self,
                                     Interpreter& interp)
{
    interp.finishHandlingException();
    return true;
}

/* static */ bool
InstrEnterFinallyRegion::execute(Traced<InstrEnterFinallyRegion*> self,
                                 Interpreter& interp)
{
    interp.pushExceptionHandler(ExceptionHandler::FinallyHandler, self->offset_);
    return true;
}

/* static */ bool
InstrLeaveFinallyRegion::execute(Traced<InstrLeaveFinallyRegion*> self,
                                 Interpreter& interp)
{
    interp.popExceptionHandler(ExceptionHandler::FinallyHandler);
    return true;
}

/* static */ bool
InstrFinishExceptionHandler::execute(Traced<InstrFinishExceptionHandler*> self,
                                     Interpreter& interp)
{
    return interp.maybeContinueHandlingException();
}

/* static */ bool
InstrLoopControlJump::execute(Traced<InstrLoopControlJump*> self,
                              Interpreter& interp)
{
    interp.loopControlJump(self->finallyCount_, self->target_);
    return true;
}

/* static */ bool
InstrAssertStackDepth::execute(Traced<InstrAssertStackDepth*> self,
                               Interpreter& interp)
{
#ifdef DEBUG
    unsigned actual = interp.frameStackDepth();
    if (actual != self->expected_) {
        cerr << "Excpected stack depth " << dec << self->expected_;
        cerr << " but got " << actual << " in: " << endl;
        cerr << *interp.getFrame()->block() << endl;
        assert(actual == self->expected_);
    }
#endif
    return true;
}
