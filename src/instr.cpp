#include "instr.h"

#include "builtin.h"
#include "common.h"
#include "dict.h"
#include "exception.h"
#include "generator.h"
#include "interp.h"
#include "singletons.h"
#include "list.h"
#include "utils.h"

#define INLINE_INSTRS /*inline*/

const char* instrName(InstrType type)
{
    static const char* names[InstrTypeCount] = {
#define instr_name(name)                                                      \
        #name,

    for_each_inline_instr(instr_name)
    for_each_outofline_instr(instr_name)
#undef instr_enum
    };

    assert(type < InstrTypeCount);
    return names[type];
}

INLINE_INSTRS void
Interpreter::executeInstr_Const(Traced<InstrConst*> instr)
{
    pushStack(instr->value());
}

INLINE_INSTRS void
Interpreter::executeInstr_GetStackLocal(Traced<InstrGetStackLocal*> instr)
{
    // Name was present when compiled, but may have been deleted.
    {
        AutoAssertNoGC nogc;
        Value value = getStackLocal(instr->slot);
        if (value != Value(UninitializedSlot)) {
            pushStack(value);
            return;
        }
    }

    raiseNameError(instr->ident);
}

INLINE_INSTRS void
Interpreter::executeInstr_SetStackLocal(Traced<InstrSetStackLocal*> instr)
{
    AutoAssertNoGC nogc;
    Value value = peekStack(0);
    assert(value != Value(UninitializedSlot));
    setStackLocal(instr->slot, value);
}

INLINE_INSTRS void
Interpreter::executeInstr_DelStackLocal(Traced<InstrDelStackLocal*> instr)
{
    // Delete by setting slot value to UninitializedSlot.
    {
        AutoAssertNoGC nogc;
        if (getStackLocal(instr->slot) != Value(UninitializedSlot)) {
            setStackLocal(instr->slot, UninitializedSlot);
            return;
        }
    }

    raiseNameError(instr->ident);
}

INLINE_INSTRS void
Interpreter::executeInstr_GetLexical(Traced<InstrGetLexical*> instr)
{
    Stack<Env*> env(lexicalEnv(instr->frameIndex));
    Stack<Value> value;
    // Name was present when compiled, but may have been deleted.
    if (!env->maybeGetAttr(instr->ident, value)) {
        raiseNameError(instr->ident);
        return;
    }
    pushStack(value);
}

INLINE_INSTRS void
Interpreter::executeInstr_SetLexical(Traced<InstrSetLexical*> instr)
{
    Stack<Value> value(peekStack(0));
    Stack<Env*> env(lexicalEnv(instr->frameIndex));
    env->setAttr(instr->ident, value);
}

INLINE_INSTRS void
Interpreter::executeInstr_DelLexical(Traced<InstrDelLexical*> instr)
{
    Stack<Env*> env(lexicalEnv(instr->frameIndex));
    if (!env->maybeDelOwnAttr(instr->ident))
        raiseNameError(instr->ident);
}

INLINE_INSTRS void
Interpreter::executeInstr_GetGlobal(Traced<InstrGetGlobal*> instr)
{
    Stack<Value> value;
    if (instr->global()->maybeGetAttr(instr->ident, value)) {
        assert(value.toObject());
        pushStack(value);
        return;
    }

    Stack<Value> builtins;
    if (instr->global()->maybeGetAttr(Name::__builtins__, builtins) &&
        builtins.maybeGetAttr(instr->ident, value))
    {
        assert(value.toObject());
        pushStack(value);
        return;
    }

    raiseNameError(instr->ident);
}

INLINE_INSTRS void
Interpreter::executeInstr_SetGlobal(Traced<InstrSetGlobal*> instr)
{
    Stack<Value> value(peekStack(0));
    instr->global()->setAttr(instr->ident, value);
}

INLINE_INSTRS void
Interpreter::executeInstr_DelGlobal(Traced<InstrDelGlobal*> instr)
{
    if (!instr->global()->maybeDelOwnAttr(instr->ident))
        raiseNameError(instr->ident);
}

INLINE_INSTRS void
Interpreter::executeInstr_GetAttr(Traced<InstrGetAttr*> instr)
{
    Stack<Value> value(popStack());
    Stack<Value> result;
    bool ok = getAttr(value, instr->ident, result);
    pushStack(result);
    if (!ok)
        raiseException();
}

