#include "instr.h"

#include "builtin.h"
#include "common.h"
#include "dict.h"
#include "exception.h"
#include "generator.h"
#include "interp.h"
#include "singletons.h"
#include "slice.h"
#include "list.h"
#include "utils.h"

InstrType instrType(InstrCode code)
{
#define define_instr_type(name, cls)                                          \
        InstrType_##cls,

    static InstrType types[InstrCodeCount] = {
        for_each_instr(define_instr_type)
    };

#undef define_instr_enum

    assert(code < InstrCodeCount);
    return types[code];
}

const char* instrName(InstrCode code)
{
#define define_instr_name(name, cls)                                          \
        #name,

    static const char* names[InstrCodeCount] = {
        for_each_instr(define_instr_name)
    };

#undef define_instr_enum

    assert(code < InstrCodeCount);
    return names[code];
}

Instr* getNextInstr(Instr* instr)
{
#define define_non_stub_case(name, cls)                                       \
      case Instr_##name:                                                      \
        return nullptr;

#define define_stub_case(name, cls)                                           \
      case Instr_##name:                                                      \
        return static_cast<cls*>(instr)->next();

    switch (instr->code()) {
        for_each_inline_instr(define_non_stub_case)
        for_each_outofline_instr(define_non_stub_case)
        for_each_stub_instr(define_stub_case)
      default:
        assert(false);
        return nullptr;
    }

#undef define_non_stub_case
#undef define_stub_case
}

Instr* getFinalInstr(Instr* instr) {
    while (Instr* next = getNextInstr(instr))
        instr = next;
    return instr;
}

void Instr::print(ostream& s) const
{
    s << instrName(code_);
    if (stubCount_)
        s << " " << dec << stubCount_;
}

void IdentInstrBase::print(ostream& s) const
{
    Instr::print(s);
    s << " " << ident;
}

void StackSlotInstr::print(ostream& s) const
{
    Instr::print(s);
    s << " " << ident << " " << slot;
}

void LexicalFrameInstr::print(ostream& s) const
{
    Instr::print(s);
    s << " " << frameIndex << " " << ident;
}

void CountInstr::print(ostream& s) const
{
    Instr::print(s);
    s << " " << count;
}

void CountStubInstr::print(ostream& s) const
{
    StubInstr::print(s);
    s << " " << count;
}

void IndexInstr::print(ostream& s) const
{
    Instr::print(s);
    s << " " << index;
}

void ValueInstr::print(ostream& s) const
{
    Instr::print(s);
    s << " " << value_;
}

void ValueInstr::traceChildren(Tracer& t)
{
    gc.trace(t, &value_);
}

void BuiltinMethodInstr::traceChildren(Tracer& t)
{
    StubInstr::traceChildren(t);
    gc.trace(t, &class_);
    gc.trace(t, &result_);
}

void BranchInstr::print(ostream& s) const
{
    Instr::print(s);
    s << " " << offset_;
}

void LambdaInstr::print(ostream& s) const
{
    Instr::print(s);
    for (auto i = info_->params_.begin(); i != info_->params_.end(); ++i) {
        s << " " << *i;
    }
}

void LambdaInstr::traceChildren(Tracer& t)
{
    gc.trace(t, &info_);
}

void StubInstr::print(ostream& s) const
{
    next_->print(s);
    s << " <- ";
    Instr::print(s);
}

void StubInstr::traceChildren(Tracer& t)
{
    gc.trace(t, &next_);
}

void BinaryOpInstr::print(ostream& s) const
{
    Instr::print(s);
    s << " " << BinaryOpNames[op];
}

void BuiltinBinaryOpInstr::print(ostream& s) const
{
    StubInstr::print(s);
    s << " " << hex << method_.toObject();
}

void BuiltinBinaryOpInstr::traceChildren(Tracer& t)
{
    StubInstr::traceChildren(t);
    gc.trace(t, &left_);
    gc.trace(t, &right_);
    gc.trace(t, &method_);
}

void CompareOpInstr::print(ostream& s) const
{
    Instr::print(s);
    s << " " << CompareOpNames[op];
}

void CompareOpStubInstr::print(ostream& s) const
{
    StubInstr::print(s);
}

void LoopControlJumpInstr::print(ostream& s) const
{
    Instr::print(s);
    s << " " << finallyCount_ << " " << target_;
}

void
Interpreter::executeInstr_Const(Traced<ValueInstr*> instr)
{
    pushStack(instr->value());
}

void
Interpreter::executeInstr_GetStackLocal(Traced<StackSlotInstr*> instr)
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

void
Interpreter::executeInstr_SetStackLocal(Traced<StackSlotInstr*> instr)
{
    AutoAssertNoGC nogc;
    Value value = peekStack();
    assert(value != Value(UninitializedSlot));
    setStackLocal(instr->slot, value);
}

void
Interpreter::executeInstr_DelStackLocal(Traced<StackSlotInstr*> instr)
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

