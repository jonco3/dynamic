#ifndef __INSTR_H__
#define __INSTR_H__

#include "bool.h"
#include "callable.h"
#include "dict.h"
#include "exception.h"
#include "frame.h"
#include "gc.h"
#include "integer.h"
#include "interp.h"
#include "list.h"
#include "name.h"
#include "object.h"
#include "singletons.h"
#include "string.h"
#include "list.h"

#include "value-inl.h"

#include <iostream>
#include <ostream>

using namespace std;

#define for_each_instr(instr)                                                \
    instr(Const)                                                             \
    instr(GetLocal)                                                          \
    instr(SetLocal)                                                          \
    instr(GetLexical)                                                        \
    instr(SetLexical)                                                        \
    instr(GetGlobal)                                                         \
    instr(SetGlobal)                                                         \
    instr(GetAttr)                                                           \
    instr(SetAttr)                                                           \
    instr(GetMethod)                                                         \
    instr(GetMethodInt)                                                      \
    instr(GetMethodFallback)                                                 \
    instr(Call)                                                              \
    instr(Return)                                                            \
    instr(In)                                                                \
    instr(Is)                                                                \
    instr(Not)                                                               \
    instr(BranchAlways)                                                      \
    instr(BranchIfTrue)                                                      \
    instr(BranchIfFalse)                                                     \
    instr(Or)                                                                \
    instr(And)                                                               \
    instr(Lambda)                                                            \
    instr(Dup)                                                               \
    instr(Pop)                                                               \
    instr(Swap)                                                              \
    instr(Tuple)                                                             \
    instr(List)                                                              \
    instr(Dict)                                                              \
    instr(MakeClassFromFrame)                                                \
    instr(Destructure)                                                       \
    instr(Raise)                                                             \
    instr(IteratorNext)

enum InstrType
{
#define instr_enum(name) Instr_##name,
    for_each_instr(instr_enum)
#undef instr_enum
    InstrTypeCount
};

struct Branch;

struct Instr : public Cell
{
    template <typename T> bool is() const { return type() == T::Type; }

    template <typename T> T* as() {
        assert(is<T>());
        return static_cast<T*>(this);
    }

    virtual ~Instr() {}
    virtual InstrType type() const = 0;
    virtual string name() const = 0;
    virtual bool execute(Interpreter& interp) = 0;

    virtual bool isBranch() const { return false; };
    inline Branch* asBranch();

    virtual void traceChildren(Tracer& t) {}
    virtual size_t size() const { return sizeof(*this); }
    virtual void print(ostream& s) const {
        s << name();
    }
};

#define instr_type(it)                                                       \
    virtual InstrType type() const { return Type; }                          \
    static const InstrType Type = it

#define instr_name(nameStr)                                                  \
    virtual string name() const { return nameStr; }

struct InstrConst : public Instr
{
    InstrConst(Traced<Value> v) : value(v) {}
    InstrConst(Traced<Object*> o) : value(o) {}

    instr_type(Instr_Const);
    instr_name("Const");

    virtual void print(ostream& s) const {
        s << name() << " " << value;
    }

    virtual bool execute(Interpreter& interp) {
        interp.pushStack(value);
        return true;
    }

    Value getValue() { return value; }

    virtual void traceChildren(Tracer& t) override {
        gc::trace(t, &value);
    }

  private:
    Value value;
};

struct IdentInstrBase : public Instr
{
    IdentInstrBase(Name ident) : ident(ident) {}

    virtual void print(ostream& s) const {
        s << name() << " " << ident;
    }

    bool raiseAttrError(Value value, Interpreter& interp) {
        const Class* cls = value.toObject()->getClass();
        string message = "'" + cls->name() + "' object has no attribute '" + ident + "'";
        interp.pushStack(gc::create<Exception>("AttributeError", message));
        return false;
    }

  protected:
    const Name ident;
};

struct InstrGetLocal : public IdentInstrBase
{
    InstrGetLocal(Name ident) : IdentInstrBase(ident) {}