INLINE_INSTRS void
Interpreter::executeInstr_SetAttr(Traced<InstrSetAttr*> instr)
{
    Stack<Value> value(peekStack(1));
    Stack<Object*> obj(popStack().toObject());
    if (builtinsInitialised) {
        if (obj->isInstanceOf<Class>()) {
            if (obj->as<Class>()->isFinal()) {
                raiseTypeError(
                    "can't set attributes of built-in/extension type");
                return;
            }
        } else if (obj->type()->isFinal() && !obj->hasAttr(instr->ident)) {
            raiseAttrError(value, instr->ident);
            return;
        }
    }

    Stack<Value> result;
    if (!setAttr(obj, instr->ident, value, result))
        raiseException(result);
}

INLINE_INSTRS void
Interpreter::executeInstr_DelAttr(Traced<InstrDelAttr*> instr)
{
    Stack<Object*> obj(popStack().toObject());
    Stack<Value> result;
    if (!delAttr(obj, instr->ident, result))
        raiseException(result);
}

INLINE_INSTRS void
Interpreter::executeInstr_Call(Traced<InstrCall*> instr)
{
    Stack<Value> target(peekStack(instr->count));
    removeStackEntries(instr->count, 1);
    startCall(target, instr->count);
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
 * value as the instr parameter if necessary by simply incrementing the argument
 * count.
 */

bool Interpreter::getMethod(Name ident)
{
    Stack<Value> value(popStack());
    Stack<Value> result;
    bool isCallableDescriptor;
    if (!getMethodAttr(value, ident, result, isCallableDescriptor)) {
        raiseAttrError(value, ident);
        return false;
    }

    pushStack(result, Boolean::get(isCallableDescriptor), value);
    return true;
}

INLINE_INSTRS void
Interpreter::executeInstr_GetMethod(Traced<InstrGetMethod*> instr)
{
    Stack<Value> value(peekStack(0));
    if (!getMethod(instr->ident))
        return;

    if (value.type()->isFinal()) {
        // Builtin classes cannot be changed, so we can cache the lookup.
        Stack<Class*> cls(value.type());
        Stack<Value> result(peekStack(2));
        replaceInstr(instr, gc.create<InstrGetMethodBuiltin>(instr->ident, cls, result));
    } else {
        replaceInstr(instr, gc.create<InstrGetMethodFallback>(instr->ident));
    }
}

INLINE_INSTRS void
Interpreter::executeInstr_GetMethodFallback(Traced<InstrGetMethodFallback*> instr)
{
    getMethod(instr->ident);
}

INLINE_INSTRS void
Interpreter::executeInstr_GetMethodBuiltin(Traced<InstrGetMethodBuiltin*> instr)
{
    {
        AutoAssertNoGC nogc;
        Value value = peekStack(0);
        if (value.type() == instr->class_) {
            popStack();
            pushStack(instr->result_, Boolean::True, value);
            return;
        }
    }

    replaceInstrAndRestart(instr, gc.create<InstrGetMethodFallback>(instr->ident));
}

INLINE_INSTRS void
Interpreter::executeInstr_CallMethod(Traced<InstrCallMethod*> instr)
{
    bool extraArg = peekStack(instr->count + 1).as<Boolean>()->value();
    Stack<Value> target(peekStack(instr->count + 2));
    unsigned argCount = instr->count + (extraArg ? 1 : 0);
    removeStackEntries(argCount, extraArg ? 2 : 3);
    return startCall(target, argCount);
}

INLINE_INSTRS void
Interpreter::executeInstr_CreateEnv(Traced<InstrCreateEnv*> instr)
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
Interpreter::executeInstr_InitStackLocals(Traced<InstrInitStackLocals*> instr)
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
Interpreter::executeInstr_In(Traced<InstrIn*> instr)
{
    // todo: implement this
    // https://docs.python.org/3/reference/expressions.html#membership-test-details

    Stack<Object*> container(popStack().toObject());
    Stack<Value> value(popStack());

    Stack<Value> type(container->type());
    Stack<Value> contains;
    bool isCallableDescriptor;
    if (!getMethodAttr(type, Name::__contains__, contains,
                       isCallableDescriptor))
    {
        pushStack(gc.create<TypeError>("Argument is not iterable"));
        raiseException();
        return;
    }

    if (isCallableDescriptor)
        pushStack(container);
    pushStack(value);
    startCall(contains, isCallableDescriptor ? 2 : 1);
}

INLINE_INSTRS void
Interpreter::executeInstr_Is(Traced<InstrIs*> instr)
{
    Value b = popStack();
    Value a = popStack();
    bool result = a.toObject() == b.toObject();
    pushStack(Boolean::get(result));
}

INLINE_INSTRS void
Interpreter::executeInstr_Not(Traced<InstrNot*> instr)
{
    // the following values are interpreted as false: False, None, numeric zero
    // of all types, and empty strings and containers (including strings,
    // tuples, lists, dictionaries, sets and frozensets).

    Object* obj = popStack().toObject();
    pushStack(Boolean::get(!obj->isTrue()));
}

INLINE_INSTRS void
Interpreter::executeInstr_BranchAlways(Traced<InstrBranchAlways*> instr)
{
    assert(instr->offset_);
    branch(instr->offset_);
}

INLINE_INSTRS void
Interpreter::executeInstr_BranchIfTrue(Traced<InstrBranchIfTrue*> instr)
{
    assert(instr->offset_);
    Object *x = popStack().toObject();
    if (x->isTrue())
        branch(instr->offset_);
}

INLINE_INSTRS void
Interpreter::executeInstr_BranchIfFalse(Traced<InstrBranchIfFalse*> instr)
{
    assert(instr->offset_);
    Object *x = popStack().toObject();
    if (!x->isTrue())
        branch(instr->offset_);
}

INLINE_INSTRS void
Interpreter::executeInstr_Or(Traced<InstrOr*> instr)
{
    // The expression |x or y| first evaluates x; if x is true, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    assert(instr->offset_);
    Object *x = peekStack(0).toObject();
    if (x->isTrue()) {
        branch(instr->offset_);
        return;
    }

    popStack();
}

INLINE_INSTRS void
Interpreter::executeInstr_And(Traced<InstrAnd*> instr)
{
    // The expression |x and y| first evaluates x; if x is false, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    assert(instr->offset_);
    Object *x = peekStack(0).toObject();
    if (!x->isTrue()) {
        branch(instr->offset_);
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
Interpreter::executeInstr_Lambda(Traced<InstrLambda*> instr)
{
    Stack<FunctionInfo*> info(instr->functionInfo());
    Stack<Block*> block(instr->block());
    Stack<Env*> env(this->env());

    Stack<Object*> obj;
    obj = gc.create<Function>(instr->functionName(), info,
                              stackSlice(instr->defaultCount()), env);
    popStack(instr->defaultCount());
    pushStack(Value(obj));
}

INLINE_INSTRS void
Interpreter::executeInstr_Dup(Traced<InstrDup*> instr)
{
    pushStack(peekStack(instr->index));
}

INLINE_INSTRS void
Interpreter::executeInstr_Pop(Traced<InstrPop*> instr)
{
    popStack();
}

INLINE_INSTRS void
Interpreter::executeInstr_Swap(Traced<InstrSwap*> instr)
{
    swapStack();
}

INLINE_INSTRS void
Interpreter::executeInstr_Tuple(Traced<InstrTuple*> instr)
{
    Tuple* tuple = Tuple::get(stackSlice(instr->size));
    popStack(instr->size);
    pushStack(tuple);
}

INLINE_INSTRS void
Interpreter::executeInstr_List(Traced<InstrList*> instr)
{
    List* list = gc.create<List>(stackSlice(instr->size));
    popStack(instr->size);
    pushStack(list);
}

INLINE_INSTRS void
Interpreter::executeInstr_Dict(Traced<InstrDict*> instr)
{
    Dict* dict = gc.create<Dict>(stackSlice(instr->size * 2));
    popStack(instr->size * 2);
    pushStack(dict);
}

INLINE_INSTRS void
Interpreter::executeInstr_Slice(Traced<InstrSlice*> instr)
{
    Slice* slice = gc.create<Slice>(stackSlice(3));
    popStack(3);
    pushStack(slice);
}

INLINE_INSTRS void
Interpreter::executeInstr_AssertionFailed(Traced<InstrAssertionFailed*> instr)
{
    Object* obj = popStack().toObject();
    assert(obj->is<String>() || obj == None);
    string message = obj != None ? obj->as<String>()->value() : "";
    pushStack(gc.create<AssertionError>(message));
    raiseException();
}

INLINE_INSTRS void
Interpreter::executeInstr_MakeClassFromFrame(Traced<InstrMakeClassFromFrame*> instr)
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

    Stack<Tuple*> tuple(bases.as<Tuple>());
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
        base = value.as<Class>();
    }

    Class* cls = gc.create<Class>(instr->ident, base, layout);
    Stack<Value> value;
    for (auto i = names.begin(); i != names.end(); i++) {
        value = env->getAttr(*i);
        cls->setAttr(*i, value);
    }
    pushStack(cls);
}

INLINE_INSTRS void
Interpreter::executeInstr_Raise(Traced<InstrRaise*> instr)
{
    // todo: exceptions must be old-style classes or derived from BaseException
    raiseException();
}

INLINE_INSTRS void
Interpreter::executeInstr_GetIterator(Traced<InstrGetIterator*> instr)
{
    Stack<Value> target(popStack());
    Stack<Value> method;
    bool isCallableDescriptor;
    Stack<Value> result;

    // Call __iter__ method to get iterator if it's present.
    if (getMethodAttr(target, Name::__iter__, method, isCallableDescriptor)) {
        if (isCallableDescriptor)
            pushStack(target);
        startCall(method, isCallableDescriptor ? 1 : 0);
        return;
    }

    // Otherwise create a SequenceIterator wrapping the target iterable.
    if (!getMethodAttr(target, Name::__getitem__, method, isCallableDescriptor))
    {
        raiseTypeError("Object not iterable");
        return;
    }

    pushStack(target);
    startCall(SequenceIterator, 1);
}

INLINE_INSTRS void
Interpreter::executeInstr_IteratorNext(Traced<InstrIteratorNext*> instr)
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

bool Interpreter::maybeCallBinaryOp(Traced<Value> obj, Name name,
                                    Traced<Value> left, Traced<Value> right,
                                    MutableTraced<Value> method)
{
    if (!obj.maybeGetAttr(name, method))
        return false;

    Stack<Value> result;
    pushStack(left);
    pushStack(right);
    if (!call(method, 2, result)) {
        pushStack(result);
        raiseException();
        return true;
    }

    if (result == Value(NotImplemented))
        return false;

    pushStack(result);
    return true;
}

bool
Interpreter::executeBinaryOp(BinaryOp op, MutableTraced<Value> method)
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
    const Name* names = Name::binMethod;
    const Name* rnames = Name::binMethodReflected;
    if (rtype != ltype && rtype->isDerivedFrom(ltype))
    {
        if (maybeCallBinaryOp(right, rnames[op], right, left, method))
            return true;
        if (maybeCallBinaryOp(left, names[op], left, right, method))
            return true;
    } else {
        if (maybeCallBinaryOp(left, names[op], left, right, method))
            return true;
        if (maybeCallBinaryOp(right, rnames[op], right, left, method))
            return true;
    }

    pushStack(gc.create<TypeError>(
                         "unsupported operand type(s) for binary operation"));
    raiseException();
    return false;
}