void
Interpreter::executeInstr_GetLexical(Traced<LexicalFrameInstr*> instr)
{
    Stack<Env*> env(lexicalEnv(instr->frameIndex));
    assert(env);

    Stack<Value> value;
    // Name was present when compiled, but may have been deleted.
    if (!env->maybeGetAttr(instr->ident, value)) {
        raiseNameError(instr->ident);
        return;
    }
    pushStack(value);
}

void
Interpreter::executeInstr_SetLexical(Traced<LexicalFrameInstr*> instr)
{
    Stack<Value> value(peekStack());
    Stack<Env*> env(lexicalEnv(instr->frameIndex));
    assert(env);

    env->setAttr(instr->ident, value);
}

void
Interpreter::executeInstr_DelLexical(Traced<LexicalFrameInstr*> instr)
{
    Stack<Env*> env(lexicalEnv(instr->frameIndex));
    assert(env);

    if (!env->maybeDelOwnAttr(instr->ident))
        raiseNameError(instr->ident);
}

void
Interpreter::executeInstr_GetGlobal(Traced<GlobalNameInstr*> instr)
{
    Stack<Object*> global(instr->global());
    Name ident = instr->ident;
    int slot = global->findOwnAttr(ident);
    if (slot != Layout::NotFound) {
        pushStack(global->getSlot(slot));

        if (instr->canAddStub()) {
            auto stub =
                InstrFactory<Instr_GetGlobalSlot>::get(instr, global, ident);
            replaceAllStubs(instr, stub);
        }
        return;
    }

    int builtinsSlot = global->findOwnAttr(Name::__builtins__);
    if (builtinsSlot != Layout::NotFound) {
        Stack<Object*> builtins(global->getSlot(builtinsSlot).toObject());
        slot = builtins->findOwnAttr(ident);
        if (slot != Layout::NotFound) {
            pushStack(builtins->getSlot(slot));

            if (instr->canAddStub()) {
                auto stub = InstrFactory<Instr_GetBuiltinsSlot>::get(
                    instr, global, builtins, ident);
                replaceAllStubs(instr, stub);
            }
            return;
        }
    }

    raiseNameError(ident);
}

void
Interpreter::executeInstr_GetGlobalSlot(Traced<GlobalSlotInstr*> instr)
{
    int slot = instr->globalSlot();
    if (slot == Layout::NotFound) {
        dispatchInstr(instr->next());
        return;
    }

    pushStack(instr->global()->getSlot(slot));
}

void
Interpreter::executeInstr_GetBuiltinsSlot(Traced<BuiltinsSlotInstr*> instr)
{
    int slot = instr->globalSlot();
    if (slot == Layout::NotFound) {
        dispatchInstr(instr->next());
        return;
    }

    Stack<Object*> builtins(instr->global()->getSlot(slot).toObject());
    slot = instr->builtinsSlot(builtins);
    if (slot == Layout::NotFound) {
        dispatchInstr(instr->next());
        return;
    }

    pushStack(builtins->getSlot(slot));
}

void
Interpreter::executeInstr_SetGlobal(Traced<GlobalNameInstr*> instr)
{
    Stack<Value> value(peekStack());
    Stack<Object*> global(instr->global());
    instr->global()->setAttr(instr->ident, value);

    if (!instr->canAddStub())
        return;

    auto stub = InstrFactory<Instr_SetGlobalSlot>::get(
        instr, global, instr->ident);
    replaceAllStubs(instr, stub);
}

void
Interpreter::executeInstr_SetGlobalSlot(Traced<GlobalSlotInstr*> instr)
{
    int slot = instr->globalSlot();
    if (slot == Layout::NotFound) {
        dispatchInstr(instr->next());
        return;
    }

    Stack<Value> value(peekStack());
    instr->global()->setSlot(slot, value);
}

void
Interpreter::executeInstr_DelGlobal(Traced<GlobalNameInstr*> instr)
{
    if (!instr->global()->maybeDelOwnAttr(instr->ident))
        raiseNameError(instr->ident);
}

void
Interpreter::executeInstr_GetAttr(Traced<IdentInstr*> instr)
{
    Stack<Value> value(popStack());
    Stack<Value> result;
    bool ok = getAttr(value, instr->ident, result);
    pushStack(result);
    if (!ok)
        raiseException();
}

void
Interpreter::executeInstr_SetAttr(Traced<IdentInstr*> instr)
{
    // todo: this logic should move into setAttr() so python setattr picks it
    // up.
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
            Stack<Value> v(obj);
            raiseAttrError(v, instr->ident);
            return;
        }
    }

    Stack<Value> result;
    if (!setAttr(obj, instr->ident, value, result))
        raiseException(result);
}

void
Interpreter::executeInstr_DelAttr(Traced<IdentInstr*> instr)
{
    // todo: should not be able to delete attributes of builtin types?
    Stack<Object*> obj(popStack().toObject());
    Stack<Value> result;
    if (!delAttr(obj, instr->ident, result))
        raiseException(result);
}