    instr_type(Instr_GetLocal);
    instr_name("GetLocal");

    virtual bool execute(Interpreter& interp) {
        Frame* frame = interp.getFrame();
        interp.pushStack(frame->getAttr(ident));
        return true;
    }
};

struct InstrSetLocal : public IdentInstrBase
{
    InstrSetLocal(Name ident) : IdentInstrBase(ident) {}

    instr_type(Instr_SetLocal);
    instr_name("SetLocal");

    virtual bool execute(Interpreter& interp) {
        Root<Value> value(interp.peekStack(0));
        interp.getFrame()->setAttr(ident, value);
        return true;
    }
};

struct InstrGetLexical : public Instr
{
    InstrGetLexical(int frame, Name ident) : frameIndex(frame), ident(ident) {}

    instr_type(Instr_GetLexical);
    instr_name("GetLexical");

    virtual void print(ostream& s) const {
        s << name() << " " << frameIndex << " " << ident;
    }

    virtual bool execute(Interpreter& interp) {
        Frame* frame = interp.getFrame(frameIndex);
        interp.pushStack(frame->getAttr(ident));
        return true;
    }

  private:
    int frameIndex;
    Name ident;
};

struct InstrSetLexical : public Instr
{
    InstrSetLexical(unsigned frame, Name ident) : frameIndex(frame), ident(ident) {}

    instr_type(Instr_SetLexical);
    instr_name("SetLexical");

    virtual void print(ostream& s) const {
        s << name() << " " << frameIndex << " " << ident;
    }

    virtual bool execute(Interpreter& interp) {
        Root<Value> value(interp.peekStack(0));
        interp.getFrame(frameIndex)->setAttr(ident, value);
        return true;
    }

  private:
    unsigned frameIndex;
    Name ident;
};

struct InstrGetGlobal : public IdentInstrBase
{
    InstrGetGlobal(Object* global, Name ident)
      : IdentInstrBase(ident), global(global)
    {
        assert(global);
    }

    instr_type(Instr_GetGlobal);
    instr_name("GetGlobal");

    virtual bool execute(Interpreter& interp) {
        interp.pushStack(global->getAttr(ident));
        return true;
    }

    virtual void traceChildren(Tracer& t) override {
        gc::trace(t, &global);
    }

  private:
    Object* global;
};

struct InstrSetGlobal : public IdentInstrBase
{
    InstrSetGlobal(Object* global, Name ident)
      : IdentInstrBase(ident), global(global)
    {
        assert(global);
    }

    instr_type(Instr_SetGlobal);
    instr_name("SetGlobal");

    virtual bool execute(Interpreter& interp) {
        Root<Value> value(interp.peekStack(0));
        global->setAttr(ident, value);
        return true;
    }

    virtual void traceChildren(Tracer& t) override {
        gc::trace(t, &global);
    }

  private:
    Object* global;
};

struct InstrGetAttr : public IdentInstrBase
{
    InstrGetAttr(Name ident) : IdentInstrBase(ident) {}

    instr_type(Instr_GetAttr);
    instr_name("GetAttr");

    virtual bool execute(Interpreter& interp) {
        Value value = interp.popStack();
        Value result;
        if (!value.maybeGetAttr(ident, result))
            return raiseAttrError(value, interp);
        assert(result != UninitializedSlot);
        interp.pushStack(result);
        return true;
    }
};

struct InstrSetAttr : public IdentInstrBase
{
    InstrSetAttr(Name ident) : IdentInstrBase(ident) {}

    instr_type(Instr_SetAttr);
    instr_name("SetAttr");

    virtual bool execute(Interpreter& interp) {
        Root<Value> value(interp.peekStack(1));
        interp.popStack().toObject()->setAttr(ident, value);
        return true;
    }
};

struct InstrGetMethod : public IdentInstrBase
{
    InstrGetMethod(Name name) : IdentInstrBase(name) {}

