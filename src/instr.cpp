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
#include "set.h"
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
        return static_cast<cls*>(instr)->next().data;

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
        s << " " << dec << size_t(stubCount_);
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

void IndexInstr::print(ostream& s) const
{
    Instr::print(s);
    s << " " << index;
}

void ValueInstr::print(ostream& s) const
{
    Instr::print(s);
    if (!value_.isObject())
        s << " " << value_;
}

void CallWithFullArgsInstr::print(ostream& s) const
{
    Instr::print(s);
    s << " ";
    if (maybePosCount == SIZE_MAX)
        s << "?";
    else
        s << dec << maybePosCount;
    s << " ";
    keywords->print(s);
}

void ValueInstr::traceChildren(Tracer& t)
{
    Instr::traceChildren(t);
    gc.trace(t, &value_);
}

void CallWithFullArgsInstr::traceChildren(Tracer& t)
{
    Instr::traceChildren(t);
    gc.trace(t, &keywords);
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
    Instr::traceChildren(t);
    gc.trace(t, &info_);
}

void StubInstr::print(ostream& s) const
{
    s << *next_.data << " <- ";
    Instr::print(s);
}

void StubInstr::traceChildren(Tracer& t)
{
    Instr::traceChildren(t);
    gc.trace(t, &next_.data);
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
    Stack<Object*> global(getFrame()->block()->global());
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

    int builtinsSlot = global->findOwnAttr(Names::__builtins__);
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
Interpreter::executeInstr_SetGlobal(Traced<GlobalNameInstr*> instr)
{
    Stack<Value> value(peekStack());
    Stack<Object*> global(getFrame()->block()->global());
    global->setAttr(instr->ident, value);

    if (!instr->canAddStub())
        return;

    auto stub = InstrFactory<Instr_SetGlobalSlot>::get(
        instr, global, instr->ident);
    replaceAllStubs(instr, stub);
}

void
Interpreter::executeInstr_DelGlobal(Traced<GlobalNameInstr*> instr)
{
    Stack<Object*> global(getFrame()->block()->global());
    if (!global->maybeDelOwnAttr(instr->ident))
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
                raise<TypeError>(
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
    startCall(target, instr->count, Layout::Empty, 1);
}

size_t CallWithFullArgsInstr::slotCount(Frame* frame, size_t stackPos) const
{
    stackPos -= frame->stackPos();
    size_t count = stackPos - argsPos;
    assert(!posCountKnown() || count == slotCount());
    return count;
}

Layout* Interpreter::unpackKeywordMapping(Traced<Layout*> initialKeywords)
{
    Stack<Layout*> keywords(initialKeywords);
    Stack<Dict*> mapping(popStack().as<Dict>()); // todo: support non-dict mappings
    for (auto i : mapping->entries()) {
        Stack<Value> key(i.first);
        if (!key.is<String>())
            raise<TypeError>("Mapping contains non-string key");
        Name name = internString(key.as<String>()->value());
        keywords = keywords->addName(name);
        stack.push_back(i.second);
    }
    return keywords;
}

void
Interpreter::executeInstr_CallWithFullArgs(Traced<CallWithFullArgsInstr*> instr)
{
    Stack<Layout*> keywords(instr->keywords);
    if (instr->mappingArg)
        keywords = unpackKeywordMapping(keywords);
    size_t slotCount = instr->slotCount(getFrame(), stack.size());
    Stack<Value> target(peekStack(slotCount));
    startCall(target, slotCount, keywords, 1);
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
    StackMethodAttr result;
    if (!getMethodAttr(value, instr->ident, result)) {
        raiseAttrError(value, instr->ident);
        return;
    }

    pushStack(result.method);
    pushStack(result.isCallable ? value : Value(UninitializedSlot));

    // Check stub count before attempting to optimise.
    if (!instr->canAddStub())
        return;

    if (value.type()->isFinal()) {
        // Builtin classes cannot be changed, so we can cache the lookup.
        Stack<Class*> cls(value.type());
        auto stub = InstrFactory<Instr_GetMethodBuiltin>::get(
            currentInstr(), cls, result.method);
        insertStubInstr(instr, stub);
    }
}

void
Interpreter::executeInstr_CallMethod(Traced<CountInstr*> instr)
{
    bool extraArg = peekStack(instr->count) != Value(UninitializedSlot);
    Stack<Value> target(peekStack(instr->count + 1));
    unsigned argCount = instr->count + (extraArg ? 1 : 0);
    return startCall(target, argCount, Layout::Empty, extraArg ? 1 : 2);
}

void
Interpreter::executeInstr_CallMethodWithFullArgs(
    Traced<CallWithFullArgsInstr*> instr)
{
    Stack<Layout*> keywords(instr->keywords);
    if (instr->mappingArg)
        keywords = unpackKeywordMapping(keywords);
    size_t slotCount = instr->slotCount(getFrame(), stack.size());
    bool extraArg = peekStack(slotCount) != Value(UninitializedSlot);
    Stack<Value> target(peekStack(slotCount + 1));
    unsigned posCount = slotCount + (extraArg ? 1 : 0);
    startCall(target, posCount, keywords, extraArg ? 1 : 2);
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
    // todo: could pop and reuse stack
}

void
Interpreter::executeInstr_SetEnv(Traced<ValueInstr*> instr)
{
    // Set the current frame's environment, used for eval/exec with supplied
    // locals.

    assert(!getFrame()->env());
    popStack();  // Ignore parent env.
    Stack<Env*> env(instr->value().as<Env>());
    assert(getFrame()->block()->argCount() == 0);
    setFrameEnv(env);
}

void
Interpreter::executeInstr_InitStackLocals(Traced<CountInstr*> instr)
{
    AutoAssertNoGC nogc;
    assert(!getFrame()->env());

    Object* obj = popStack().asObject();
    Env* parentEnv = obj ? obj->as<Env>() : nullptr;
    setFrameEnv(parentEnv);

    fillStack(instr->count, UninitializedSlot);
}

void
Interpreter::executeInstr_In(Traced<Instr*> instr)
{
    // todo: implement this
    // https://docs.python.org/3/reference/expressions.html#membership-test-details

    Stack<Value> container(popStack());
    Stack<Value> value(popStack());

    StackMethodAttr method;
    if (getSpecialMethodAttr(container, Names::__contains__, method)) {
        if (method.isCallable)
            pushStack(container);
        pushStack(value);
        startCall(method.method, 1 + method.extraArgs());
        return;
    }

    // todo: hasAttr
    if (getSpecialMethodAttr(container, Names::__iter__, method)) {
        pushStack(container);
        pushStack(value);
        startCall(InUsingIteration, 2);
        return;
    }

    // todo: hasAttr
    if (getSpecialMethodAttr(container, Names::__getitem__, method)) {
        pushStack(container);
        pushStack(value);
        startCall(InUsingSubscript, 2);
        return;
    }

    raise<TypeError>("Argument is not iterable");
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

    Stack<Value> value(popStack());
    pushStack(Boolean::get(!Value::IsTrue(value)));
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
    Stack<Value> x(popStack());
    if (Value::IsTrue(x))
        branch(instr->offset());
}

void
Interpreter::executeInstr_BranchIfFalse(Traced<BranchInstr*> instr)
{
    assert(instr->offset());
    Stack<Value> x(popStack());
    if (!Value::IsTrue(x))
        branch(instr->offset());
}

void
Interpreter::executeInstr_Or(Traced<BranchInstr*> instr)
{
    // The expression |x or y| first evaluates x; if x is true, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    assert(instr->offset());
    Stack<Value> x(peekStack());
    if (Value::IsTrue(x)) {
        branch(instr->offset());
        return;
    }
}

void
Interpreter::executeInstr_And(Traced<BranchInstr*> instr)
{
    // The expression |x and y| first evaluates x; if x is false, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    assert(instr->offset());
    Stack<Value> x(peekStack());
    if (!Value::IsTrue(x)) {
        branch(instr->offset());
        return;
    }
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
Interpreter::executeInstr_Set(Traced<CountInstr*> instr)
{
    Set* set = gc.create<Set>(stackSlice(instr->count));
    popStack(instr->count);
    pushStack(set);
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
    raise<AssertionError>(message);
}

void
Interpreter::executeInstr_MakeClassFromFrame(Traced<IdentInstr*> instr)
{
    Stack<Env*> env(this->env());

    // todo: this layout transplanting should be used to a utility method
    vector<Name> names;
    for (Layout* l = env->layout(); l != Env::InitialLayout; l = l->parent()) {
        if (l->name() != Names::__bases__)
            names.push_back(l->name());
    }
    Stack<Layout*> layout(Class::InitialLayout);
    for (auto i = names.begin(); i != names.end(); i++)
        layout = layout->addName(*i);

    Stack<Value> bases;
    if (!env->maybeGetAttr(Names::__bases__, bases))
        return raise<AttributeError>("Missing __bases__");

    if (!bases.toObject()->is<Tuple>())
        return raise<TypeError>("__bases__ is not a tuple");

    Stack<Tuple*> tuple(bases.as<Tuple>());
    Stack<Class*> base(Object::ObjectClass);
    if (tuple->len() > 1) {
        return raise<NotImplementedError>("Multiple inheritance not NYI");
    } else if (tuple->len() == 1) {
        Stack<Value> value(tuple->getitem(0));
        if (!value.toObject()->is<Class>())
            return raise<TypeError>("__bases__[0] is not a class");
        base = value.as<Class>();
    }

    Class* cls = gc.create<Class>(instr->ident->value(), base, layout);
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
    StackMethodAttr method;

    // Call __iter__ method to get iterator if it's present.
    if (getSpecialMethodAttr(target, Names::__iter__, method))
    {
        if (method.isCallable)
            pushStack(target);
        return syncCall(method.method, method.extraArgs(), resultOut);
    }

    // Otherwise create a SequenceIterator wrapping the target iterable.
    // todo: add hasMethodAttr, or just hasAttr?
    if (!getMethodAttr(target, Names::__getitem__, method))
        return Raise<TypeError>("Object not iterable", resultOut);

    return call(SequenceIterator, target, resultOut);
}

void
Interpreter::executeDestructureGeneric(unsigned expected)
{
    Stack<Value> result;
    if (!getIterator(result))
        return raiseException(result);

    Stack<Value> iterator(result);
    Stack<Value> type(iterator.type());
    StackMethodAttr nextMethod;
    if (!getMethodAttr(type, Names::__next__, nextMethod)) {
        return raise<TypeError>(string("Argument is not iterable: ") +
                                type.as<Class>()->name());
    }

    for (size_t count = 0; count < expected; count++) {
        if (nextMethod.isCallable)
            pushStack(iterator);
        if (!syncCall(nextMethod.method, nextMethod.extraArgs(), result)) {
            if (result.is<StopIteration>())
                return raise<ValueError>("too few values to unpack");
            return raiseException(result);
        }
        pushStack(result);
    }

    if (nextMethod.isCallable)
        pushStack(iterator);
    if (syncCall(nextMethod.method, nextMethod.extraArgs(), result))
        return raise<ValueError>("too many values to unpack");
    if (!result.is<StopIteration>())
        return raiseException(result);
}

template <typename T>
void
Interpreter::executeDestructureBuiltin(unsigned expected, T* seq)
{
    popStack();

    unsigned count = seq->len();
    if (count < expected)
        return raise<ValueError>("too few values to unpack");

    if (count > expected)
        return raise<ValueError>("too many values to unpack");

    for (unsigned i = 0; i < count; i++)
        pushStack(seq->getitem(i));
}

void
Interpreter::executeInstr_Destructure(Traced<CountInstr*> instr)
{
    unsigned count = instr->count;
    Stack<Value> iterable(peekStack());

    if (iterable.is<Tuple>())
        executeDestructureBuiltin<Tuple>(count, iterable.as<Tuple>());
    else if (iterable.is<List>())
        executeDestructureBuiltin<List>(count, iterable.as<List>());
    else
        executeDestructureGeneric(count);
}

void
Interpreter::executeUnpackGeneric()
{
    Stack<Value> result;
    if (!getIterator(result))
        return raiseException(result);

    Stack<Value> iterator(result);
    Stack<Value> type(iterator.type());
    StackMethodAttr nextMethod;
    if (!getMethodAttr(type, Names::__next__, nextMethod)) {
        return raise<TypeError>(string("Argument is not iterable: ") +
                                type.as<Class>()->name());
    }

    for (;;) {
        if (nextMethod.isCallable)
            pushStack(iterator);
        if (!syncCall(nextMethod.method, nextMethod.extraArgs(), result)) {
            if (result.is<StopIteration>())
                break;
            return raiseException(result);
        }
        logStackPush(result);
        stack.push_back(result);
    }
}

template <typename T>
void
Interpreter::executeUnpackBuiltin(T* seq)
{
    popStack();

    for (unsigned i = 0; i < seq->len(); i++) {
        Value value = seq->getitem(i);
        logStackPush(value);
        stack.push_back(value);
    }
}

void
Interpreter::executeInstr_UnpackArgs(Traced<Instr*> instr)
{
    Stack<Value> iterable(peekStack());

    if (iterable.is<Tuple>())
        executeUnpackBuiltin<Tuple>(iterable.as<Tuple>());
    else if (iterable.is<List>())
        executeUnpackBuiltin<List>(iterable.as<List>());
    else
        executeUnpackGeneric();
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
    // The stack is already set up with next method and object instance on top
    Stack<Value> target(peekStack(1));
    Stack<Value> result;
    bool ok = call(target, peekStack(), result);
    pushStack(result);
    bool finished = !ok && result.isObject() && result.is<StopIteration>();
    if (!finished && !ok)
        return raiseException();

    pushStack(Boolean::get(!finished));
}

bool Interpreter::maybeCallBinaryOp(Name name,
                                    Traced<Value> left, Traced<Value> right,
                                    StackMethodAttr& methodOut, bool& okOut)
{
    assert(!okOut);
    Stack<Value> type(left.type());
    if (!getMethodAttr(type, name, methodOut))
        return false;

    Stack<Value> result;
    if (methodOut.isCallable)
        pushStack(left);
    pushStack(right);
    if (!syncCall(methodOut.method, 1 + methodOut.extraArgs(), result)) {
        pushStack(result);
        raiseException();
        return true;
    }

    if (result == Value(NotImplemented))
        return false;

    pushStack(result);
    okOut = true;
    return true;
}

bool
Interpreter::executeBinaryOp(BinaryOp op,
                             StackMethodAttr& methodOut, bool& reversedOut)
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
    const Name* names = Names::binMethod;
    const Name* rnames = Names::binMethodReflected;
    bool ok = false;
    if (rtype != ltype && rtype->isDerivedFrom(ltype))
    {
        if (maybeCallBinaryOp(rnames[op], right, left, methodOut, ok)) {
            reversedOut = true;
            return ok;
        }
        if (maybeCallBinaryOp(names[op], left, right, methodOut, ok)) {
            reversedOut = false;
            return ok;
        }
    } else {
        if (maybeCallBinaryOp(names[op], left, right, methodOut, ok)) {
            reversedOut = false;
            return ok;
        }
        if (maybeCallBinaryOp(rnames[op], right, left, methodOut, ok)) {
            reversedOut = true;
            return ok;
        }
    }

    raise<TypeError>("unsupported operand type(s) for binary operation");
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

static InstrCode BinaryOpBuiltinInstr(bool reversed)
{
    return reversed ? Instr_BinaryOpBuiltinReversed : Instr_BinaryOpBuiltin;
}

void
Interpreter::executeInstr_BinaryOp(Traced<BinaryOpInstr*> instr)
{
    BinaryOp op = instr->op;
    Stack<Value> right(peekStack(0));
    Stack<Value> left(peekStack(1));

    // Find the method to call and execute it.
    StackMethodAttr method;
    bool reversed;
    if (!executeBinaryOp(op, method, reversed))
        return;

    // Check stub count before attempting to optimise.
    if (!instr->canAddStub())
        return;

    Stack<StubInstr*> stub;

    if (ShouldInlineIntBinaryOp(op) && left.isInt32() && right.isInt32()) {
        // If both arguments are 32 bit tagged integers, inline the operation.
        auto code = InstrCode(Instr_BinaryOpInt_Add + op);
        stub = gc.create<BinaryOpStubInstr>(code, currentInstr());
    } else if (ShouldInlineFloatBinaryOp(op) &&
               left.isDouble() && right.isDouble())
    {
        // If both arguments are doubles, inline the operation.
        auto code = InstrCode(Instr_BinaryOpFloat_Add + op);
        stub = gc.create<BinaryOpStubInstr>(code, currentInstr());
    } else if (left.type()->isFinal() && right.type()->isFinal()) {
        // If both arguments are instances of builtin classes, cache the method.
        assert(method.isCallable);
        Stack<Class*> lc(left.type());
        Stack<Class*> rc(right.type());
        auto code = BinaryOpBuiltinInstr(reversed);
        stub = gc.create<BuiltinBinaryOpInstr>(code, currentInstr(),
                                               lc, rc, method.method);
    }

    if (stub)
        insertStubInstr(instr, stub);
}

InstrCode InlineIntCompareOpInstr(CompareOp op) {
    return InstrCode(Instr_CompareOpInt_LT + op);
}

InstrCode InlineFloatCompareOpInstr(CompareOp op) {
    return InstrCode(Instr_CompareOpFloat_LT + op);
}

bool
Interpreter::executeCompareOp(CompareOp op,
                              StackMethodAttr& methodOut, bool& reversedOut)
{
    Stack<Value> right(popStack());
    Stack<Value> left(popStack());

    // "There are no swapped-argument versions of these methods (to be used when
    // the left argument does not support the operation but the right argument
    // does); rather, __lt__() and __gt__() are each other's reflection,
    // __le__() and __ge__() are each other's reflection, and __eq__() and
    // __ne__() are their own reflection."

    const Name* names = Names::compareMethod;
    const Name* rnames = Names::compareMethodReflected;
    bool ok = false;
    if (maybeCallBinaryOp(names[op], left, right, methodOut, ok)) {
        reversedOut = false;
        return ok;
    }

    if (maybeCallBinaryOp(rnames[op], right, left, methodOut, ok)) {
        reversedOut = true;
        return ok;
    }

    if (op == CompareNE &&
        maybeCallBinaryOp(names[CompareEQ], left, right, methodOut, ok))
    {
        reversedOut = false;
        if (ok)
            refStack() = Boolean::get(!peekStack().as<Boolean>()->value());
        return ok;
    }

    Stack<Value> result;
    if (op == CompareEQ) {
        result = Boolean::get(left.toObject() == right.toObject());
    } else if (op == CompareNE) {
        result = Boolean::get(left.toObject() != right.toObject());
    } else {
        raise<TypeError>("unsupported operand type(s) for compare operation");
        return false;
    }

    reversedOut = false;
    pushStack(result);
    // todo: could return a different status to indicate no method to call?
    return false;
}

void
Interpreter::executeInstr_CompareOp(Traced<CompareOpInstr*> instr)
{
    CompareOp op = instr->op;
    Stack<Value> right(peekStack(0));
    Stack<Value> left(peekStack(1));

    // Find the method to call and execute it.
    StackMethodAttr method;
    bool reversed;
    if (!executeCompareOp(op, method, reversed))
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

    // todo: optimise for builtin classes

    if (stub)
        insertStubInstr(instr, stub);
}

bool
Interpreter::executeAugAssignUpdate(BinaryOp op, StackMethodAttr& methodOut,
                                    bool& reversedOut)
{
    Stack<Value> update(popStack());
    Stack<Value> value(popStack());

    const Name* anames = Names::augAssignMethod;
    const Name* bnames = Names::binMethod;
    const Name* rnames = Names::binMethodReflected;
    bool ok = false;
    if (maybeCallBinaryOp(anames[op], value, update, methodOut, ok)) {
        reversedOut = false;
        return ok;
    }
    if (maybeCallBinaryOp(bnames[op], value, update, methodOut, ok)) {
        reversedOut = false;
        return ok;
    }
    if (maybeCallBinaryOp(rnames[op], update, value, methodOut, ok)) {
        reversedOut = true;
        return ok;
    }

    raise<TypeError>("unsupported operand type(s) for augmented assignment");
    return false;
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
    StackMethodAttr method;
    bool reversed;
    if (!executeAugAssignUpdate(op, method, reversed))
        return;

    // Check stub count before attempting to optimise.
    if (!instr->canAddStub())
        return;

    Stack<StubInstr*> stub;

    if (ShouldInlineIntAugAssignOp(op) && left.isInt32() && right.isInt32()) {
        // If both arguments are integers, inline the operation.
        auto code = InstrCode(Instr_BinaryOpInt_Add + op);
        stub = gc.create<BinaryOpStubInstr>(code, currentInstr());
    } else if (ShouldInlineFloatAugAssignOp(op)
        && left.isDouble() && right.isDouble())
    {
        // If both arguments are doubles, inline the operation.
        auto code = InstrCode(Instr_BinaryOpFloat_Add + op);
        stub = gc.create<BinaryOpStubInstr>(code, currentInstr());
    } else if (left.type()->isFinal() && right.type()->isFinal()) {
        // If both arguments are instances of builtin classes, cache the method.
        assert(method.isCallable);
        Stack<Class*> lc(left.type());
        Stack<Class*> rc(right.type());
        auto code = BinaryOpBuiltinInstr(reversed);
        stub = gc.create<BuiltinBinaryOpInstr>(code, currentInstr(),
                                               lc, rc, method.method);
    }

    if (stub)
        insertStubInstr(instr, stub);
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
        env()->getAttr(Names::self).asObject()->as<GeneratorIter>());
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
    pushStack(Boolean::get(match));
}

void
Interpreter::executeInstr_HandleCurrentException(Traced<Instr*> instr)
{
    Stack<Exception*> exception(currentException());
    pushStack(exception);
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
    list.as<List>()->append(value);
    pushStack(None);
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

#define fetchInstr()                                                          \
    assert(instrp);                                                           \
    assert(getFrame()->block()->contains(instrp));                            \
    thunk = instrp++

#define execInstr()                                                       \
    logInstr(thunk->data);                                                    \
    assert(thunk->code < InstrCodeCount);                                     \
    goto *dispatchTable[thunk->code]

#define dispatch()                                                            \
    fetchInstr();                                                             \
    execInstr()

    dispatch();

#ifdef DEBUG
#define maybeCountInstr(instr)                                                \
    instrCounts[instr]++;
#else
#define maybeCountInstr(instr)
#endif

#define start_handle_instr(it, cls)                                           \
    instr_##it: {                                                             \
        maybeCountInstr(Instr_##it);                                          \
        Heap<cls*>& instr = reinterpret_cast<Heap<cls*>&>(thunk->data)

#define execute_outofline_instr(it)                                           \
        executeInstr_##it(instr)

#define end_handle_instr()                                                    \
        dispatch();                                                           \
    }

    start_handle_instr(Abort, Instr);
        (void)instr;
        resultOut = popStack();
        assert(resultOut.isInstanceOf(Exception::ObjectClass));
        popFrame();
        return false;
    end_handle_instr();


    start_handle_instr(Return, Instr);
        (void)instr;
        Value value = popStack();
        returnFromFrame(value);
        if (!instrp)  // todo: quit/abort instr
            goto exit;
    end_handle_instr();

  exit:
    assert(!instrp);
    resultOut = popStack();
    return true;

#define handle_outofline_instr(it, cls)                                       \
    start_handle_instr(it, cls);                                              \
    execute_outofline_instr(it);                                              \
    end_handle_instr()

    for_each_outofline_instr(handle_outofline_instr);

#define dispatchNextStub()                                                    \
    do {                                                                      \
        thunk = const_cast<InstrThunk*>(&instr->next());                      \
        execInstr();                                                          \
    } while (false)

    start_handle_instr(GetGlobalSlot, GlobalSlotInstr);
        Object* global = getFrame()->block()->global();
        int slot = instr->globalSlot(global);
        if (slot == Layout::NotFound)
            dispatchNextStub();

        pushStack(global->getSlot(slot));
    end_handle_instr();

    start_handle_instr(GetBuiltinsSlot, BuiltinsSlotInstr);
        Object* global = getFrame()->block()->global();
        int slot = instr->globalSlot(global);
        if (slot == Layout::NotFound)
            dispatchNextStub();

        Object* builtins = global->getSlot(slot).toObject();
        slot = instr->builtinsSlot(builtins);
        if (slot == Layout::NotFound)
            dispatchNextStub();

        pushStack(builtins->getSlot(slot));
    end_handle_instr();

    start_handle_instr(SetGlobalSlot, GlobalSlotInstr);
        Object* global = getFrame()->block()->global();
        int slot = instr->globalSlot(global);
        if (slot == Layout::NotFound)
            dispatchNextStub();

        global->setSlot(slot, peekStack());
    end_handle_instr();

    start_handle_instr(GetMethodBuiltin, BuiltinMethodInstr);
        Value value = peekStack();
        if (value.type() != instr->class_)
            dispatchNextStub();

        popStack();
        pushStack(instr->result_, value);
    end_handle_instr();

#define define_binary_op_int_stub(name)                                       \
    start_handle_instr(BinaryOpInt_##name, BinaryOpStubInstr);                \
    assert(ShouldInlineIntBinaryOp(Binary##name));                            \
        if (!peekStack(0).isInt32() || !peekStack(1).isInt32())               \
            dispatchNextStub();                                               \
                                                                              \
        int32_t b = popStack().asInt32();                                     \
        int32_t a = peekStack().asInt32();                                    \
        if (!Integer::binaryOp<Binary##name>(a, b, refStack()))               \
            raiseException();                                                 \
    end_handle_instr()

    for_each_int_binary_op_to_inline(define_binary_op_int_stub);
#undef define_binary_op_int_stub

#define define_binary_op_float_stub(name)                                     \
    start_handle_instr(BinaryOpFloat_##name, BinaryOpStubInstr);              \
    assert(ShouldInlineFloatBinaryOp(Binary##name));                          \
        if (!peekStack(0).isDouble() || !peekStack(1).isDouble())             \
            dispatchNextStub();                                               \
                                                                              \
        double b = popStack().asDouble();                                     \
        double a = popStack().asDouble();                                     \
        pushStack(Float::binaryOp<Binary##name>(a, b));                       \
    end_handle_instr()

    for_each_float_binary_op_to_inline(define_binary_op_float_stub);
#undef define_binary_op_float_stub

    start_handle_instr(BinaryOpBuiltin, BuiltinBinaryOpInstr);
        Value right = peekStack(0);
        Value left = peekStack(1);
        if (left.type() != instr->left() || right.type() != instr->right())
            dispatchNextStub();

        {
            Stack<Value> method(instr->method());
            startCall(method, 2);
        }
    end_handle_instr();

    start_handle_instr(BinaryOpBuiltinReversed, BuiltinBinaryOpInstr);
        Value right = peekStack(0);
        Value left = peekStack(1);
        if (left.type() != instr->left() || right.type() != instr->right())
            dispatchNextStub();

        {
            Stack<Value> method(instr->method());
            swapStack();
            startCall(method, 2);
        }
    end_handle_instr();

#define define_compare_op_int_stub(name, x, y, z)                             \
    start_handle_instr(CompareOpInt_##name, CompareOpStubInstr);              \
        if (!peekStack(0).isInt32() || !peekStack(1).isInt32())               \
            dispatchNextStub();                                               \
                                                                              \
        int32_t b = popStack().asInt32();                                     \
        int32_t a = popStack().asInt32();                                     \
        pushStack(Integer::compareOp<Compare##name>(a, b));                   \
    end_handle_instr()

    for_each_compare_op(define_compare_op_int_stub);
#undef define_compare_op_int_stub

#define define_compare_op_float_stub(name, x, y, z)                           \
    start_handle_instr(CompareOpFloat_##name, CompareOpStubInstr);            \
        if (!peekStack(0).isDouble() || !peekStack(1).isDouble())             \
            dispatchNextStub();                                               \
                                                                              \
        double b = popStack().asDouble();                                     \
        double a = popStack().asDouble();                                     \
        pushStack(Float::compareOp<Compare##name>(a, b));                     \
    end_handle_instr()

    for_each_compare_op(define_compare_op_float_stub);
#undef define_compare_op_float_stub

#undef fetchInstr
#undef execInstr
#undef dispatch
#undef start_handle_instr
#undef execute_outofline_instr
#undef end_handle_instr
#undef dispatchNextStub

    crash("Not reached");
}