void
Interpreter::executeInstr_Call(Traced<CountInstr*> instr)
{
    Stack<Value> target(peekStack(instr->count));
    startCall(target, instr->count, 1);
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

void
Interpreter::executeInstr_GetMethod(Traced<IdentInstr*> instr)
{
    // Attempt to get the method.
    Stack<Value> value(popStack());
    Stack<Value> result;
    bool isCallableDescriptor;
    if (!getMethodAttr(value, instr->ident, result, isCallableDescriptor)) {
        raiseAttrError(value, instr->ident);
        return;
    }

    pushStack(result, Boolean::get(isCallableDescriptor), value);

    // Check stub count before attempting to optimise.
    if (!instr->canAddStub())
        return;

    if (value.type()->isFinal()) {
        // Builtin classes cannot be changed, so we can cache the lookup.
        Stack<Class*> cls(value.type());
        auto stub = InstrFactory<Instr_GetMethodBuiltin>::get(currentInstr(),
                                                              cls, result);
        insertStubInstr(instr, stub);
    }
}

void
Interpreter::executeInstr_GetMethodBuiltin(Traced<BuiltinMethodInstr*> instr)
{
    Value value = peekStack();
    if (value.type() != instr->class_) {
        dispatchInstr(instr->next());
        return;
    }

    popStack();
    pushStack(instr->result_, Boolean::True, value);
}

void
Interpreter::executeInstr_CallMethod(Traced<CountInstr*> instr)
{
    bool extraArg = peekStack(instr->count + 1).as<Boolean>()->value();
    Stack<Value> target(peekStack(instr->count + 2));
    unsigned argCount = instr->count + (extraArg ? 1 : 0);
    return startCall(target, argCount, extraArg ? 2 : 3);
}

void
Interpreter::executeInstr_CreateEnv(Traced<Instr*> instr)
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

void
Interpreter::executeInstr_InitStackLocals(Traced<Instr*> instr)
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

void
Interpreter::executeInstr_In(Traced<Instr*> instr)
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

    // todo: invert the condition
    if (isCallableDescriptor)
        pushStack(container);
    pushStack(value);
    startCall(contains, isCallableDescriptor ? 2 : 1);
}

void
Interpreter::executeInstr_Is(Traced<Instr*> instr)
{
    Value b = popStack();
    Value a = popStack();
    bool result = a.toObject() == b.toObject();
    pushStack(Boolean::get(result));
}

void
Interpreter::executeInstr_Not(Traced<Instr*> instr)
{
    // the following values are interpreted as false: False, None, numeric zero
    // of all types, and empty strings and containers (including strings,
    // tuples, lists, dictionaries, sets and frozensets).

    Object* obj = popStack().toObject();
    pushStack(Boolean::get(!obj->isTrue()));
}

void
Interpreter::executeInstr_BranchAlways(Traced<BranchInstr*> instr)
{
    assert(instr->offset());
    branch(instr->offset());
}

void
Interpreter::executeInstr_BranchIfTrue(Traced<BranchInstr*> instr)
{
    assert(instr->offset());
    Object *x = popStack().toObject();
    if (x->isTrue())
        branch(instr->offset());
}

void
Interpreter::executeInstr_BranchIfFalse(Traced<BranchInstr*> instr)
{
    assert(instr->offset());
    Object *x = popStack().toObject();
    if (!x->isTrue())
        branch(instr->offset());
}

void
Interpreter::executeInstr_Or(Traced<BranchInstr*> instr)
{
    // The expression |x or y| first evaluates x; if x is true, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    assert(instr->offset());
    Object *x = peekStack().toObject();
    if (x->isTrue()) {
        branch(instr->offset());
        return;
    }

    popStack();
}

void
Interpreter::executeInstr_And(Traced<BranchInstr*> instr)
{
    // The expression |x and y| first evaluates x; if x is false, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    assert(instr->offset());
    Object *x = peekStack().toObject();
    if (!x->isTrue()) {
        branch(instr->offset());
        return;
    }

    popStack();
}

void
Interpreter::executeInstr_Lambda(Traced<LambdaInstr*> instr)
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

void
Interpreter::executeInstr_Dup(Traced<IndexInstr*> instr)
{
    pushStack(peekStack(instr->index));
}

void
Interpreter::executeInstr_Pop(Traced<Instr*> instr)
{
    popStack();
}

void
Interpreter::executeInstr_Swap(Traced<Instr*> instr)
{
    swapStack();
}

void
Interpreter::executeInstr_Tuple(Traced<CountInstr*> instr)
{
    Tuple* tuple = Tuple::get(stackSlice(instr->count));
    popStack(instr->count);
    pushStack(tuple);
}

void
Interpreter::executeInstr_List(Traced<CountInstr*> instr)
{
    List* list = gc.create<List>(stackSlice(instr->count));
    popStack(instr->count);
    pushStack(list);
}

