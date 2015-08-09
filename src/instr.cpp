#include "instr.h"

#include "builtin.h"
#include "generator.h"

#define INLINE_INSTRS /*inline*/

/* static */ INLINE_INSTRS bool
InstrConst::execute(Traced<InstrConst*> self, Interpreter& interp)
{
    interp.pushStack(self->value);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrGetStackLocal::execute(Traced<InstrGetStackLocal*> self,
                            Interpreter& interp)
{
    // Name was present when compiled, but may have been deleted.
    {
        AutoAssertNoGC nogc;
        Value value = interp.getStackLocal(self->slot_);
        if (value != Value(UninitializedSlot)) {
            interp.pushStack(value);
            return true;
        }
    }

    interp.raiseNameError(self->ident);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrSetStackLocal::execute(Traced<InstrSetStackLocal*> self,
                            Interpreter& interp)
{
    AutoAssertNoGC nogc;
    Value value = interp.peekStack(0);
    assert(value != Value(UninitializedSlot));
    interp.setStackLocal(self->slot_, value);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrDelStackLocal::execute(Traced<InstrDelStackLocal*> self,
                            Interpreter& interp)
{
    // Delete by setting slot value to UninitializedSlot.
    {
        AutoAssertNoGC nogc;
        if (interp.getStackLocal(self->slot_) != Value(UninitializedSlot)) {
            interp.setStackLocal(self->slot_, UninitializedSlot);
            return true;
        }
    }

    interp.raiseNameError(self->ident);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrGetLexical::execute(Traced<InstrGetLexical*> self,
                         Interpreter& interp)
{
    Stack<Env*> env(interp.lexicalEnv(self->frameIndex));
    Stack<Value> value;
    // Name was present when compiled, but may have been deleted.
    if (!env->maybeGetAttr(self->ident, value)) {
        interp.raiseNameError(self->ident);
        return true;
    }
    interp.pushStack(value);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrSetLexical::execute(Traced<InstrSetLexical*> self, Interpreter& interp)
{
    Stack<Value> value(interp.peekStack(0));
    Stack<Env*> env(interp.lexicalEnv(self->frameIndex));
    env->setAttr(self->ident, value);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrDelLexical::execute(Traced<InstrDelLexical*> self, Interpreter& interp)
{
    Stack<Env*> env(interp.lexicalEnv(self->frameIndex));
    if (!env->maybeDelOwnAttr(self->ident)) {
        interp.raiseNameError(self->ident);
        return true;
    }
    return true;
}

/* static */ INLINE_INSTRS bool
InstrGetGlobal::execute(Traced<InstrGetGlobal*> self, Interpreter& interp)
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

    interp.raiseNameError(self->ident);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrSetGlobal::execute(Traced<InstrSetGlobal*> self, Interpreter& interp)
{
    Stack<Value> value(interp.peekStack(0));
    self->global->setAttr(self->ident, value);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrDelGlobal::execute(Traced<InstrDelGlobal*> self, Interpreter& interp)
{
    if (!self->global->maybeDelOwnAttr(self->ident)) {
        interp.raiseNameError(self->ident);
        return true;
    }
    return true;
}

/* static */ INLINE_INSTRS bool
InstrGetAttr::execute(Traced<InstrGetAttr*> self, Interpreter& interp)
{
    Stack<Value> value(interp.popStack());
    Stack<Value> result;
    bool ok = getAttr(value, self->ident, result);
    interp.pushStack(result);
    return ok;
}

/* static */ INLINE_INSTRS bool
InstrSetAttr::execute(Traced<InstrSetAttr*> self, Interpreter& interp)
{
    Stack<Value> value(interp.peekStack(1));
    Stack<Object*> obj(interp.popStack().toObject());
    Stack<Value> result;
    if (!setAttr(obj, self->ident, value, result)) {
        interp.raiseException(result);
        return true;
    }

    return true;
}

/* static */ INLINE_INSTRS bool
InstrDelAttr::execute(Traced<InstrDelAttr*> self, Interpreter& interp)
{
    Stack<Object*> obj(interp.popStack().toObject());
    Stack<Value> result;
    if (!delAttr(obj, self->ident, result)) {
        interp.raiseException(result);
        return true;
    }

    return true;
}

/* static */ INLINE_INSTRS bool
InstrCall::execute(Traced<InstrCall*> self, Interpreter& interp)
{
    Stack<Value> target(interp.peekStack(self->count_));
    interp.removeStackEntries(self->count_, 1);
    return interp.startCall(target, self->count_);
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

static bool getMethod(Name ident, Interpreter& interp)
{
    Stack<Value> value(interp.popStack());
    Stack<Value> result;
    bool isCallableDescriptor;
    if (!getMethodAttr(value, ident, result, isCallableDescriptor)) {
        interp.raiseAttrError(value, ident);
        return false;
    }

    interp.pushStack(result, Boolean::get(isCallableDescriptor), value);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrGetMethod::execute(Traced<InstrGetMethod*> self, Interpreter& interp)
{
    Stack<Value> value(interp.peekStack(0));
    if (!getMethod(self->ident, interp))
        return true;

    if (value.isInt()) {
        Stack<Value> result(interp.peekStack(2));
        interp.replaceInstr(self,
                            gc.create<InstrGetMethodInt>(self->ident, result));
    } else {
        interp.replaceInstr(self,
                            gc.create<InstrGetMethodFallback>(self->ident));
    }

    return true;
}

/* static */ bool
InstrGetMethodFallback::execute(Traced<InstrGetMethodFallback*> self,
                                Interpreter& interp)
{
    return getMethod(self->ident, interp);
}

/* static */ INLINE_INSTRS bool
InstrGetMethodInt::execute(Traced<InstrGetMethodInt*> self, Interpreter& interp)
{
    {
        AutoAssertNoGC nogc;
        Value value = interp.peekStack(0);
        if (value.isInt()) {
            interp.popStack();
            interp.pushStack(self->result_, Boolean::True, value);
            return true;
        }
    }

    return interp.replaceInstrAndRestart(
        self,
        gc.create<InstrGetMethodFallback>(self->ident));
}

/* static */ INLINE_INSTRS bool
InstrCallMethod::execute(Traced<InstrCallMethod*> self, Interpreter& interp)
{
    bool addSelf = interp.peekStack(self->count_ + 1).as<Boolean>()->value();
    Stack<Value> target(interp.peekStack(self->count_ + 2));
    unsigned argCount = self->count_ + (addSelf ? 1 : 0);
    interp.removeStackEntries(argCount, addSelf ? 2 : 3);
    return interp.startCall(target, argCount);
}

/* static */ INLINE_INSTRS bool
InstrCreateEnv::execute(Traced<InstrCreateEnv*> self, Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    assert(!frame->env());

    Object* obj = interp.popStack().asObject();
    Stack<Env*> parentEnv(obj ? obj->as<Env>() : nullptr);
    Stack<Block*> block(frame->block());
    Stack<Layout*> layout(block->layout());
    Stack<Env*> callEnv(gc.create<Env>(parentEnv, layout));
    unsigned argCount = block->argCount();
    Stack<Value> value;
    for (size_t i = 0; i < argCount; i++) {
        value = interp.getStackLocal(i);
        callEnv->setSlot(i, value);
        interp.setStackLocal(i, UninitializedSlot);
    }
    interp.setFrameEnv(callEnv);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrInitStackLocals::execute(Traced<InstrInitStackLocals*> self,
                              Interpreter& interp)
{
    AutoAssertNoGC nogc;
    Frame* frame = interp.getFrame();
    assert(!frame->env());

    Object* obj = interp.popStack().asObject();
    Env* parentEnv = obj ? obj->as<Env>() : nullptr;
    interp.setFrameEnv(parentEnv);

    Block* block = frame->block();
    interp.fillStack(block->stackLocalCount(), UninitializedSlot);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrIn::execute(Traced<InstrIn*> self, Interpreter& interp)
{
    // todo: implement this
    // https://docs.python.org/2/reference/expressions.html#membership-test-details

    Stack<Object*> container(interp.popStack().toObject());
    Stack<Value> value(interp.popStack());
    Stack<Value> contains;
    if (!container->maybeGetAttr(Name::__contains__, contains)) {
        interp.pushStack(gc.create<TypeError>("Argument is not iterable"));
        interp.raiseException();
        return true;
    }

    interp.pushStack(container, value);
    return interp.startCall(contains, 2);
}

/* static */ INLINE_INSTRS bool
InstrIs::execute(Traced<InstrIs*> self, Interpreter& interp)
{
    Value b = interp.popStack();
    Value a = interp.popStack();
    bool result = a.toObject() == b.toObject();
    interp.pushStack(Boolean::get(result));
    return true;
}

/* static */ INLINE_INSTRS bool
InstrNot::execute(Traced<InstrNot*> self, Interpreter& interp)
{
    // the following values are interpreted as false: False, None, numeric zero
    // of all types, and empty strings and containers (including strings,
    // tuples, lists, dictionaries, sets and frozensets).

    Object* obj = interp.popStack().toObject();
    interp.pushStack(Boolean::get(!obj->isTrue()));
    return true;
}

/* static */ INLINE_INSTRS bool
InstrBranchAlways::execute(Traced<InstrBranchAlways*> self, Interpreter& interp)
{
    assert(self->offset_);
    interp.branch(self->offset_);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrBranchIfTrue::execute(Traced<InstrBranchIfTrue*> self, Interpreter& interp)
{
    assert(self->offset_);
    Object *x = interp.popStack().toObject();
    if (x->isTrue())
        interp.branch(self->offset_);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrBranchIfFalse::execute(Traced<InstrBranchIfFalse*> self,
                            Interpreter& interp)
{
    assert(self->offset_);
    Object *x = interp.popStack().toObject();
    if (!x->isTrue())
        interp.branch(self->offset_);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrOr::execute(Traced<InstrOr*> self, Interpreter& interp)
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

/* static */ INLINE_INSTRS bool
InstrAnd::execute(Traced<InstrAnd*> self, Interpreter& interp)
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

/* static */ INLINE_INSTRS bool
InstrLambda::execute(Traced<InstrLambda*> self, Interpreter& interp)
{
    Stack<FunctionInfo*> info(self->info_);
    Stack<Block*> block(self->block());
    Stack<Env*> env(interp.env());

    Stack<Object*> obj;
    obj = gc.create<Function>(self->funcName_, info,
                              interp.stackSlice(self->defaultCount()), env);
    interp.popStack(self->defaultCount());
    interp.pushStack(Value(obj));
    return true;
}

/* static */ INLINE_INSTRS bool
InstrDup::execute(Traced<InstrDup*> self, Interpreter& interp)
{
    interp.pushStack(interp.peekStack(self->index_));
    return true;
}

/* static */ INLINE_INSTRS bool
InstrPop::execute(Traced<InstrPop*> self, Interpreter& interp)
{
    interp.popStack();
    return true;
}

/* static */ INLINE_INSTRS bool
InstrSwap::execute(Traced<InstrSwap*> self, Interpreter& interp)
{
    interp.swapStack();
    return true;
}

/* static */ INLINE_INSTRS bool
InstrTuple::execute(Traced<InstrTuple*> self, Interpreter& interp)
{
    Tuple* tuple = Tuple::get(interp.stackSlice(self->size));
    interp.popStack(self->size);
    interp.pushStack(tuple);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrList::execute(Traced<InstrList*> self, Interpreter& interp)
{
    List* list = gc.create<List>(interp.stackSlice(self->size));
    interp.popStack(self->size);
    interp.pushStack(list);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrDict::execute(Traced<InstrDict*> self, Interpreter& interp)
{
    Dict* dict = gc.create<Dict>(interp.stackSlice(self->size * 2));
    interp.popStack(self->size * 2);
    interp.pushStack(dict);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrSlice::execute(Traced<InstrSlice*> self, Interpreter& interp)
{
    Slice* slice = gc.create<Slice>(interp.stackSlice(3));
    interp.popStack(3);
    interp.pushStack(slice);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrAssertionFailed::execute(Traced<InstrAssertionFailed*> self,
                              Interpreter& interp)
{
    Object* obj = interp.popStack().toObject();
    assert(obj->is<String>() || obj == None);
    string message = obj != None ? obj->as<String>()->value() : "";
    interp.pushStack(gc.create<AssertionError>(message));
    interp.raiseException();
    return true;
}

/* static */ INLINE_INSTRS bool
InstrMakeClassFromFrame::execute(Traced<InstrMakeClassFromFrame*> self,
                                 Interpreter& interp)
{
    Stack<Env*> env(interp.env());

    // todo: this layout transplanting should be used to a utility method
    vector<Name> names;
    for (Layout* l = env->layout(); l != Env::InitialLayout; l = l->parent())
        names.push_back(l->name());
    Stack<Layout*> layout(Class::InitialLayout);
    for (auto i = names.begin(); i != names.end(); i++)
        layout = layout->addName(*i);

    Stack<Value> bases;
    if (!env->maybeGetAttr(Name::__bases__, bases)) {
        interp.pushStack(gc.create<AttributeError>("Missing __bases__"));
        interp.raiseException();
        return true;
    }
    if (!bases.toObject()->is<Tuple>()) {
        interp.pushStack(gc.create<TypeError>("__bases__ is not a tuple"));
        interp.raiseException();
        return true;
    }
    Stack<Tuple*> tuple(bases.toObject()->as<Tuple>());
    Stack<Class*> base(Object::ObjectClass);
    if (tuple->len() > 1) {
        interp.pushStack(gc.create<NotImplementedError>(
                             "Multiple inheritance not NYI"));
        interp.raiseException();
        return true;
    } else if (tuple->len() == 1) {
        Stack<Value> value(tuple->getitem(0));
        if (!value.toObject()->is<Class>()) {
            interp.pushStack(gc.create<TypeError>("__bases__[0] is not a class"));
            interp.raiseException();
            return true;
        }
        base = value.toObject()->as<Class>();
    }
    Class* cls = gc.create<Class>(self->ident, base, layout);
    Stack<Value> value;
    for (auto i = names.begin(); i != names.end(); i++) {
        value = env->getAttr(*i);
        cls->setAttr(*i, value);
    }
    interp.pushStack(cls);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrRaise::execute(Traced<InstrRaise*> self, Interpreter& interp)
{
    // todo: exceptions must be old-style classes or derived from BaseException
    interp.raiseException();
    return true;
}

/* static */ INLINE_INSTRS bool
InstrIteratorNext::execute(Traced<InstrIteratorNext*> self, Interpreter& interp)
{
    // The stack is already set up with next method and target on top
    Stack<Value> target(interp.peekStack(2));
    Stack<Value> result;
    interp.pushStack(interp.peekStack(0));
    bool ok = interp.call(target, 1, result);
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

/* static */ INLINE_INSTRS bool
InstrBinaryOp::execute(Traced<InstrBinaryOp*> self, Interpreter& interp)
{
    Value right = interp.peekStack(0);
    Value left = interp.peekStack(1);
    if (left.isInt32() && right.isInt32()) {
        assert(Integer::ObjectClass->hasAttr(Name::binMethod[self->op()]));
        return replaceWithIntInstr(self, interp);
    } else {
        return interp.replaceInstrAndRestart(
            self,
            gc.create<InstrBinaryOpFallback>(self->op()));
    }
}

/* static */ bool
InstrBinaryOp::replaceWithIntInstr(Traced<InstrBinaryOp*> self,
                                   Interpreter& interp)
{
    // todo: can use static table
    switch (self->op()) {
#define create_instr(name, token, method, rmethod, imethod)                   \
      case Binary##name:                                                      \
        return interp.replaceInstrAndRestart(                                 \
            self,                                                             \
            gc.create<InstrBinaryOpInt<Binary##name>>());

    for_each_binary_op(create_instr)
#undef create_instr
      default:
        assert(false);
        return false;
    }
}

static inline bool maybeCallBinaryOp(Interpreter& interp,
                              Traced<Value> obj,
                              Name name,
                              Traced<Value> left,
                              Traced<Value> right,
                              bool& successOut)
{
    successOut = false;
    Stack<Value> method;
    if (!obj.maybeGetAttr(name, method))
        return false;

    Stack<Value> result;
    interp.pushStack(left);
    interp.pushStack(right);
    if (!interp.call(method, 2, result)) {
        interp.pushStack(result);
        return true;
    }

    if (result == Value(NotImplemented))
        return false;

    interp.pushStack(result);
    successOut = true;
    return true;
}

/* static */ INLINE_INSTRS bool
InstrBinaryOpFallback::execute(Traced<InstrBinaryOpFallback*> self,
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
    interp.raiseException();
    return true;
}

template <BinaryOp Op>
/* static */ INLINE_INSTRS bool
InstrBinaryOpInt<Op>::execute(Traced<InstrBinaryOpInt*> self,
                              Interpreter& interp)
{
    if (!interp.peekStack(0).isInt32() || !interp.peekStack(1).isInt32()) {
        return interp.replaceInstrAndRestart(
            self,
            gc.create<InstrBinaryOpFallback>(self->op()));
    }

    int32_t b = interp.popStack().asInt32();
    int32_t a = interp.popStack().asInt32();
    interp.pushStack(Integer::binaryOp<Op>(a, b));
    return true;
}

void CompareOpInstr::print(ostream& s) const
{
    s << name() << " " << CompareOpNames[op()];
}

/* static */ INLINE_INSTRS bool
InstrCompareOp::execute(Traced<InstrCompareOp*> self, Interpreter& interp)
{
    Value right = interp.peekStack(0);
    Value left = interp.peekStack(1);
    if (left.isInt32() && right.isInt32()) {
        assert(Integer::ObjectClass->hasAttr(Name::compareMethod[self->op()]));
        return replaceWithIntInstr(self, interp);
    } else {
        return interp.replaceInstrAndRestart(
            self,
            gc.create<InstrCompareOpFallback>(self->op()));
    }
}

/* static */ inline bool
InstrCompareOp::replaceWithIntInstr(Traced<InstrCompareOp*> self,
                                    Interpreter& interp)
{
    // todo: can use static table
    switch (self->op()) {
#define create_instr(name, token, method, rmethod)                            \
      case Compare##name:                                                     \
        return interp.replaceInstrAndRestart(                                 \
            self,                                                             \
            gc.create<InstrCompareOpInt<Compare##name>>());

    for_each_compare_op(create_instr)
#undef create_instr
      default:
        assert(false);
        return false;
    }
}

/* static */ INLINE_INSTRS bool
InstrCompareOpFallback::execute(Traced<InstrCompareOpFallback*> self,
                                Interpreter& interp)
{
    Stack<Value> right(interp.popStack());
    Stack<Value> left(interp.popStack());

    // "There are no swapped-argument versions of these methods (to be used when
    // the left argument does not support the operation but the right argument
    // does); rather, __lt__() and __gt__() are each other's reflection,
    // __le__() and __ge__() are each other's reflection, and __eq__() and
    // __ne__() are their own reflection."

    bool success;
    CompareOp op = self->op();
    const Name* names = Name::compareMethod;
    const Name* rnames = Name::compareMethodReflected;
    if (maybeCallBinaryOp(interp, left, names[op], left, right, success))
        return success;
    if (!rnames[op].isNull() &&
        maybeCallBinaryOp(interp, right, rnames[op], right, left, success)) {
        return success;
    }
    if (op == CompareNE &&
        maybeCallBinaryOp(interp, left, names[CompareEQ], left, right, success))
    {
        if (success) {
            Stack<Value> result(interp.popStack());
            interp.pushStack(Boolean::get(!result.as<Boolean>()->value()));
        }
        return success;
    }

    Stack<Value> result;
    if (op == CompareEQ) {
        result = Boolean::get(left.toObject() == right.toObject());
    } else if (op == CompareNE) {
        result = Boolean::get(left.toObject() != right.toObject());
    } else {
        interp.pushStack(gc.create<TypeError>(
                         "unsupported operand type(s) for compare operation"));
        interp.raiseException();
        return true;
    }

    interp.pushStack(result);
    return true;
}

template <CompareOp Op> /* static */ INLINE_INSTRS bool
InstrCompareOpInt<Op>::execute(Traced<InstrCompareOpInt*> self,
                               Interpreter& interp)
{
    if (!interp.peekStack(0).isInt32() || !interp.peekStack(1).isInt32()) {
        return interp.replaceInstrAndRestart(
            self,
            gc.create<InstrCompareOpFallback>(self->op()));
    }

    int32_t b = interp.popStack().asInt32();
    int32_t a = interp.popStack().asInt32();
    interp.pushStack(Integer::compareOp<Op>(a, b));
    return true;
}

/* static */ INLINE_INSTRS bool
InstrAugAssignUpdate::execute(Traced<InstrAugAssignUpdate*> self,
                              Interpreter& interp)
{
    Stack<Value> update(interp.popStack());
    Stack<Value> value(interp.popStack());

    Stack<Value> method;
    Stack<Value> result;
    interp.pushStack(value);
    interp.pushStack(update);
    if (value.maybeGetAttr(Name::augAssignMethod[self->op()], method)) {
        if (!interp.call(method, 2, result)) {
            interp.raiseException(result);
            return true;
        }
        interp.pushStack(result);
        return true;
    } else if (value.maybeGetAttr(Name::binMethod[self->op()], method)) {
        if (!interp.call(method, 2, result)) {
            interp.raiseException(result);
            return true;
        }
        interp.pushStack(result);
        return true;
    } else {
        interp.popStack(2);
        string message = "unsupported operand type(s) for augmented assignment";
        interp.pushStack(gc.create<TypeError>(message));
        interp.raiseException();
        return true;
    }
}

/* static */ INLINE_INSTRS bool
InstrStartGenerator::execute(Traced<InstrStartGenerator*> self,
                             Interpreter& interp)
{
    Frame* frame = interp.getFrame();
    Stack<Block*> block(frame->block());
    Stack<Env*> env(interp.env());
    Stack<GeneratorIter*> gen;
    gen = gc.create<GeneratorIter>(block, env, block->argCount());
    Stack<Value> value(gen);
    interp.pushStack(value);
    gen->suspend(interp, value);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrResumeGenerator::execute(Traced<InstrResumeGenerator*> self,
                              Interpreter& interp)
{
    Stack<Env*> env(interp.env());
    // todo: get slot 0
    Stack<GeneratorIter*> gen(env->getAttr("self").asObject()->as<GeneratorIter>());
    return gen->resume(interp);
}

/* static */ INLINE_INSTRS bool
InstrLeaveGenerator::execute(Traced<InstrLeaveGenerator*> self,
                             Interpreter& interp)
{
    Stack<GeneratorIter*> gen(interp.getGeneratorIter());
    return gen->leave(interp);
}

/* static */ INLINE_INSTRS bool
InstrSuspendGenerator::execute(Traced<InstrSuspendGenerator*> self,
                               Interpreter& interp)
{
    Stack<Value> value(interp.popStack());
    Stack<GeneratorIter*> gen(interp.getGeneratorIter());
    gen->suspend(interp, value);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrEnterCatchRegion::execute(Traced<InstrEnterCatchRegion*> self,
                               Interpreter& interp)
{
    interp.pushExceptionHandler(ExceptionHandler::CatchHandler, self->offset_);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrLeaveCatchRegion::execute(Traced<InstrLeaveCatchRegion*> self,
                               Interpreter& interp)
{
    interp.popExceptionHandler(ExceptionHandler::CatchHandler);
    return true;
}

/* static */ INLINE_INSTRS bool
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

/* static */ INLINE_INSTRS bool
InstrHandleCurrentException::execute(Traced<InstrHandleCurrentException*> self,
                                     Interpreter& interp)
{
    interp.finishHandlingException();
    return true;
}

/* static */ INLINE_INSTRS bool
InstrEnterFinallyRegion::execute(Traced<InstrEnterFinallyRegion*> self,
                                 Interpreter& interp)
{
    interp.pushExceptionHandler(ExceptionHandler::FinallyHandler, self->offset_);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrLeaveFinallyRegion::execute(Traced<InstrLeaveFinallyRegion*> self,
                                 Interpreter& interp)
{
    interp.popExceptionHandler(ExceptionHandler::FinallyHandler);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrFinishExceptionHandler::execute(Traced<InstrFinishExceptionHandler*> self,
                                     Interpreter& interp)
{
    return interp.maybeContinueHandlingException();
}

/* static */ INLINE_INSTRS bool
InstrLoopControlJump::execute(Traced<InstrLoopControlJump*> self,
                              Interpreter& interp)
{
    interp.loopControlJump(self->finallyCount_, self->target_);
    return true;
}

/* static */ INLINE_INSTRS bool
InstrAssertStackDepth::execute(Traced<InstrAssertStackDepth*> self,
                               Interpreter& interp)
{
#ifdef DEBUG
    Frame* frame = interp.getFrame();
    unsigned depth = interp.stackPos() - frame->stackPos();
    if (depth != self->expected_) {
        cerr << "Excpected stack depth " << dec << self->expected_;
        cerr << " but got " << depth << " in: " << endl;
        cerr << *interp.getFrame()->block() << endl;
        assert(depth == self->expected_);
    }
#endif
    return true;
}

#define typedef_binary_op_int(name, token, method, rmethod, imethod)          \
    typedef InstrBinaryOpInt<Binary##name> InstrBinaryOpInt_##name;
    for_each_binary_op(typedef_binary_op_int)
#undef typedef_binary_op_int

#define typedef_compare_op_int(name, token, method, rmethod)                  \
    typedef InstrCompareOpInt<Compare##name> InstrCompareOpInt_##name;
    for_each_compare_op(typedef_compare_op_int)
#undef typedef_compare_op_int

bool Interpreter::run(MutableTraced<Value> resultOut)
{
    static const void* dispatchTable[] =
    {
        &&instr_Abort,
        &&instr_Return,
#define dispatch_table_entry(it)                                              \
        &&instr_##it,
    for_each_instr(dispatch_table_entry)
#undef dispatch_table_entry
    };
    static_assert(sizeof(dispatchTable) / sizeof(void*) == InstrTypeCount,
        "Dispatch table is not correct size");

    InstrThunk* thunk;

#define dispatch()                                                            \
    assert(getFrame()->block()->contains(instrp));                            \
    thunk = instrp++;                                                         \
    logInstr(thunk->data);                                                    \
    assert(thunk->type < InstrTypeCount);                                     \
    goto *dispatchTable[thunk->type]

    dispatch();

  instr_Abort: {
        resultOut = popStack();
        popFrame();
        return false;
    }

  instr_Return: {
        Value value = interp.popStack();
        interp.returnFromFrame(value);
        if (!instrp)
            goto exit;
        dispatch();
    }

#define handle_instr(it)                                                      \
    instr_##it: {                                                             \
        Instr##it** ip = reinterpret_cast<Instr##it**>(&thunk->data);         \
        bool ok = Instr##it::execute(                                         \
            Traced<Instr##it*>::fromTracedLocation(ip), interp);              \
        if (!ok)                                                              \
            raiseException();                                                 \
        assert(instrp);                                                       \
        dispatch();                                                           \
    }
    for_each_instr(handle_instr)
#undef handle_instr

  exit:
    assert(!instrp);
    resultOut = popStack();
    return true;
}