void BinaryOpInstr::print(ostream& s) const
{
    s << name() << " " << BinaryOpNames[op];
}

bool ShouldInlineBinaryOp(BinaryOp op) {
    // Must match for_each_binary_op_to_inline macro in instr.h
    return op <= BinaryTrueDiv;
}

static Instr* InlineInstrForIntBinaryOp(BinaryOp op)
{
    // todo: could use static table
    switch (op) {
#define create_instr(name)                                                    \
      case Binary##name:                                                      \
          return gc.create<InstrBinaryOpInt<Binary##name>>();
    for_each_binary_op_to_inline(create_instr)
#undef create_instr
      default:
        crash("Unexpected binary op");
        return nullptr;
    }
}

INLINE_INSTRS void
Interpreter::executeInstr_BinaryOp(Traced<InstrBinaryOp*> instr)
{
    Stack<Value> right(peekStack(0));
    Stack<Value> left(peekStack(1));

    // If both arguments are integers, inline the operation and restart.
    if (ShouldInlineBinaryOp(instr->op) &&
        left.isInt32() && right.isInt32())
    {
        assert(Integer::ObjectClass->hasAttr(Name::binMethod[instr->op]));
        replaceInstrAndRestart(instr, InlineInstrForIntBinaryOp(instr->op));
        return;
    }

    // Find the method to call and execute it.  If successful and both arguments
    // are instances of builtin classes, cache the method.
    Stack<Value> method;
    if (executeBinaryOp(instr->op, method) &&
        left.type()->isFinal() && right.type()->isFinal())
    {
        Stack<Class*> leftClass(left.type());
        Stack<Class*> rightClass(right.type());
        replaceInstr(instr, gc.create<InstrBinaryOpBuiltin>(
                         instr->op, leftClass, rightClass, method));
        return;
    }

    // Replace with fallback instruction.
    replaceInstr(instr, gc.create<InstrBinaryOpFallback>(instr->op));
}