void
Interpreter::executeInstr_Dict(Traced<CountInstr*> instr)
{
    Dict* dict = gc.create<Dict>(stackSlice(instr->count * 2));
    popStack(instr->count * 2);
    pushStack(dict);
}

void
Interpreter::executeInstr_Slice(Traced<Instr*> instr)
{
    Slice* slice = gc.create<Slice>(stackSlice(3));
    popStack(3);
    pushStack(slice);
}

void
Interpreter::executeInstr_AssertionFailed(Traced<Instr*> instr)
{
    Object* obj = popStack().toObject();
    assert(obj->is<String>() || obj == None);
    string message = obj != None ? obj->as<String>()->value() : "";
    pushStack(gc.create<AssertionError>(message));
    raiseException();
}

void
Interpreter::executeInstr_MakeClassFromFrame(Traced<IdentInstr*> instr)
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

bool Interpreter::getIterator(MutableTraced<Value> resultOut)
{
    Stack<Value> target(popStack());
    Stack<Value> method;
    bool isCallableDescriptor;

    // Call __iter__ method to get iterator if it's present.
    if (getSpecialMethodAttr(target, Name::__iter__, method,
                             isCallableDescriptor))
    {
        if (isCallableDescriptor)
            pushStack(target);
        return call(method, isCallableDescriptor ? 1 : 0, resultOut);
    }

    // Otherwise create a SequenceIterator wrapping the target iterable.
    if (!getMethodAttr(target, Name::__getitem__, method, isCallableDescriptor))
    {
        resultOut = gc.create<TypeError>("Object not iterable");
        return false;
    }

    pushStack(target);
    return call(SequenceIterator, 1, resultOut);
}

void
Interpreter::executeDestructureFallback(unsigned expected)
{
    Stack<Value> result;
    if (!getIterator(result))
        return raiseException(result);

    Stack<Value> iterator(result);
    Stack<Value> type(iterator.type());
    Stack<Value> nextMethod;
    bool isCallableDescriptor;
    if (!getMethodAttr(type, Name::__next__, nextMethod, isCallableDescriptor)) {
        return raiseTypeError(string("Argument is not iterable: ") +
                              type.as<Class>()->name());
    }

    for (size_t count = 0; count < expected; count++) {
        if (isCallableDescriptor)
            pushStack(iterator);
        if (!call(nextMethod, isCallableDescriptor ? 1 : 0, result)) {
            if (result.is<StopIteration>())
                return raiseValueError("too few values to unpack");
            return raiseException(result);
        }
        pushStack(result);
    }

    if (isCallableDescriptor)
        pushStack(iterator);
    if (call(nextMethod, isCallableDescriptor ? 1 : 0, result))
        return raiseValueError("too many values to unpack");
    if (!result.is<StopIteration>())
        return raiseException(result);
}

void
Interpreter::executeInstr_Destructure(Traced<CountInstr*> instr)
{
    unsigned count = instr->count;

    // todo: not sure how much point there is adding a stub here rather than
    // just switching on iterable type.
    if (instr->canAddStub()) {
        Stack<Value> iterable(peekStack());
        if (iterable.is<Tuple>()) {
            Stack<Instr*> stub(InstrFactory<Instr_DestructureTuple>::get(
                                   currentInstr(), count));
            insertStubInstr(instr, stub);
            dispatchInstr(stub);
            return;
        } else if (iterable.is<List>()) {
            Stack<Instr*> stub(InstrFactory<Instr_DestructureList>::get(
                                   currentInstr(), count));
            insertStubInstr(instr, stub);
            dispatchInstr(stub);
            return;
        }
    }

    executeDestructureFallback(instr->count);
}

void
Interpreter::executeInstr_DestructureTuple(Traced<CountStubInstr*> instr)
{
    Stack<Value> iterable(peekStack());
    if (!iterable.is<Tuple>()) {
        dispatchInstr(instr->next());
        return;
    }

    popStack();
    Tuple* tuple = iterable.as<Tuple>();

    unsigned count = tuple->len();
    if (count < instr->count)
        return raiseValueError("too few values to unpack");

    if (count > instr->count)
        return raiseValueError("too many values to unpack");

    for (unsigned i = 0; i < count; i++)
        pushStack(tuple->getitem(i));
}

void
Interpreter::executeInstr_DestructureList(Traced<CountStubInstr*> instr)
{
    Stack<Value> iterable(peekStack());
    if (!iterable.is<List>()) {
        dispatchInstr(instr->next());
        return;
    }

    popStack();
    List* list = iterable.as<List>();

    unsigned count = list->len();
    if (count < instr->count)
        return raiseValueError("too few values to unpack");

    if (count > instr->count)
        return raiseValueError("too many values to unpack");

    for (unsigned i = 0; i < count; i++)
        pushStack(list->getitem(i));
}

