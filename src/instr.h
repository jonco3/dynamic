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
    instr(Pop)                                                               \
    instr(Tuple)                                                             \
    instr(List)                                                              \
    instr(Dict)

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

  protected:
    bool raise(Interpreter& interp, const string& message) {
        interp.pushStack(new Exception(message));
        return false;
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

    virtual void traceChildren(Tracer& t) {
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

    virtual void traceChildren(Tracer& t) {
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

    virtual void traceChildren(Tracer& t) {
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
        Value value;
        bool ok = interp.popStack().toObject()->getAttrOrRaise(ident, value);
        interp.pushStack(value);
        return ok;
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

struct InstrGetMethod : public Instr
{
    InstrGetMethod(Name name) : methodName(name) {}

    instr_type(Instr_GetMethod);
    instr_name("GetMethod");

    virtual void print(ostream& s) const {
        s << name() << " " << methodName;
    }

    virtual bool execute(Interpreter& interp) {
        Object* obj = interp.popStack().toObject();
        Value value;
        bool ok = obj->getAttrOrRaise(methodName, value);
        interp.pushStack(value);
        if (ok)
            interp.pushStack(obj);
        return ok;
    }

  private:
    Name methodName;
};

struct InstrCall : public Instr
{
    InstrCall(unsigned args) : args(args) {}

    instr_type(Instr_Call);
    instr_name("Call");

    virtual void print(ostream& s) const {
        s << name() << " " << args;
    }

    bool raiseArgumentException(Interpreter& interp) {
        return raise(interp, "Wrong number of arguments");
    }

    virtual bool execute(Interpreter& interp) {
        Root<Object*> target(interp.peekStack(args).toObject());
        if (target->is<Native>()) {
            Root<Native*> native(target->as<Native>());
            if (native->requiredArgs() != args)
                return raiseArgumentException(interp);
            return native->call(interp);
        } else if (target->is<Function>()) {
            Root<Function*> function(target->as<Function>());
            if (function->requiredArgs() != args)
                return raiseArgumentException(interp);
            Root<Frame*> callFrame(interp.newFrame(function));
            for (int i = args - 1; i >= 0; --i) {
                Root<Value> arg(interp.popStack());
                callFrame->setAttr(function->paramName(i), arg);
            }
            interp.popStack();
            interp.pushFrame(callFrame);
            return true;
        } else {
            for (unsigned i = 0; i < args + 1; ++i)
                interp.popStack();
            return raise(interp, "TypeError: object is not callable");
        }
    }

  private:
    unsigned args;
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
        return raise(interp, "not implemented");
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
        if (x->isTrue())
            interp.branch(offset_);
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
        if (!x->isTrue())
            interp.branch(offset_);
        return true;
    }
};

struct InstrLambda : public Instr
{
    InstrLambda(const vector<Name>& params, Block* block)
      : params_(params), block_(block) {}

    instr_type(Instr_Lambda);
    instr_name("Lambda");

    virtual bool execute(Interpreter& interp) {
        Root<Block*> block(block_);
        Root<Frame*> frame(interp.getFrame(0));
        interp.pushStack(new Function(params_, block, frame));
        return true;
    }

    virtual void traceChildren(Tracer& t) {
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

struct InstrPop : public Instr
{
    instr_type(Instr_Pop);
    instr_name("Pop");

    virtual bool execute(Interpreter& interp) {
        interp.popStack();
        return true;
    }
};

struct InstrTuple : public Instr
{
    instr_type(Instr_Tuple);
    instr_name("Tuple");

  InstrTuple(unsigned size) : size(size) {}

    virtual bool execute(Interpreter& interp) {
        Tuple* tuple = Tuple::get(size, interp.stackRef(size - 1));
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
        List* list = new List(size, interp.stackRef(size - 1));
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
        Dict* dict = new Dict(size, interp.stackRef(size * 2 - 1));
        interp.popStack(size * 2);
        interp.pushStack(dict);
        return true;
    }

  private:
    unsigned size;
};

#endif