INLINE_INSTRS void
Interpreter::executeInstr_BinaryOpFallback(Traced<InstrBinaryOpFallback*> instr)
{
    Stack<Value> method;
    executeBinaryOp(instr->op, method);
}

template <BinaryOp Op>
inline void
Interpreter::executeBinaryOpInt(Traced<InstrBinaryOpInt<Op>*> instr)
{
    assert(ShouldInlineBinaryOp(instr->op));

    if (!peekStack(0).isInt32() || !peekStack(1).isInt32()) {
        replaceInstrAndRestart(
            instr, gc.create<InstrBinaryOpFallback>(instr->op));
        return;
    }

    int32_t b = popStack().asInt32();
    int32_t a = popStack().asInt32();
    pushStack(Integer::binaryOp<Op>(a, b));
}

#define define_execute_binary_op_int(name)                                    \
    void Interpreter::executeInstr_BinaryOpInt_##name(                        \
        Traced<InstrBinaryOpInt_##name*> instr)                               \
    {                                                                         \
        executeBinaryOpInt<Binary##name>(instr);                              \
    }
for_each_binary_op_to_inline(define_execute_binary_op_int)
#undef define_execute_binary_op_int

InstrBinaryOpBuiltin::InstrBinaryOpBuiltin(BinaryOp op,
                                           Traced<Class*> left,
                                           Traced<Class*> right,
                                           Traced<Value> method)
  : BinaryOpInstr(op),
    left_(left),
    right_(right),
    method_(method)
{}