void
Interpreter::executeInstr_Raise(Traced<Instr*> instr)
{
    // todo: exceptions must be old-style classes or derived from BaseException
    raiseException();
}

void
Interpreter::executeInstr_GetIterator(Traced<Instr*> instr)
{
    Stack<Value> result;
    if (!getIterator(result))
        return raiseException(result);

    pushStack(result);
}

void
Interpreter::executeInstr_IteratorNext(Traced<Instr*> instr)
{
    // The stack is already set up with next method and target on top
    Stack<Value> target(peekStack(2));
    Stack<Value> result;
    pushStack(peekStack());
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

static bool ShouldInlineIntBinaryOp(BinaryOp op) {
    // Must match for_each_int_binary_op_to_inline macro in instr.h
    return true;
}

static bool ShouldInlineFloatBinaryOp(BinaryOp op) {
    // Must match for_each_float_binary_op_to_inline macro in instr.h
    return op <= BinaryTrueDiv;
}

void
Interpreter::executeInstr_BinaryOp(Traced<BinaryOpInstr*> instr)
{
    BinaryOp op = instr->op;
    Stack<Value> right(peekStack(0));
    Stack<Value> left(peekStack(1));

    // Find the method to call and execute it.
    Stack<Value> method;
    if (!executeBinaryOp(op, method))
        return;

    // Check stub count before attempting to optimise.
    if (!instr->canAddStub())
        return;

    Stack<StubInstr*> stub;

    if (ShouldInlineIntBinaryOp(op) && left.isInt32() && right.isInt32()) {
        // If both arguments are 32 bit tagged integers, inline the operation.
        auto code = InstrCode(Instr_BinaryOpInt_Add + op);
        stub = gc.create<BinaryOpStubInstr>(code, currentInstr());
    } else if (ShouldInlineIntBinaryOp(op) && left.isInt() && right.isInt()) {
        // If both arguments are integers, inline the operation.
        auto code = InstrCode(Instr_BinaryOpInteger_Add + op);
        stub = gc.create<BinaryOpStubInstr>(code, currentInstr());
    } else if (ShouldInlineFloatBinaryOp(op) &&
               left.isDouble() && right.isDouble())
    {
        // If both arguments are doubles, inline the operation.
        auto code = InstrCode(Instr_BinaryOpFloat_Add + op);
        stub = gc.create<BinaryOpStubInstr>(code, currentInstr());
    } else if (left.type()->isFinal() && right.type()->isFinal()) {
    // If both arguments are instances of builtin classes, cache the method.
        Stack<Class*> lc(left.type());
        Stack<Class*> rc(right.type());
        auto code = Instr_BinaryOpBuiltin;
        stub = gc.create<BuiltinBinaryOpInstr>(code, currentInstr(),
                                               lc, rc, method);
    }

    if (stub)
        insertStubInstr(instr, stub);
}

template <BinaryOp Op>
inline void
Interpreter::executeBinaryOpInt(Traced<BinaryOpStubInstr*> instr)
{
    assert(ShouldInlineIntBinaryOp(Op));

    if (!peekStack(0).isInt32() || !peekStack(1).isInt32()) {
        dispatchInstr(instr->next());
        return;
    }

    int32_t b = popStack().asInt32();
    int32_t a = popStack().asInt32();
    pushStack(Integer::binaryOp<Op>(a, b));
}

#define define_execute_binary_op_int(name)                                    \
    void Interpreter::executeInstr_BinaryOpInt_##name(                        \
        Traced<BinaryOpStubInstr*> instr)                                     \
    {                                                                         \
        executeBinaryOpInt<Binary##name>(instr);                              \
    }
for_each_int_binary_op_to_inline(define_execute_binary_op_int)
#undef define_execute_binary_op_int

template <BinaryOp Op>
inline void
Interpreter::executeBinaryOpInteger(Traced<BinaryOpStubInstr*> instr)
{
    assert(ShouldInlineIntBinaryOp(Op));

    if (!peekStack(0).isInt() || !peekStack(1).isInt()) {
        dispatchInstr(instr->next());
        return;
    }

    int64_t b = popStack().toInt();
    int64_t a = popStack().toInt();
    pushStack(Integer::binaryOp<Op>(a, b));
}

#define define_execute_binary_op_integer(name)                                \
    void Interpreter::executeInstr_BinaryOpInteger_##name(                    \
        Traced<BinaryOpStubInstr*> instr)                                     \
    {                                                                         \
        executeBinaryOpInteger<Binary##name>(instr);                          \
    }
for_each_int_binary_op_to_inline(define_execute_binary_op_integer)
#undef define_execute_binary_op_integer

template <BinaryOp Op>
inline void
Interpreter::executeBinaryOpFloat(Traced<BinaryOpStubInstr*> instr)
{
    assert(ShouldInlineFloatBinaryOp(Op));

    if (!peekStack(0).isDouble() || !peekStack(1).isDouble()) {
        dispatchInstr(instr->next());
        return;
    }

    double b = popStack().asDouble();
    double a = popStack().asDouble();
    pushStack(Float::binaryOp<Op>(a, b));
}