    instr_type(Instr_GetMethod);
    instr_name("GetMethod");

    virtual bool execute(Interpreter& interp) override;
};

struct InstrGetMethodInt : public IdentInstrBase
{
    InstrGetMethodInt(Name name, Traced<Value> result)
      : IdentInstrBase(name), result_(result) {}

    instr_type(Instr_GetMethodInt);
    instr_name("GetMethodInt");

    virtual bool execute(Interpreter& interp);

    virtual void traceChildren(Tracer& t) override {
        gc::trace(t, &result_);
    }

  private:
    Value result_;
};

struct InstrGetMethodFallback : public IdentInstrBase
{
    InstrGetMethodFallback(Name name) : IdentInstrBase(name) {}

    instr_type(Instr_GetMethodFallback);
    instr_name("GetMethodFallback");

    virtual bool execute(Interpreter& interp);
};

inline bool InstrGetMethod::execute(Interpreter& interp)
{
    Value value(interp.popStack());
    Value result;
    if (!value.maybeGetAttr(ident, result))
        return raiseAttrError(value, interp);
    assert(result != UninitializedSlot);
    interp.pushStack(result);
    interp.pushStack(value);

    if (value.isInt32()) {
        Root<Value> rootedResult(result);
        interp.replaceInstr(gc::create<InstrGetMethodInt>(ident, rootedResult), this);
    } else {
        interp.replaceInstr(gc::create<InstrGetMethodFallback>(ident), this);
    }

    return true;
}

inline bool InstrGetMethodInt::execute(Interpreter& interp)
{
    Value value = interp.popStack();
    if (!value.isInt32()) {
        interp.replaceInstr(gc::create<InstrGetMethodFallback>(ident), this);
        Value result;
        if (!value.maybeGetAttr(ident, result))
            return raiseAttrError(value, interp);
        assert(result != UninitializedSlot);
        interp.pushStack(result);
    } else {
        interp.pushStack(result_);
    }

    interp.pushStack(value);
    return true;
}

inline bool InstrGetMethodFallback::execute(Interpreter& interp)
{
    Value value = interp.popStack();
    Value result;
    if (!value.maybeGetAttr(ident, result))
        return raiseAttrError(value, interp);
    assert(result != UninitializedSlot);
    interp.pushStack(result);
    interp.pushStack(value);
    return true;
}

struct InstrCall : public Instr
{
    InstrCall(unsigned count) : count(count) {}

    instr_type(Instr_Call);
    instr_name("Call");

    virtual void print(ostream& s) const {
        s << name() << " " << count;
    }

    virtual bool execute(Interpreter& interp) {
        Root<Value> target(interp.peekStack(count));
        TracedVector<Value> args = interp.stackSlice(count - 1, count);
        interp.popStack(count + 1);
        return interp.startCall(target, args);
    }

  private:
    unsigned count;
};

struct InstrReturn : public Instr
{
    instr_type(Instr_Return);
    instr_name("Return");

    virtual bool execute(Interpreter& interp) {
        Value value = interp.popStack();
        interp.popFrame();
        interp.pushStack(value);
        return true;
    }
};

struct InstrIn : public Instr
{
    instr_type(Instr_In);
    instr_name("In");

    // todo: implement this
    // https://docs.python.org/2/reference/expressions.html#membership-test-details

    virtual bool execute(Interpreter& interp) {
        Root<Object*> container(interp.popStack().toObject());
        Root<Value> value(interp.popStack());
        Value contains; // todo: root
        if (!container->maybeGetAttr("__contains__", contains)) {
            interp.pushStack(gc::create<Exception>("TypeError", "Argument is not iterable"));
            return false;
        }

        RootVector<Value> args;
        args.push_back(container);
        args.push_back(value);
        Root<Value> rootedContains(contains);
        return interp.startCall(rootedContains, args);
    }
};

struct InstrIs : public Instr
{
    instr_type(Instr_Is);
    instr_name("Is");