void InstrBinaryOpBuiltin::traceChildren(Tracer& t)
{
    gc.trace(t, &left_);
    gc.trace(t, &right_);
    gc.trace(t, &method_);
}

INLINE_INSTRS void
Interpreter::executeInstr_BinaryOpBuiltin(Traced<InstrBinaryOpBuiltin*> instr)
{
    Value right = peekStack(0);
    Value left = peekStack(1);

    if (left.type() != instr->left() || right.type() != instr->right()) {
        replaceInstrAndRestart(
            instr, gc.create<InstrBinaryOpFallback>(instr->op));
        return;
    }

    Stack<Value> method(instr->method());
    startCall(method, 2);
}

void CompareOpInstr::print(ostream& s) const
{
    s << name() << " " << CompareOpNames[op];
}

static Instr* InlineInstrForIntCompareOp(CompareOp op)
{
    // todo: could use static table
    switch (op) {
#define create_instr(name, token, method, rmethod)                            \
      case Compare##name:                                                     \
          return gc.create<InstrCompareOpInt<Compare##name>>();
    for_each_compare_op(create_instr)
#undef create_instr
      default:
        crash("unexpected compare op");
        return nullptr;
    }
}

INLINE_INSTRS void
Interpreter::executeInstr_CompareOp(Traced<InstrCompareOp*> instr)
{
    Value right = peekStack(0);
    Value left = peekStack(1);
    Instr* replacement;
    if (left.isInt32() && right.isInt32()) {
        assert(Integer::ObjectClass->hasAttr(Name::compareMethod[instr->op]));
        replacement = InlineInstrForIntCompareOp(instr->op);
    } else {
        replacement = gc.create<InstrCompareOpFallback>(instr->op);
    }
    replaceInstrAndRestart(instr, replacement);
}