#define define_execute_binary_op_float(name)                                   \
    void Interpreter::executeInstr_BinaryOpFloat_##name(                       \
        Traced<BinaryOpStubInstr*> instr)                                      \
    {                                                                          \
        executeBinaryOpFloat<Binary##name>(instr);                             \
    }
for_each_float_binary_op_to_inline(define_execute_binary_op_float)
#undef define_execute_binary_op_float

void
Interpreter::executeInstr_BinaryOpBuiltin(Traced<BuiltinBinaryOpInstr*> instr)
{
    Value right = peekStack(0);
    Value left = peekStack(1);

    if (left.type() != instr->left() || right.type() != instr->right()) {
        dispatchInstr(instr->next());
        return;
    }

    Stack<Value> method(instr->method());
    startCall(method, 2);
}

InstrCode InlineIntCompareOpInstr(CompareOp op) {
    return InstrCode(Instr_CompareOpInt_LT + op);
}

InstrCode InlineFloatCompareOpInstr(CompareOp op) {
    return InstrCode(Instr_CompareOpFloat_LT + op);
}

bool
Interpreter::executeCompareOp(CompareOp op, MutableTraced<Value> method)
{
    Stack<Value> right(popStack());
    Stack<Value> left(popStack());

    // "There are no swapped-argument versions of these methods (to be used when
    // the left argument does not support the operation but the right argument
    // does); rather, __lt__() and __gt__() are each other's reflection,
    // __le__() and __ge__() are each other's reflection, and __eq__() and
    // __ne__() are their own reflection."

    const Name* names = Name::compareMethod;
    const Name* rnames = Name::compareMethodReflected;
    if (maybeCallBinaryOp(left, names[op], left, right, method))
        return true;

    if (!rnames[op].isNull() &&
        maybeCallBinaryOp(right, rnames[op], right, left, method))
    {
        return true;
    }

    if (op == CompareNE &&
        maybeCallBinaryOp(left, names[CompareEQ], left, right, method))
    {
        if (!isHandlingException()) {
            Stack<Value> result(popStack());
            pushStack(Boolean::get(!result.as<Boolean>()->value()));
        }
        return true;
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
        return false;
    }

    pushStack(result);
    return false;
}

void
Interpreter::executeInstr_CompareOp(Traced<CompareOpInstr*> instr)
{
    CompareOp op = instr->op;
    Stack<Value> right(peekStack(0));
    Stack<Value> left(peekStack(1));

    // Find the method to call and execute it.
    Stack<Value> method;
    if (!executeCompareOp(op, method))
        return;

    // Check stub count before attempting to optimise.
    if (!instr->canAddStub())
        return;

    Stack<CompareOpStubInstr*> stub;

    if (left.isInt32() && right.isInt32()) {
        // If both arguments are integers, inline the operation.
        auto code = InstrCode(Instr_CompareOpInt_LT + op);
        stub = gc.create<CompareOpStubInstr>(code, currentInstr());
    } else if (left.isDouble() && right.isDouble()) {
        // If both arguments are doubles, inline the operation.
        auto code = InstrCode(Instr_CompareOpFloat_LT + op);
        stub = gc.create<CompareOpStubInstr>(code, currentInstr());
    }

    if (stub)
        insertStubInstr(instr, stub);
}

template <CompareOp Op>
inline void
Interpreter::executeCompareOpInt(Traced<CompareOpStubInstr*> instr)
{
    if (!peekStack(0).isInt32() || !peekStack(1).isInt32()) {
        dispatchInstr(instr->next());
        return;
    }

    int32_t b = popStack().asInt32();
    int32_t a = popStack().asInt32();
    pushStack(Integer::compareOp<Op>(a, b));
}

#define define_execute_compare_op_int(name, token, method, rmethod)            \
void Interpreter::executeInstr_CompareOpInt_##name(                            \
    Traced<CompareOpStubInstr*> instr)                                         \
{                                                                              \
    executeCompareOpInt<Compare##name>(instr);                                 \
}
for_each_compare_op(define_execute_compare_op_int)
#undef define_execute_compare_op_int

template <CompareOp Op>
inline void
Interpreter::executeCompareOpFloat(Traced<CompareOpStubInstr*> instr)
{
    if (!peekStack(0).isDouble() || !peekStack(1).isDouble()) {
        dispatchInstr(instr->next());
        return;
    }

    double b = popStack().asDouble();
    double a = popStack().asDouble();
    pushStack(Float::compareOp<Op>(a, b));
}

#define define_execute_compare_op_float(name, token, method, rmethod)          \
void Interpreter::executeInstr_CompareOpFloat_##name(                          \
    Traced<CompareOpStubInstr*> instr)                                         \
{                                                                              \
    executeCompareOpFloat<Compare##name>(instr);                               \
}
for_each_compare_op(define_execute_compare_op_float)
#undef define_execute_compare_op_float

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