    virtual bool execute(Interpreter& interp) {
        Value b = interp.popStack();
        Value a = interp.popStack();
        bool result = a.toObject() == b.toObject();
        interp.pushStack(Boolean::get(result));
        return true;
    }
};

struct InstrNot : public Instr
{
    instr_type(Instr_Not);
    instr_name("Not");

    // the following values are interpreted as false: False, None, numeric zero
    // of all types, and empty strings and containers (including strings,
    // tuples, lists, dictionaries, sets and frozensets).

    virtual bool execute(Interpreter& interp) {
        Object* obj = interp.popStack().toObject();
        interp.pushStack(Boolean::get(!obj->isTrue()));
        return true;
    }
};

struct Branch : public Instr
{
    Branch() : offset_(0) {}
    virtual bool isBranch() const { return true; };

    void setOffset(int offset) {
        assert(!offset_);
        offset_ = offset;
    }

    virtual void print(ostream& s) const {
        s << name() << " " << offset_;
    }

  protected:
    int offset_;
};

inline Branch* Instr::asBranch()
{
    assert(isBranch());
    return static_cast<Branch*>(this);
}

struct InstrBranchAlways : public Branch
{
    InstrBranchAlways(int offset = 0) { offset_ = offset; }
    instr_type(Instr_BranchAlways);
    instr_name("BranchAlways");

    virtual bool execute(Interpreter& interp) {
        assert(offset_);
        interp.branch(offset_);
        return true;
    }
};

struct InstrBranchIfTrue : public Branch
{
    instr_type(Instr_BranchIfTrue);
    instr_name("BranchIfTrue");

    virtual bool execute(Interpreter& interp) {
        assert(offset_);
        Object *x = interp.popStack().toObject();
        if (x->isTrue())
            interp.branch(offset_);
        return true;
    }
};

struct InstrBranchIfFalse : public Branch
{
    instr_type(Instr_BranchIfFalse);
    instr_name("BranchIfFalse");

    virtual bool execute(Interpreter& interp) {
        assert(offset_);
        Object *x = interp.popStack().toObject();
        if (!x->isTrue())
            interp.branch(offset_);
        return true;
    }
};

struct InstrOr : public Branch
{
    instr_type(Instr_Or);
    instr_name("Or");

    // The expression |x or y| first evaluates x; if x is true, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    virtual bool execute(Interpreter& interp) {
        assert(offset_);
        Object *x = interp.peekStack(0).toObject();
        if (x->isTrue()) {
            interp.branch(offset_);
            return true;
        }
        interp.popStack();
        return true;
    }
};

struct InstrAnd : public Branch
{
    instr_type(Instr_And);
    instr_name("And");

    // The expression |x and y| first evaluates x; if x is false, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    virtual bool execute(Interpreter& interp) {
        assert(offset_);
        Object *x = interp.peekStack(0).toObject();
        if (!x->isTrue()) {
            interp.branch(offset_);
            return true;
        }
        interp.popStack();
        return true;
    }
};

struct InstrLambda : public Instr
{
    InstrLambda(const vector<Name>& params, Block* block)
      : params_(params), block_(block) {}

    instr_type(Instr_Lambda);
    instr_name("Lambda");

    Block* block() const { return block_; }

    virtual bool execute(Interpreter& interp) {
        Root<Block*> block(block_);
        Root<Frame*> frame(interp.getFrame(0));
        interp.pushStack(gc::create<Function>(params_, block, frame));
        return true;
    }

    virtual void traceChildren(Tracer& t) override {
        gc::trace(t, &block_);
    }

    virtual void print(ostream& s) const {
        s << name();
        for (auto i = params_.begin(); i != params_.end(); ++i)
            s << " " << *i;
        s << ": { " << block_ << " }";
    }

  private:
    vector<Name> params_;
    Block* block_;
};

struct InstrDup : public Instr
{
    InstrDup(unsigned index) : index_(index) {}
    instr_type(Instr_Dup);
    instr_name("Dup");

