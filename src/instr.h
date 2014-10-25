#ifndef __INSTR_H__
#define __INSTR_H__

#include "bool.h"
#include "callable.h"
#include "frame.h"
#include "gc.h"
#include "integer.h"
#include "interp.h"
#include "name.h"
#include "object.h"
#include "repr.h"
#include "singletons.h"

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
    instr(Lambda)

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
    InstrConst(Value v) : value(v) {}

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
        assert(frame->hasAttr(ident));
        Value value;
        frame->getAttr(ident, value);
        interp.pushStack(value);
        return true;
    }
};

struct InstrSetLocal : public IdentInstrBase
{
    InstrSetLocal(Name ident) : IdentInstrBase(ident) {}

    instr_type(Instr_SetLocal);
    instr_name("SetLocal");

    virtual bool execute(Interpreter& interp) {
        Root<Value> value(interp.popStack());
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
        assert(frame->hasAttr(ident));
        Value value;
        frame->getAttr(ident, value);
        interp.pushStack(value);
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
        Root<Value> value(interp.popStack());
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
      : IdentInstrBase(ident), global(global) {}

    instr_type(Instr_GetGlobal);
    instr_name("GetGlobal");

    virtual bool execute(Interpreter& interp) {
        Value value;
        if (!global->getAttr(ident, value))
            return false;
        interp.pushStack(value);
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
      : IdentInstrBase(ident), global(global) {}

    instr_type(Instr_SetGlobal);
    instr_name("SetGlobal");

    virtual bool execute(Interpreter& interp) {
        Root<Value> value(interp.popStack());
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
        if (!interp.popStack().toObject()->getAttr(ident, value))
            return false;
        interp.pushStack(value);
        return true;
    }
};

struct InstrSetAttr : public IdentInstrBase
{
    InstrSetAttr(Name ident) : IdentInstrBase(ident) {}

    instr_type(Instr_SetAttr);
    instr_name("SetAttr");

    virtual bool execute(Interpreter& interp) {
        Root<Value> value(interp.popStack());
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
        Value method;
        if (!obj->getAttr(methodName, method))
            return false;
        interp.pushStack(method);
        interp.pushStack(obj);
        return true;
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

    virtual bool execute(Interpreter& interp) {
        Object* target = interp.peekStack(args).toObject();
        if (target->is<Native>()) {
            Native* native = target->as<Native>();
            if (native->requiredArgs() < args)
                throw runtime_error("Not enough arguments");
            native->call(interp);
            return true;
        } else if (target->is<Function>()) {
            Function* function = target->as<Function>();
            if (function->requiredArgs() < args)
                throw runtime_error("Not enough arguments");
            Frame *callFrame = interp.newFrame(function);
            for (int i = args - 1; i >= 0; --i) {
                Root<Value> arg(interp.popStack());
                callFrame->setAttr(function->paramName(i), arg);
            }
            interp.popStack();
            interp.pushFrame(callFrame);
            return true;
        } else {
            throw runtime_error("Attempt to call non-callable object: " +
                                     repr(target));
            // todo: implement exceptions: return false and unwind
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
        cerr << "not implemented" << endl;
        return false;
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
        interp.pushStack(new Function(params_, block_, interp.getFrame(0)));
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

#endif