static bool ShouldInlineIntAugAssignOp(BinaryOp op) {
    // Must match for_each_int_aug_assign_op_to_inline macro in instr.h
    return op <= BinaryTrueDiv;
}

static bool ShouldInlineFloatAugAssignOp(BinaryOp op) {
    // Must match for_each_float_aug_assign_op_to_inline macro in instr.h
    return op <= BinaryTrueDiv;
}

void
Interpreter::executeInstr_AugAssignUpdate(Traced<BinaryOpInstr*> instr)
{
    const BinaryOp op = instr->op;
    Stack<Value> right(peekStack(0));
    Stack<Value> left(peekStack(1));

    // Find the method to call and execute it.
    Stack<Value> method;
    bool isCallableDescriptor;
    if (!executeAugAssignUpdate(op, method, isCallableDescriptor))
        return;

    // Check stub count before attempting to optimise.
    if (!instr->canAddStub())
        return;

    Stack<StubInstr*> stub;

    if (ShouldInlineIntAugAssignOp(op) && left.isInt32() && right.isInt32()) {
        // If both arguments are integers, inline the operation.
        auto code = InstrCode(Instr_AugAssignUpdateInt_Add + op);
        stub = gc.create<BinaryOpStubInstr>(code, currentInstr());
    } else if (ShouldInlineFloatAugAssignOp(op)
        && left.isDouble() && right.isDouble())
    {
        // If both arguments are doubles, inline the operation.
        auto code = InstrCode(Instr_AugAssignUpdateFloat_Add + op);
        stub = gc.create<BinaryOpStubInstr>(code, currentInstr());
    } else if (left.type()->isFinal() && right.type()->isFinal()) {
        // If both arguments are instances of builtin classes, cache the method.
        assert(isCallableDescriptor);
        Stack<Class*> lc(left.type());
        Stack<Class*> rc(right.type());
        auto code = Instr_AugAssignUpdateBuiltin;
        stub = gc.create<BuiltinBinaryOpInstr>(code, currentInstr(),
                                               lc, rc, method);
    }

    if (stub)
        insertStubInstr(instr, stub);
}

template <BinaryOp Op>
inline void
Interpreter::executeAugAssignUpdateInt(Traced<BinaryOpStubInstr*> instr)
{
    assert(ShouldInlineIntBinaryOp(Op));

    if (!peekStack(0).isInt32() || !peekStack(1).isInt32()) {
        dispatchInstr(instr->next());
        return;
    }

    int32_t b = popStack().asInt32();
    int32_t a = popStack().asInt32();
    pushStack(Integer::binaryOp<Op>(a, b));
}

#define define_execute_aug_assign_op_int(name)                                \
    void Interpreter::executeInstr_AugAssignUpdateInt_##name(                 \
        Traced<BinaryOpStubInstr*> instr)                                     \
    {                                                                         \
        executeAugAssignUpdateInt<Binary##name>(instr);                       \
    }
for_each_int_aug_assign_op_to_inline(define_execute_aug_assign_op_int)
#undef define_execute_aug_assign_op_int

template <BinaryOp Op>
inline void
Interpreter::executeAugAssignUpdateFloat(Traced<BinaryOpStubInstr*> instr)
{
    assert(ShouldInlineFloatBinaryOp(Op));

    if (!peekStack(0).isDouble() || !peekStack(1).isDouble()) {
        dispatchInstr(instr->next());
        return;
    }

    double b = popStack().asDouble();
    double a = popStack().asDouble();
    pushStack(Float::binaryOp<Op>(a, b));
}

#define define_execute_aug_assign_op_float(name)                              \
    void Interpreter::executeInstr_AugAssignUpdateFloat_##name(               \
        Traced<BinaryOpStubInstr*> instr)                                     \
    {                                                                         \
        executeAugAssignUpdateFloat<Binary##name>(instr);                     \
    }
for_each_float_aug_assign_op_to_inline(define_execute_aug_assign_op_float)
#undef define_execute_aug_assign_op_float

void
Interpreter::executeInstr_AugAssignUpdateBuiltin(Traced<BuiltinBinaryOpInstr*> instr)
{
    Value right = peekStack(0);
    Value left = peekStack(1);

    if (left.type() != instr->left() || right.type() != instr->right()) {
        dispatchInstr(instr->next());
        return;
    }

    Stack<Value> method(instr->method());
    startCall(method, 2);
}

void
Interpreter::executeInstr_StartGenerator(Traced<Instr*> instr)
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

void
Interpreter::executeInstr_ResumeGenerator(Traced<Instr*> instr)
{
    // todo: get slot 0
    Stack<GeneratorIter*> gen(
        env()->getAttr("self").asObject()->as<GeneratorIter>());
    gen->resume(*this);
}

