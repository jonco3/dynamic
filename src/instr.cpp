#include "instr.h"

#include "builtin.h"
#include "dict.h"
#include "exception.h"
#include "generator.h"
#include "interp.h"
#include "singletons.h"
#include "list.h"

#define INLINE_INSTRS /*inline*/

INLINE_INSTRS void
Interpreter::executeInstr_Const(Traced<InstrConst*> self)
{
    pushStack(self->value);
}

INLINE_INSTRS void
Interpreter::executeInstr_GetStackLocal(Traced<InstrGetStackLocal*> self)
{
    // Name was present when compiled, but may have been deleted.
    {
        AutoAssertNoGC nogc;
        Value value = getStackLocal(self->slot_);
        if (value != Value(UninitializedSlot)) {
            pushStack(value);
            return;
        }
    }

    raiseNameError(self->ident);
}

INLINE_INSTRS void
Interpreter::executeInstr_SetStackLocal(Traced<InstrSetStackLocal*> self)
{
    AutoAssertNoGC nogc;
    Value value = peekStack(0);
    assert(value != Value(UninitializedSlot));
    setStackLocal(self->slot_, value);
}

INLINE_INSTRS void
Interpreter::executeInstr_DelStackLocal(Traced<InstrDelStackLocal*> self)
{
    // Delete by setting slot value to UninitializedSlot.
    {
        AutoAssertNoGC nogc;
        if (getStackLocal(self->slot_) != Value(UninitializedSlot)) {
            setStackLocal(self->slot_, UninitializedSlot);
            return;
        }
    }

    raiseNameError(self->ident);
}

INLINE_INSTRS void
Interpreter::executeInstr_GetLexical(Traced<InstrGetLexical*> self)
{
    Stack<Env*> env(lexicalEnv(self->frameIndex));
    Stack<Value> value;
    // Name was present when compiled, but may have been deleted.
    if (!env->maybeGetAttr(self->ident, value)) {
        raiseNameError(self->ident);
        return;
    }
    pushStack(value);
}

INLINE_INSTRS void
Interpreter::executeInstr_SetLexical(Traced<InstrSetLexical*> self)
{
    Stack<Value> value(peekStack(0));
    Stack<Env*> env(lexicalEnv(self->frameIndex));
    env->setAttr(self->ident, value);
}

INLINE_INSTRS void
Interpreter::executeInstr_DelLexical(Traced<InstrDelLexical*> self)
{
    Stack<Env*> env(lexicalEnv(self->frameIndex));
    if (!env->maybeDelOwnAttr(self->ident))
        raiseNameError(self->ident);
}

INLINE_INSTRS void
Interpreter::executeInstr_GetGlobal(Traced<InstrGetGlobal*> self)
{
    Stack<Value> value;
    if (self->global->maybeGetAttr(self->ident, value)) {
        assert(value.toObject());
        pushStack(value);
        return;
    }

    Stack<Value> builtins;
    if (self->global->maybeGetAttr(Name::__builtins__, builtins) &&
        builtins.maybeGetAttr(self->ident, value))
    {
        assert(value.toObject());
        pushStack(value);
        return;
    }

    raiseNameError(self->ident);
}

INLINE_INSTRS void
Interpreter::executeInstr_SetGlobal(Traced<InstrSetGlobal*> self)
{
    Stack<Value> value(peekStack(0));
    self->global->setAttr(self->ident, value);
}

INLINE_INSTRS void
Interpreter::executeInstr_DelGlobal(Traced<InstrDelGlobal*> self)
{
    if (!self->global->maybeDelOwnAttr(self->ident))
        raiseNameError(self->ident);
}

INLINE_INSTRS void
Interpreter::executeInstr_GetAttr(Traced<InstrGetAttr*> self)
{
    Stack<Value> value(popStack());
    Stack<Value> result;
    bool ok = getAttr(value, self->ident, result);
    pushStack(result);
    if (!ok)
        raiseException();
}