    virtual bool execute(Interpreter& interp) {
        interp.pushStack(interp.peekStack(index_));
        return true;
    }

    virtual void print(ostream& s) const {
        s << name() << " " << index_;
    }

  private:
    unsigned index_;
};

struct InstrPop : public Instr
{
    instr_type(Instr_Pop);
    instr_name("Pop");

    virtual bool execute(Interpreter& interp) {
        interp.popStack();
        return true;
    }
};

struct InstrSwap : public Instr
{
    instr_type(Instr_Swap);
    instr_name("Swap");

    virtual bool execute(Interpreter& interp) {
        interp.swapStack();
        return true;
    }
};

struct InstrTuple : public Instr
{
    instr_type(Instr_Tuple);
    instr_name("Tuple");

  InstrTuple(unsigned size) : size(size) {}

    virtual bool execute(Interpreter& interp) {
        Tuple* tuple = Tuple::get(interp.stackSlice(size - 1, size));
        interp.popStack(size);
        interp.pushStack(tuple);
        return true;
    }

  private:
    unsigned size;
};

struct InstrList : public Instr
{
    instr_type(Instr_List);
    instr_name("List");

  InstrList(unsigned size) : size(size) {}

    virtual bool execute(Interpreter& interp) {
        List* list = gc::create<List>(interp.stackSlice(size - 1, size));
        interp.popStack(size);
        interp.pushStack(list);
        return true;
    }

  private:
    unsigned size;
};

struct InstrDict : public Instr
{
    instr_type(Instr_Dict);
    instr_name("Dict");

    InstrDict(unsigned size) : size(size) {}

    virtual bool execute(Interpreter& interp) {
        Dict* dict = gc::create<Dict>(interp.stackSlice(size * 2 - 1, size * 2));
        interp.popStack(size * 2);
        interp.pushStack(dict);
        return true;
    }

  private:
    unsigned size;
};

struct InstrAssertionFailed : public Instr
{
    instr_type(Instr_Dict);
    instr_name("AssertionFailed");

    virtual bool execute(Interpreter& interp) {
        Object* obj = interp.popStack().toObject();
        assert(obj->is<String>() || obj == None);
        string message = obj != None ? obj->as<String>()->value() : "";
        interp.pushStack(gc::create<Exception>("AssertionError", message));
        return false;
    }
};

struct InstrMakeClassFromFrame : public IdentInstrBase
{
    InstrMakeClassFromFrame(Name id) : IdentInstrBase(id) {}

    instr_type(Instr_MakeClassFromFrame);
    instr_name("MakeClassFromFrame");

    virtual bool execute(Interpreter& interp) {
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
};

struct InstrDestructure : public Instr
{
    InstrDestructure(unsigned count) : count_(count) {}

    instr_type(Instr_Destructure);
    instr_name("Destructure");

    virtual bool execute(Interpreter& interp) {
        Root<Value> seq(interp.popStack());
        Value attr; // todo: root
        if (!seq.get().maybeGetAttr("__len__", attr)) {
            interp.pushStack(gc::create<Exception>("TypeError", "Argument is not iterable"));
            return false;
        }
        Root<Value> lenFunc(attr);

        if (!seq.get().maybeGetAttr("__getitem__", attr)) {
            interp.pushStack(gc::create<Exception>("TypeError", "Argument is not iterable"));
            return false;
        }
        Root<Value> getitemFunc(attr);

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

  private:
    unsigned count_;
};

struct InstrRaise : public Instr
{
    instr_type(Instr_Raise);
    instr_name("Raise");

    virtual bool execute(Interpreter& interp) {
        // todo: exceptions must be old-style classes or derived from BaseException
        return false;
    }
};

struct InstrIteratorNext : public Instr
{
    instr_type(Instr_IteratorNext);
    instr_name("IteratorNext");

    virtual bool execute(Interpreter& interp) {
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
};

#endif