void
Interpreter::executeInstr_LeaveGenerator(Traced<Instr*> instr)
{
    Stack<GeneratorIter*> gen(getGeneratorIter());
    gen->leave(*this);
}

void
Interpreter::executeInstr_SuspendGenerator(Traced<Instr*> instr)
{
    Stack<Value> value(popStack());
    Stack<GeneratorIter*> gen(getGeneratorIter());
    gen->suspend(*this, value);
}

void
Interpreter::executeInstr_EnterCatchRegion(Traced<BranchInstr*> instr)
{
    pushExceptionHandler(ExceptionHandler::CatchHandler, instr->offset());
}

void
Interpreter::executeInstr_LeaveCatchRegion(Traced<Instr*> instr)
{
    popExceptionHandler(ExceptionHandler::CatchHandler);
}

void
Interpreter::executeInstr_MatchCurrentException(Traced<Instr*> instr)
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

void
Interpreter::executeInstr_HandleCurrentException(Traced<Instr*> instr)
{
    finishHandlingException();
}

void
Interpreter::executeInstr_EnterFinallyRegion(Traced<BranchInstr*> instr)
{
    pushExceptionHandler(ExceptionHandler::FinallyHandler, instr->offset());
}

void
Interpreter::executeInstr_LeaveFinallyRegion(Traced<Instr*> instr)
{
    popExceptionHandler(ExceptionHandler::FinallyHandler);
}

void
Interpreter::executeInstr_FinishExceptionHandler(Traced<Instr*> instr)
{
    return maybeContinueHandlingException();
}

void
Interpreter::executeInstr_LoopControlJump(Traced<LoopControlJumpInstr*> instr)
{
    loopControlJump(instr->finallyCount(), instr->target());
}

void
Interpreter::executeInstr_ListAppend(Traced<Instr*> instr)
{
    Stack<Value> value(popStack());
    Stack<Value> list(popStack());
    Stack<Value> result;
    list.as<List>()->append(value, result);
    pushStack(result);
}

void
Interpreter::executeInstr_AssertStackDepth(Traced<CountInstr*> instr)
{
#ifdef DEBUG
    Frame* frame = getFrame();
    unsigned depth = stackPos() - frame->stackPos();
    if (depth != instr->count) {
        cerr << "Excpected stack depth " << dec << instr->count;
        cerr << " but got " << depth << " in: " << endl;
        cerr << *getFrame()->block() << endl;
        assert(depth == instr->count);
    }
#endif
}

bool Interpreter::run(MutableTraced<Value> resultOut)
{
#define define_dispatch_table_entry(it, cls)                                  \
        &&instr_##it,

    static const void* dispatchTable[] = {
        for_each_instr(define_dispatch_table_entry)
    };

#undef define_dispatch_table_entry

    static_assert(sizeof(dispatchTable) / sizeof(void*) == InstrCodeCount,
        "Dispatch table is not correct size");

    InstrThunk* thunk;

#define dispatch()                                                            \
    assert(getFrame()->block()->contains(instrp));                            \
    thunk = instrp++;                                                         \
    logInstr(thunk->data);                                                    \
    assert(thunk->code < InstrCodeCount);                                     \
    goto *dispatchTable[thunk->code]

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
        if (!instrp)  // todo: quit/abort instr
            goto exit;
        dispatch();
    }

#define define_handle_instr(it, cls)                                          \
    instr_##it: {                                                             \
        cls** ip = reinterpret_cast<cls**>(&thunk->data.get());               \
        executeInstr_##it(Traced<cls*>::fromTracedLocation(ip));              \
        assert(instrp);                                                       \
        dispatch();                                                           \
    }

    for_each_outofline_instr(define_handle_instr)
    for_each_stub_instr(define_handle_instr)

#undef define_handle_instr

  exit:
    assert(!instrp);
    resultOut = popStack();
    return true;
}

void Interpreter::dispatchInstr(Traced<Instr*> instr)
{
#define define_dispatch_table_entry(it, cls)                                  \
        &&instr_##it,

    static const void* dispatchTable[] = {
        for_each_instr(define_dispatch_table_entry)
    };

#undef define_dispatch_table_entry

    static_assert(sizeof(dispatchTable) / sizeof(void*) == InstrCodeCount,
        "Dispatch table is not correct size");

    logInstr(instr);
    assert(instr->code() < InstrCodeCount);
    goto *dispatchTable[instr->code()];

  instr_Abort:
  instr_Return:
    assert(false);

#define define_handle_instr(it, cls)                                          \
    instr_##it: {                                                             \
        Instr** instrp = const_cast<Instr**>(&instr.get());                   \
        cls** ip = reinterpret_cast<cls**>(instrp);                           \
        executeInstr_##it(Traced<cls*>::fromTracedLocation(ip));              \
        return;                                                               \
    }

    for_each_outofline_instr(define_handle_instr)
    for_each_stub_instr(define_handle_instr)

#undef define_handle_instr
}