INLINE_INSTRS void
Interpreter::executeInstr_SetAttr(Traced<InstrSetAttr*> self)
{
    Stack<Value> value(peekStack(1));
    Stack<Object*> obj(popStack().toObject());
    Stack<Value> result;
    if (!setAttr(obj, self->ident, value, result))
        raiseException(result);
}

INLINE_INSTRS void
Interpreter::executeInstr_DelAttr(Traced<InstrDelAttr*> self)
{
    Stack<Object*> obj(popStack().toObject());
    Stack<Value> result;
    if (!delAttr(obj, self->ident, result))
        raiseException(result);
}

INLINE_INSTRS void
Interpreter::executeInstr_Call(Traced<InstrCall*> self)
{
    Stack<Value> target(peekStack(self->count_));
    removeStackEntries(self->count_, 1);
    startCall(target, self->count_);
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

INLINE_INSTRS void
Interpreter::executeInstr_GetMethod(Traced<InstrGetMethod*> self)
{
    Stack<Value> value(peekStack(0));
    if (!getMethod(self->ident, *this))
        return;

    if (value.isInt()) {
        Stack<Value> result(peekStack(2));
        replaceInstr(self, gc.create<InstrGetMethodInt>(self->ident, result));
    } else {
        replaceInstr(self, gc.create<InstrGetMethodFallback>(self->ident));
    }
}

/* static */ void
Interpreter::executeInstr_GetMethodFallback(Traced<InstrGetMethodFallback*> self)
{
    getMethod(self->ident, *this);
}

INLINE_INSTRS void
Interpreter::executeInstr_GetMethodInt(Traced<InstrGetMethodInt*> self)
{
    {
        AutoAssertNoGC nogc;
        Value value = peekStack(0);
        if (value.isInt()) {
            popStack();
            pushStack(self->result_, Boolean::True, value);
            return;
        }
    }

    replaceInstrAndRestart(self, gc.create<InstrGetMethodFallback>(self->ident));
}

INLINE_INSTRS void
Interpreter::executeInstr_CallMethod(Traced<InstrCallMethod*> self)
{
    bool addSelf = peekStack(self->count_ + 1).as<Boolean>()->value();
    Stack<Value> target(peekStack(self->count_ + 2));
    unsigned argCount = self->count_ + (addSelf ? 1 : 0);
    removeStackEntries(argCount, addSelf ? 2 : 3);
    return startCall(target, argCount);
}

INLINE_INSTRS void
Interpreter::executeInstr_CreateEnv(Traced<InstrCreateEnv*> self)
{
    Frame* frame = getFrame();
    assert(!frame->env());

    Object* obj = popStack().asObject();
    Stack<Env*> parentEnv(obj ? obj->as<Env>() : nullptr);
    Stack<Block*> block(frame->block());
    Stack<Layout*> layout(block->layout());
    Stack<Env*> callEnv(gc.create<Env>(parentEnv, layout));
    unsigned argCount = block->argCount();
    Stack<Value> value;
    for (size_t i = 0; i < argCount; i++) {
        value = getStackLocal(i);
        callEnv->setSlot(i, value);
        setStackLocal(i, UninitializedSlot);
    }
    setFrameEnv(callEnv);
}

INLINE_INSTRS void
Interpreter::executeInstr_InitStackLocals(Traced<InstrInitStackLocals*> self)
{
    AutoAssertNoGC nogc;
    Frame* frame = getFrame();
    assert(!frame->env());

    Object* obj = popStack().asObject();
    Env* parentEnv = obj ? obj->as<Env>() : nullptr;
    setFrameEnv(parentEnv);

    Block* block = frame->block();
    fillStack(block->stackLocalCount(), UninitializedSlot);
}

INLINE_INSTRS void
Interpreter::executeInstr_In(Traced<InstrIn*> self)
{
    // todo: implement this
    // https://docs.python.org/2/reference/expressions.html#membership-test-details

    Stack<Object*> container(popStack().toObject());
    Stack<Value> value(popStack());
    Stack<Value> contains;
    if (!container->maybeGetAttr(Name::__contains__, contains)) {
        pushStack(gc.create<TypeError>("Argument is not iterable"));
        raiseException();
        return;
    }

    pushStack(container, value);
    startCall(contains, 2);
}

INLINE_INSTRS void
Interpreter::executeInstr_Is(Traced<InstrIs*> self)
{
    Value b = popStack();
    Value a = popStack();
    bool result = a.toObject() == b.toObject();
    pushStack(Boolean::get(result));
}

INLINE_INSTRS void
Interpreter::executeInstr_Not(Traced<InstrNot*> self)
{
    // the following values are interpreted as false: False, None, numeric zero
    // of all types, and empty strings and containers (including strings,
    // tuples, lists, dictionaries, sets and frozensets).

    Object* obj = popStack().toObject();
    pushStack(Boolean::get(!obj->isTrue()));
}

INLINE_INSTRS void
Interpreter::executeInstr_BranchAlways(Traced<InstrBranchAlways*> self)
{
    assert(self->offset_);
    branch(self->offset_);
}

INLINE_INSTRS void
Interpreter::executeInstr_BranchIfTrue(Traced<InstrBranchIfTrue*> self)
{
    assert(self->offset_);
    Object *x = popStack().toObject();
    if (x->isTrue())
        branch(self->offset_);
}

INLINE_INSTRS void
Interpreter::executeInstr_BranchIfFalse(Traced<InstrBranchIfFalse*> self)
{
    assert(self->offset_);
    Object *x = popStack().toObject();
    if (!x->isTrue())
        branch(self->offset_);
}

INLINE_INSTRS void
Interpreter::executeInstr_Or(Traced<InstrOr*> self)
{
    // The expression |x or y| first evaluates x; if x is true, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    assert(self->offset_);
    Object *x = peekStack(0).toObject();
    if (x->isTrue()) {
        branch(self->offset_);
        return;
    }

    popStack();
}

INLINE_INSTRS void
Interpreter::executeInstr_And(Traced<InstrAnd*> self)
{
    // The expression |x and y| first evaluates x; if x is false, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    assert(self->offset_);
    Object *x = peekStack(0).toObject();
    if (!x->isTrue()) {
        branch(self->offset_);
        return;
    }

    popStack();
}

InstrLambda::InstrLambda(Name name, const vector<Name>& paramNames,
                         Traced<Block*> block, unsigned defaultCount,
                         bool takesRest, bool isGenerator)
  : funcName_(name),
    info_(gc.create<FunctionInfo>(paramNames, block, defaultCount, takesRest,
                                  isGenerator))
{}

INLINE_INSTRS void
Interpreter::executeInstr_Lambda(Traced<InstrLambda*> self)
{
    Stack<FunctionInfo*> info(self->functionInfo());
    Stack<Block*> block(self->block());
    Stack<Env*> env(this->env());

    Stack<Object*> obj;
    obj = gc.create<Function>(self->functionName(), info,
                              stackSlice(self->defaultCount()), env);
    popStack(self->defaultCount());
    pushStack(Value(obj));
}

INLINE_INSTRS void
Interpreter::executeInstr_Dup(Traced<InstrDup*> self)
{
    pushStack(peekStack(self->index_));
}

INLINE_INSTRS void
Interpreter::executeInstr_Pop(Traced<InstrPop*> self)
{
    popStack();
}

INLINE_INSTRS void
Interpreter::executeInstr_Swap(Traced<InstrSwap*> self)
{
    swapStack();
}

INLINE_INSTRS void
Interpreter::executeInstr_Tuple(Traced<InstrTuple*> self)
{
    Tuple* tuple = Tuple::get(stackSlice(self->size));
    popStack(self->size);
    pushStack(tuple);
}

INLINE_INSTRS void
Interpreter::executeInstr_List(Traced<InstrList*> self)
{
    List* list = gc.create<List>(stackSlice(self->size));
    popStack(self->size);
    pushStack(list);
}

INLINE_INSTRS void
Interpreter::executeInstr_Dict(Traced<InstrDict*> self)
{
    Dict* dict = gc.create<Dict>(stackSlice(self->size * 2));
    popStack(self->size * 2);
    pushStack(dict);
}

INLINE_INSTRS void
Interpreter::executeInstr_Slice(Traced<InstrSlice*> self)
{
    Slice* slice = gc.create<Slice>(stackSlice(3));
    popStack(3);
    pushStack(slice);
}

INLINE_INSTRS void
Interpreter::executeInstr_AssertionFailed(Traced<InstrAssertionFailed*> self)
{
    Object* obj = popStack().toObject();
    assert(obj->is<String>() || obj == None);
    string message = obj != None ? obj->as<String>()->value() : "";
    pushStack(gc.create<AssertionError>(message));
    raiseException();
}

INLINE_INSTRS void
Interpreter::executeInstr_MakeClassFromFrame(Traced<InstrMakeClassFromFrame*> self)
{
    Stack<Env*> env(this->env());

    // todo: this layout transplanting should be used to a utility method
    vector<Name> names;
    for (Layout* l = env->layout(); l != Env::InitialLayout; l = l->parent())
        names.push_back(l->name());
    Stack<Layout*> layout(Class::InitialLayout);
    for (auto i = names.begin(); i != names.end(); i++)
        layout = layout->addName(*i);

    Stack<Value> bases;
    if (!env->maybeGetAttr(Name::__bases__, bases)) {
        pushStack(gc.create<AttributeError>("Missing __bases__"));
        raiseException();
        return;
    }

    if (!bases.toObject()->is<Tuple>()) {
        pushStack(gc.create<TypeError>("__bases__ is not a tuple"));
        raiseException();
        return;
    }

    Stack<Tuple*> tuple(bases.toObject()->as<Tuple>());
    Stack<Class*> base(Object::ObjectClass);
    if (tuple->len() > 1) {
        pushStack(gc.create<NotImplementedError>(
                             "Multiple inheritance not NYI"));
        raiseException();
        return;
    } else if (tuple->len() == 1) {
        Stack<Value> value(tuple->getitem(0));
        if (!value.toObject()->is<Class>()) {
            pushStack(gc.create<TypeError>("__bases__[0] is not a class"));
            raiseException();
            return;
        }
        base = value.toObject()->as<Class>();
    }

    Class* cls = gc.create<Class>(self->ident, base, layout);
    Stack<Value> value;
    for (auto i = names.begin(); i != names.end(); i++) {
        value = env->getAttr(*i);
        cls->setAttr(*i, value);
    }
    pushStack(cls);
}

INLINE_INSTRS void
Interpreter::executeInstr_Raise(Traced<InstrRaise*> self)
{
    // todo: exceptions must be old-style classes or derived from BaseException
    raiseException();
}

INLINE_INSTRS void
Interpreter::executeInstr_IteratorNext(Traced<InstrIteratorNext*> self)
{
    // The stack is already set up with next method and target on top
    Stack<Value> target(peekStack(2));
    Stack<Value> result;
    pushStack(peekStack(0));
    bool ok = call(target, 1, result);
    if (!ok) {
        Stack<Object*> exc(result.toObject());
        if (exc->is<StopIteration>()) {
            pushStack(Boolean::False);
            return;
        }
    }

    pushStack(result);
    if (ok)
        pushStack(Boolean::True);
    else
        raiseException();
}

void BinaryOpInstr::print(ostream& s) const
{
    s << name() << " " << BinaryOpNames[op()];
}

INLINE_INSTRS void
Interpreter::executeInstr_BinaryOp(Traced<InstrBinaryOp*> self)
{
    Value right = peekStack(0);
    Value left = peekStack(1);
    if (left.isInt32() && right.isInt32()) {
        assert(Integer::ObjectClass->hasAttr(Name::binMethod[self->op()]));
        InstrBinaryOp::replaceWithIntInstr(self, *this);
    } else {
        replaceInstrAndRestart(
            self, gc.create<InstrBinaryOpFallback>(self->op()));
    }
}

/* static */ void
InstrBinaryOp::replaceWithIntInstr(Traced<InstrBinaryOp*> self, Interpreter& interp)
{
    // todo: can use static table
    switch (self->op()) {
#define create_instr(name, token, method, rmethod, imethod)                   \
      case Binary##name:                                                      \
          interp.replaceInstrAndRestart(                                      \
            self, gc.create<InstrBinaryOpInt<Binary##name>>());               \
        return;

    for_each_binary_op(create_instr)
#undef create_instr
      default:
        assert(false);
    }
}

static inline bool maybeCallBinaryOp(Interpreter& interp,
                                     Traced<Value> obj,
                                     Name name,
                                     Traced<Value> left,
                                     Traced<Value> right)
{
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
    return true;
}

INLINE_INSTRS void
Interpreter::executeInstr_BinaryOpFallback(Traced<InstrBinaryOpFallback*> self)
{
    Stack<Value> right(popStack());
    Stack<Value> left(popStack());

    // "If the right operand's type is a subclass of the left operand's type and
    // that subclass provides the reflected method for the operation, this
    // method will be called before the left operand's non-reflected
    // method. This behavior allows subclasses to override their ancestors'
    // operations."
    // todo: needs test

    Stack<Class*> ltype(left.type());
    Stack<Class*> rtype(right.type());
    BinaryOp op = self->op();
    const Name* names = Name::binMethod;
    const Name* rnames = Name::binMethodReflected;
    if (rtype != ltype && rtype->isDerivedFrom(ltype))
    {
        if (maybeCallBinaryOp(*this, right, rnames[op], right, left))
            return;
        if (maybeCallBinaryOp(*this, left, names[op], left, right))
            return;
    } else {
        if (maybeCallBinaryOp(*this, left, names[op], left, right))
            return;
        if (maybeCallBinaryOp(*this, right, rnames[op], right, left))
            return;
    }

    pushStack(gc.create<TypeError>(
                         "unsupported operand type(s) for binary operation"));
    raiseException();
}

template <BinaryOp Op>
inline void
Interpreter::executeBinaryOpInt(Traced<InstrBinaryOpInt<Op>*> self)
{
    if (!peekStack(0).isInt32() || !peekStack(1).isInt32()) {
        replaceInstrAndRestart(
            self, gc.create<InstrBinaryOpFallback>(self->op()));
        return;
    }

    int32_t b = popStack().asInt32();
    int32_t a = popStack().asInt32();
    pushStack(Integer::binaryOp<Op>(a, b));
}

#define define_execute_binary_op_int(name, token, method, rmethod, imethod)   \
void Interpreter::executeInstr_BinaryOpInt_##name(                            \
    Traced<InstrBinaryOpInt_##name*> self)                                    \
{                                                                             \
    executeBinaryOpInt<Binary##name>(self);                                   \
}
for_each_binary_op(define_execute_binary_op_int)
#undef define_execute_binary_op_int

void CompareOpInstr::print(ostream& s) const
{
    s << name() << " " << CompareOpNames[op()];
}

INLINE_INSTRS void
Interpreter::executeInstr_CompareOp(Traced<InstrCompareOp*> self)
{
    Value right = peekStack(0);
    Value left = peekStack(1);
    if (left.isInt32() && right.isInt32()) {
        assert(Integer::ObjectClass->hasAttr(Name::compareMethod[self->op()]));
        InstrCompareOp::replaceWithIntInstr(self, *this);
    } else {
        replaceInstrAndRestart(
            self, gc.create<InstrCompareOpFallback>(self->op()));
    }
}

/* static */ inline void
InstrCompareOp::replaceWithIntInstr(Traced<InstrCompareOp*> self, Interpreter& interp)
{
    // todo: can use static table
    switch (self->op()) {
#define create_instr(name, token, method, rmethod)                            \
      case Compare##name:                                                     \
        interp.replaceInstrAndRestart(                                        \
            self, gc.create<InstrCompareOpInt<Compare##name>>());             \
        return;

    for_each_compare_op(create_instr)
#undef create_instr
      default:
        assert(false);
    }
}

INLINE_INSTRS void
Interpreter::executeInstr_CompareOpFallback(Traced<InstrCompareOpFallback*> self)
{
    Stack<Value> right(popStack());
    Stack<Value> left(popStack());

    // "There are no swapped-argument versions of these methods (to be used when
    // the left argument does not support the operation but the right argument
    // does); rather, __lt__() and __gt__() are each other's reflection,
    // __le__() and __ge__() are each other's reflection, and __eq__() and
    // __ne__() are their own reflection."

    CompareOp op = self->op();
    const Name* names = Name::compareMethod;
    const Name* rnames = Name::compareMethodReflected;
    if (maybeCallBinaryOp(*this, left, names[op], left, right))
        return;
    if (!rnames[op].isNull() &&
        maybeCallBinaryOp(*this, right, rnames[op], right, left)) {
        return;
    }
    if (op == CompareNE &&
        maybeCallBinaryOp(*this, left, names[CompareEQ], left, right))
    {
        if (!isHandlingException()) {
            Stack<Value> result(popStack());
            pushStack(Boolean::get(!result.as<Boolean>()->value()));
        }
        return;
    }

    Stack<Value> result;
    if (op == CompareEQ) {
        result = Boolean::get(left.toObject() == right.toObject());
    } else if (op == CompareNE) {
        result = Boolean::get(left.toObject() != right.toObject());
    } else {
        pushStack(gc.create<TypeError>(
                         "unsupported operand type(s) for compare operation"));
        raiseException();
        return;
    }

    pushStack(result);
}

template <CompareOp Op>
inline void
Interpreter::executeCompareOpInt(Traced<InstrCompareOpInt<Op>*> self)
{
    if (!peekStack(0).isInt32() || !peekStack(1).isInt32()) {
        replaceInstrAndRestart(
            self, gc.create<InstrCompareOpFallback>(self->op()));
        return;
    }

    int32_t b = popStack().asInt32();
    int32_t a = popStack().asInt32();
    pushStack(Integer::compareOp<Op>(a, b));
}

#define define_execute_compare_op_int(name, token, method, rmethod)            \
void Interpreter::executeInstr_CompareOpInt_##name(                            \
    Traced<InstrCompareOpInt_##name*> self)                                    \
{                                                                              \
    executeCompareOpInt<Compare##name>(self);                                  \
}
for_each_compare_op(define_execute_compare_op_int)
#undef define_execute_compare_op_int

INLINE_INSTRS void
Interpreter::executeInstr_AugAssignUpdate(Traced<InstrAugAssignUpdate*> self)
{
    Stack<Value> update(popStack());
    Stack<Value> value(popStack());

    Stack<Value> method;
    Stack<Value> result;
    pushStack(value);
    pushStack(update);
    if (value.maybeGetAttr(Name::augAssignMethod[self->op()], method)) {
        if (!call(method, 2, result)) {
            raiseException(result);
            return;
        }
        pushStack(result);
    } else if (value.maybeGetAttr(Name::binMethod[self->op()], method)) {
        if (!call(method, 2, result)) {
            raiseException(result);
            return;
        }
        pushStack(result);
    } else {
        popStack(2);
        string message = "unsupported operand type(s) for augmented assignment";
        pushStack(gc.create<TypeError>(message));
        raiseException();
    }
}

INLINE_INSTRS void
Interpreter::executeInstr_StartGenerator(Traced<InstrStartGenerator*> self)
{
    Frame* frame = getFrame();
    Stack<Block*> block(frame->block());
    Stack<Env*> env(this->env());
    Stack<GeneratorIter*> gen;
    gen = gc.create<GeneratorIter>(block, env, block->argCount());
    Stack<Value> value(gen);
    pushStack(value);
    gen->suspend(*this, value);
}

INLINE_INSTRS void
Interpreter::executeInstr_ResumeGenerator(Traced<InstrResumeGenerator*> self)
{
    // todo: get slot 0
    Stack<GeneratorIter*> gen(
        env()->getAttr("self").asObject()->as<GeneratorIter>());
    gen->resume(*this);
}

INLINE_INSTRS void
Interpreter::executeInstr_LeaveGenerator(Traced<InstrLeaveGenerator*> self)
{
    Stack<GeneratorIter*> gen(getGeneratorIter());
    gen->leave(*this);
}

INLINE_INSTRS void
Interpreter::executeInstr_SuspendGenerator(Traced<InstrSuspendGenerator*> self)
{
    Stack<Value> value(popStack());
    Stack<GeneratorIter*> gen(getGeneratorIter());
    gen->suspend(*this, value);
}

INLINE_INSTRS void
Interpreter::executeInstr_EnterCatchRegion(Traced<InstrEnterCatchRegion*> self)
{
    pushExceptionHandler(ExceptionHandler::CatchHandler, self->offset_);
}

INLINE_INSTRS void
Interpreter::executeInstr_LeaveCatchRegion(Traced<InstrLeaveCatchRegion*> self)
{
    popExceptionHandler(ExceptionHandler::CatchHandler);
}

INLINE_INSTRS void
Interpreter::executeInstr_MatchCurrentException(Traced<InstrMatchCurrentException*> self)
{
    Stack<Object*> obj(popStack().toObject());
    Stack<Exception*> exception(currentException());
    bool match = obj->is<Class>() && exception->isInstanceOf(obj->as<Class>());
    if (match) {
        finishHandlingException();
        pushStack(exception);
    }
    pushStack(Boolean::get(match));
}

INLINE_INSTRS void
Interpreter::executeInstr_HandleCurrentException(Traced<InstrHandleCurrentException*> self)
{
    finishHandlingException();
}

INLINE_INSTRS void
Interpreter::executeInstr_EnterFinallyRegion(Traced<InstrEnterFinallyRegion*> self)
{
    pushExceptionHandler(ExceptionHandler::FinallyHandler, self->offset_);
}

INLINE_INSTRS void
Interpreter::executeInstr_LeaveFinallyRegion(Traced<InstrLeaveFinallyRegion*> self)
{
    popExceptionHandler(ExceptionHandler::FinallyHandler);
}

INLINE_INSTRS void
Interpreter::executeInstr_FinishExceptionHandler(Traced<InstrFinishExceptionHandler*> self)
{
    return maybeContinueHandlingException();
}

INLINE_INSTRS void
Interpreter::executeInstr_LoopControlJump(Traced<InstrLoopControlJump*> self)
{
    loopControlJump(self->finallyCount(), self->target());
}

INLINE_INSTRS void
Interpreter::executeInstr_AssertStackDepth(Traced<InstrAssertStackDepth*> self)
{
#ifdef DEBUG
    Frame* frame = getFrame();
    unsigned depth = stackPos() - frame->stackPos();
    if (depth != self->expected_) {
        cerr << "Excpected stack depth " << dec << self->expected_;
        cerr << " but got " << depth << " in: " << endl;
        cerr << *getFrame()->block() << endl;
        assert(depth == self->expected_);
    }
#endif
}

bool Interpreter::run(MutableTraced<Value> resultOut)
{
    static const void* dispatchTable[] =
    {
#define dispatch_table_entry(it)                                              \
        &&instr_##it,
    for_each_inline_instr(dispatch_table_entry)
    for_each_outofline_instr(dispatch_table_entry)
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
        Value value = popStack();
        returnFromFrame(value);
        if (!instrp)
            goto exit;
        dispatch();
    }

#define handle_instr(it)                                                      \
    instr_##it: {                                                             \
        Instr##it** ip = reinterpret_cast<Instr##it**>(&thunk->data);         \
        executeInstr_##it(Traced<Instr##it*>::fromTracedLocation(ip));        \
        assert(instrp);                                                       \
        dispatch();                                                           \
    }
    for_each_outofline_instr(handle_instr)
#undef handle_instr

  exit:
    assert(!instrp);
    resultOut = popStack();
    return true;
}
