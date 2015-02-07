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

#define instr_type(it)                                                        \
    virtual InstrType type() const { return Type; }                           \
    static const InstrType Type = it

#define instr_name(nameStr)                                                   \
    virtual string name() const { return nameStr; }

#define instr_execute                                                         \
    virtual bool execute(Interpreter& interp) override

#define define_instr(it, name)                                                \
    instr_type(it);                                                           \
    instr_name(name);                                                         \
    instr_execute

struct InstrConst : public Instr
{
    define_instr(Instr_Const, "Const");
    InstrConst(Traced<Value> v) : value(v) {}
    InstrConst(Traced<Object*> o) : value(o) {}

    virtual void print(ostream& s) const {
        s << name() << " " << value;
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

    bool raiseAttrError(Traced<Value> value, Interpreter& interp);

  protected:
    const Name ident;
};

struct InstrGetLocal : public IdentInstrBase
{
    define_instr(Instr_GetLocal, "GetLocal");
    InstrGetLocal(Name ident) : IdentInstrBase(ident) {}
};

struct InstrSetLocal : public IdentInstrBase
{
    define_instr(Instr_SetLocal, "SetLocal");
    InstrSetLocal(Name ident) : IdentInstrBase(ident) {}
};

struct InstrGetLexical : public Instr
{
    define_instr(Instr_GetLexical, "GetLexical");
    InstrGetLexical(int frame, Name ident) : frameIndex(frame), ident(ident) {}

    virtual void print(ostream& s) const {
        s << name() << " " << frameIndex << " " << ident;
    }

  private:
    int frameIndex;
    Name ident;
};

struct InstrSetLexical : public Instr
{
    define_instr(Instr_SetLexical, "SetLexical");
    InstrSetLexical(unsigned frame, Name ident) : frameIndex(frame), ident(ident) {}

    virtual void print(ostream& s) const {
        s << name() << " " << frameIndex << " " << ident;
    }

  private:
    unsigned frameIndex;
    Name ident;
};

struct InstrGetGlobal : public IdentInstrBase
{
    define_instr(Instr_GetGlobal, "GetGlobal");

    InstrGetGlobal(Object* global, Name ident)
      : IdentInstrBase(ident), global(global)
    {
        assert(global);
    }

    virtual void traceChildren(Tracer& t) override {
        gc::trace(t, &global);
    }

  private:
    Object* global;
};

struct InstrSetGlobal : public IdentInstrBase
{
    define_instr(Instr_SetGlobal, "SetGlobal");

    InstrSetGlobal(Object* global, Name ident)
      : IdentInstrBase(ident), global(global)
    {
        assert(global);
    }

    virtual void traceChildren(Tracer& t) override {
        gc::trace(t, &global);
    }

  private:
    Object* global;
};

struct InstrGetAttr : public IdentInstrBase
{
    define_instr(Instr_GetAttr, "GetAttr");
    InstrGetAttr(Name ident) : IdentInstrBase(ident) {}
};

struct InstrSetAttr : public IdentInstrBase
{
    define_instr(Instr_SetAttr, "SetAttr");
    InstrSetAttr(Name ident) : IdentInstrBase(ident) {}
};

struct InstrGetMethod : public IdentInstrBase
{
    define_instr(Instr_GetMethod, "GetMethod");
    InstrGetMethod(Name name) : IdentInstrBase(name) {}
};

struct InstrGetMethodInt : public IdentInstrBase
{
    define_instr(Instr_GetMethodInt, "GetMethodInt");
    InstrGetMethodInt(Name name, Traced<Value> result)
      : IdentInstrBase(name), result_(result) {}

    virtual void traceChildren(Tracer& t) override {
        gc::trace(t, &result_);
    }

  private:
    Value result_;
};

struct InstrGetMethodFallback : public IdentInstrBase
{
    define_instr(Instr_GetMethodFallback, "GetMethodFallback");
    InstrGetMethodFallback(Name name) : IdentInstrBase(name) {}
};

struct InstrCall : public Instr
{
    define_instr(Instr_Call, "Call");
    InstrCall(unsigned count) : count(count) {}

    virtual void print(ostream& s) const {
        s << name() << " " << count;
    }

  private:
    unsigned count;
};

struct InstrReturn : public Instr
{
    define_instr(Instr_Return, "Return");
};

struct InstrIn : public Instr
{
    define_instr(Instr_In, "In");
};

struct InstrIs : public Instr
{
    define_instr(Instr_Is, "Is");
};

struct InstrNot : public Instr
{
    define_instr(Instr_Not, "Not");
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
    define_instr(Instr_BranchAlways, "BranchAlways");
    InstrBranchAlways(int offset = 0) { offset_ = offset; }
};

struct InstrBranchIfTrue : public Branch
{
    define_instr(Instr_BranchIfTrue, "BranchIfTrue");
};

struct InstrBranchIfFalse : public Branch
{
    define_instr(Instr_BranchIfFalse, "BranchIfFalse");
};

struct InstrOr : public Branch
{
    define_instr(Instr_Or, "Or");
};

struct InstrAnd : public Branch
{
    define_instr(Instr_And, "And");
};

struct InstrLambda : public Instr
{
    define_instr(Instr_Lambda, "Lambda");

    InstrLambda(const vector<Name>& params, Block* block)
      : params_(params), block_(block) {}

    Block* block() const { return block_; }

    virtual void traceChildren(Tracer& t) override {
        gc::trace(t, &block_);
    }

    virtual void print(ostream& s) const {
        s << name();
        for (auto i = params_.begin(); i != params_.end(); ++i)
            s << " " << *i;
        s << ": { " << *block_ << " }";
    }

  private:
    vector<Name> params_;
    Block* block_;
};

struct InstrDup : public Instr
{
    define_instr(Instr_Dup, "Dup");
    InstrDup(unsigned index) : index_(index) {}

    virtual void print(ostream& s) const {
        s << name() << " " << index_;
    }

  private:
    unsigned index_;
};

struct InstrPop : public Instr
{
    define_instr(Instr_Pop, "Pop");
};

struct InstrSwap : public Instr
{
    define_instr(Instr_Swap, "Swap");
};

struct InstrTuple : public Instr
{
    define_instr(Instr_Tuple, "Tuple");
    InstrTuple(unsigned size) : size(size) {}

  private:
    unsigned size;
};

struct InstrList : public Instr
{
    define_instr(Instr_List, "List");
    InstrList(unsigned size) : size(size) {}

  private:
    unsigned size;
};

struct InstrDict : public Instr
{
    define_instr(Instr_Dict, "Dict");
    InstrDict(unsigned size) : size(size) {}

  private:
    unsigned size;
};

struct InstrAssertionFailed : public Instr
{
    define_instr(Instr_Dict, "AssertionFailed");
};

struct InstrMakeClassFromFrame : public IdentInstrBase
{
    define_instr(Instr_MakeClassFromFrame, "MakeClassFromFrame");
    InstrMakeClassFromFrame(Name id) : IdentInstrBase(id) {}
};

struct InstrDestructure : public Instr
{
    define_instr(Instr_Destructure, "Destructure");
    InstrDestructure(unsigned count) : count_(count) {}

  private:
    unsigned count_;
};

struct InstrRaise : public Instr
{
    define_instr(Instr_Raise, "Raise");
};

struct InstrIteratorNext : public Instr
{
    define_instr(Instr_IteratorNext, "IteratorNext");
};

#endif