INLINE_INSTRS void
Interpreter::executeInstr_CompareOpFallback(Traced<InstrCompareOpFallback*> instr)
{
    Stack<Value> right(popStack());
    Stack<Value> left(popStack());

    // "There are no swapped-argument versions of these methods (to be used when
    // the left argument does not support the operation but the right argument
    // does); rather, __lt__() and __gt__() are each other's reflection,
    // __le__() and __ge__() are each other's reflection, and __eq__() and
    // __ne__() are their own reflection."

    CompareOp op = instr->op;
    const Name* names = Name::compareMethod;
    const Name* rnames = Name::compareMethodReflected;
    Stack<Value> method;
    if (maybeCallBinaryOp(left, names[op], left, right, method))
        return;
    if (!rnames[op].isNull() &&
        maybeCallBinaryOp(right, rnames[op], right, left, method)) {
        return;
    }
    if (op == CompareNE &&
        maybeCallBinaryOp(left, names[CompareEQ], left, right, method))
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
Interpreter::executeCompareOpInt(Traced<InstrCompareOpInt<Op>*> instr)
{
    if (!peekStack(0).isInt32() || !peekStack(1).isInt32()) {
        replaceInstrAndRestart(
            instr, gc.create<InstrCompareOpFallback>(instr->op));
        return;
    }

    int32_t b = popStack().asInt32();
    int32_t a = popStack().asInt32();
    pushStack(Integer::compareOp<Op>(a, b));
}

#define define_execute_compare_op_int(name, token, method, rmethod)            \
void Interpreter::executeInstr_CompareOpInt_##name(                            \
    Traced<InstrCompareOpInt_##name*> instr)                                    \
{                                                                              \
    executeCompareOpInt<Compare##name>(instr);                                  \
}
for_each_compare_op(define_execute_compare_op_int)
#undef define_execute_compare_op_int

bool
Interpreter::executeAugAssignUpdate(BinaryOp op, MutableTraced<Value> method,
                                    bool& isCallableDescriptor)
{
    Stack<Value> update(popStack());
    Stack<Value> value(popStack());

    Stack<Value> type(value.type());
    if (!getMethodAttr(type, Name::augAssignMethod[op], method,
                       isCallableDescriptor) &&
        !getMethodAttr(type, Name::binMethod[op], method,
                       isCallableDescriptor))
    {
        string message = "unsupported operand type(s) for augmented assignment";
        pushStack(gc.create<TypeError>(message));
        raiseException();
        return false;
    }

    if (isCallableDescriptor)
        pushStack(value);
    pushStack(update);
    Stack<Value> result;
    bool ok = call(method, isCallableDescriptor ? 2 : 1, result);
    pushStack(result);
    if (!ok)
        raiseException();

    return true;
}

INLINE_INSTRS void
Interpreter::executeInstr_AugAssignUpdate(Traced<InstrAugAssignUpdate*> instr)
{
    Stack<Value> right(peekStack(0));
    Stack<Value> left(peekStack(1));

    // Find the method to call and execute it.
    Stack<Value> method;
    bool isCallableDescriptor;
    if (!executeAugAssignUpdate(instr->op, method, isCallableDescriptor))
        return;

    // If successful and both arguments are instances of builtin classes, cache
    // the method.
    if (left.type()->isFinal() && right.type()->isFinal())
    {
        assert(isCallableDescriptor);
        Stack<Class*> leftClass(left.type());
        Stack<Class*> rightClass(right.type());
        replaceInstr(instr, gc.create<InstrAugAssignUpdateBuiltin>(
                         instr->op, leftClass, rightClass, method));
        return;
    }

    // Otherwise replace with fallback instruction.
    replaceInstr(instr, gc.create<InstrAugAssignUpdateFallback>(instr->op));
}

INLINE_INSTRS void
Interpreter::executeInstr_AugAssignUpdateFallback(Traced<InstrAugAssignUpdateFallback*> instr)
{
    Stack<Value> method;
    bool isCallableDescriptor;
    executeAugAssignUpdate(instr->op, method, isCallableDescriptor);
}

InstrAugAssignUpdateBuiltin::InstrAugAssignUpdateBuiltin(BinaryOp op,
                                                         Traced<Class*> left,
                                                         Traced<Class*> right,
                                                         Traced<Value> method)
  : BinaryOpInstr(op),
    left_(left),
    right_(right),
    method_(method)
{}

void InstrAugAssignUpdateBuiltin::traceChildren(Tracer& t)
{
    gc.trace(t, &left_);
    gc.trace(t, &right_);
    gc.trace(t, &method_);
}

INLINE_INSTRS void
Interpreter::executeInstr_AugAssignUpdateBuiltin(Traced<InstrAugAssignUpdateBuiltin*> instr)
{
    Value right = peekStack(0);
    Value left = peekStack(1);

    if (left.type() != instr->left() || right.type() != instr->right()) {
        replaceInstrAndRestart(
            instr, gc.create<InstrAugAssignUpdateFallback>(instr->op));
        return;
    }

    Stack<Value> method(instr->method());
    startCall(method, 2);
}

INLINE_INSTRS void
Interpreter::executeInstr_StartGenerator(Traced<InstrStartGenerator*> instr)
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
Interpreter::executeInstr_ResumeGenerator(Traced<InstrResumeGenerator*> instr)
{
    // todo: get slot 0
    Stack<GeneratorIter*> gen(
        env()->getAttr("self").asObject()->as<GeneratorIter>());
    gen->resume(*this);
}

INLINE_INSTRS void
Interpreter::executeInstr_LeaveGenerator(Traced<InstrLeaveGenerator*> instr)
{
    Stack<GeneratorIter*> gen(getGeneratorIter());
    gen->leave(*this);
}

INLINE_INSTRS void
Interpreter::executeInstr_SuspendGenerator(Traced<InstrSuspendGenerator*> instr)
{
    Stack<Value> value(popStack());
    Stack<GeneratorIter*> gen(getGeneratorIter());
    gen->suspend(*this, value);
}

INLINE_INSTRS void
Interpreter::executeInstr_EnterCatchRegion(Traced<InstrEnterCatchRegion*> instr)
{
    pushExceptionHandler(ExceptionHandler::CatchHandler, instr->offset_);
}

INLINE_INSTRS void
Interpreter::executeInstr_LeaveCatchRegion(Traced<InstrLeaveCatchRegion*> instr)
{
    popExceptionHandler(ExceptionHandler::CatchHandler);
}

INLINE_INSTRS void
Interpreter::executeInstr_MatchCurrentException(Traced<InstrMatchCurrentException*> instr)
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
Interpreter::executeInstr_HandleCurrentException(Traced<InstrHandleCurrentException*> instr)
{
    finishHandlingException();
}

INLINE_INSTRS void
Interpreter::executeInstr_EnterFinallyRegion(Traced<InstrEnterFinallyRegion*> instr)
{
    pushExceptionHandler(ExceptionHandler::FinallyHandler, instr->offset_);
}

INLINE_INSTRS void
Interpreter::executeInstr_LeaveFinallyRegion(Traced<InstrLeaveFinallyRegion*> instr)
{
    popExceptionHandler(ExceptionHandler::FinallyHandler);
}

INLINE_INSTRS void
Interpreter::executeInstr_FinishExceptionHandler(Traced<InstrFinishExceptionHandler*> instr)
{
    return maybeContinueHandlingException();
}

INLINE_INSTRS void
Interpreter::executeInstr_LoopControlJump(Traced<InstrLoopControlJump*> instr)
{
    loopControlJump(instr->finallyCount(), instr->target());
}

INLINE_INSTRS void
Interpreter::executeInstr_AssertStackDepth(Traced<InstrAssertStackDepth*> instr)
{
#ifdef DEBUG
    Frame* frame = getFrame();
    unsigned depth = stackPos() - frame->stackPos();
    if (depth != instr->expected) {
        cerr << "Excpected stack depth " << dec << instr->expected;
        cerr << " but got " << depth << " in: " << endl;
        cerr << *getFrame()->block() << endl;
        assert(depth == instr->expected);
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
        assert(resultOut.isInstanceOf(Exception::ObjectClass));
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
