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
#include "specials.h"
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
    instr(AssertionFailed)                                                   \
    instr(MakeClassFromFrame)                                                \
    instr(Destructure)                                                       \
    instr(Raise)                                                             \
    instr(IteratorNext)                                                      \
    instr(BinaryOp)                                                          \
    instr(BinaryOpInt)                                                       \
    instr(BinaryOpFallback)                                                  \
    instr(AugAssignUpdate)                                                   \
    instr(ResumeGenerator)                                                   \
    instr(LeaveGenerator)                                                    \
    instr(SuspendGenerator)

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

#define define_instr_members(it, name)                                        \
    instr_type(it);                                                           \
    instr_name(name);                                                         \
    instr_execute

#define define_simple_instr(name)                                             \
    struct Instr##name : public Instr                                         \
    {                                                                         \
        define_instr_members(Instr_##name, #name);                            \
    }

struct InstrConst : public Instr
{
    define_instr_members(Instr_Const, "Const");
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

#define define_ident_instr(name)                                              \
    struct Instr##name : public IdentInstrBase                                \
    {                                                                         \
        Instr##name(Name ident) : IdentInstrBase(ident) {}                    \
        define_instr_members(Instr_##name, #name);                            \
    }

define_ident_instr(GetLocal);
define_ident_instr(SetLocal);

struct InstrGetLexical : public IdentInstrBase
{
    define_instr_members(Instr_GetLexical, "GetLexical");
    InstrGetLexical(unsigned frame, Name ident)
      : IdentInstrBase(ident), frameIndex(frame) {}

    virtual void print(ostream& s) const {
        s << name() << " " << frameIndex << " " << ident;
    }

  private:
    unsigned frameIndex;
};

struct InstrSetLexical : public IdentInstrBase
{
    define_instr_members(Instr_SetLexical, "SetLexical");
    InstrSetLexical(unsigned frame, Name ident)
      : IdentInstrBase(ident), frameIndex(frame) {}

    virtual void print(ostream& s) const {
        s << name() << " " << frameIndex << " " << ident;
    }

  private:
    unsigned frameIndex;
};

struct InstrGetGlobal : public IdentInstrBase
{
    define_instr_members(Instr_GetGlobal, "GetGlobal");

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
    define_instr_members(Instr_SetGlobal, "SetGlobal");

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

define_ident_instr(GetAttr);
define_ident_instr(SetAttr);
define_ident_instr(GetMethod);

struct InstrGetMethodInt : public IdentInstrBase
{
    define_instr_members(Instr_GetMethodInt, "GetMethodInt");
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
    define_instr_members(Instr_GetMethodFallback, "GetMethodFallback");
    InstrGetMethodFallback(Name name) : IdentInstrBase(name) {}
};

struct InstrCall : public Instr
{
    define_instr_members(Instr_Call, "Call");
    InstrCall(unsigned count) : count(count) {}

    virtual void print(ostream& s) const {
        s << name() << " " << count;
    }

  private:
    unsigned count;
};

define_simple_instr(Return);
define_simple_instr(In);
define_simple_instr(Is);
define_simple_instr(Not);

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
    define_instr_members(Instr_BranchAlways, "BranchAlways");
    InstrBranchAlways(int offset = 0) { offset_ = offset; }
};

#define define_branch_instr(name)                                             \
    struct Instr##name : public Branch                                        \
    {                                                                         \
        define_instr_members(Instr_##name, #name);                            \
    }

define_branch_instr(BranchIfTrue);
define_branch_instr(BranchIfFalse);
define_branch_instr(Or);
define_branch_instr(And);

#undef define_branch_instr

struct InstrLambda : public Instr
{
    define_instr_members(Instr_Lambda, "Lambda");

  InstrLambda(const vector<Name>& params, Block* block, bool isGenerator = false)
      : params_(params), block_(block), isGenerator_(isGenerator) {}

    Block* block() const { return block_; }

    bool isGenerator() const { return isGenerator_; }

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
    bool isGenerator_;
};

struct InstrDup : public Instr
{
    define_instr_members(Instr_Dup, "Dup");
    InstrDup(unsigned index) : index_(index) {}

    virtual void print(ostream& s) const {
        s << name() << " " << index_;
    }

  private:
    unsigned index_;
};

define_simple_instr(Pop);
define_simple_instr(Swap);

struct InstrTuple : public Instr
{
    define_instr_members(Instr_Tuple, "Tuple");
    InstrTuple(unsigned size) : size(size) {}

  private:
    unsigned size;
};

struct InstrList : public Instr
{
    define_instr_members(Instr_List, "List");
    InstrList(unsigned size) : size(size) {}

  private:
    unsigned size;
};

struct InstrDict : public Instr
{
    define_instr_members(Instr_Dict, "Dict");
    InstrDict(unsigned size) : size(size) {}

  private:
    unsigned size;
};

define_simple_instr(AssertionFailed);

define_ident_instr(MakeClassFromFrame);

struct InstrDestructure : public Instr
{
    define_instr_members(Instr_Destructure, "Destructure");
    InstrDestructure(unsigned count) : count_(count) {}

  private:
    unsigned count_;
};

define_simple_instr(Raise);
define_simple_instr(IteratorNext);

struct BinaryOpInstr : public Instr
{
    BinaryOpInstr(BinaryOp op) : op_(op) {}

    virtual void print(ostream& s) const;

    BinaryOp op() const { return op_; }

  private:
    BinaryOp op_;
};

struct InstrBinaryOp : public BinaryOpInstr
{
    define_instr_members(Instr_BinaryOp, "BinaryOp");
    InstrBinaryOp(BinaryOp op) : BinaryOpInstr(op) {}
};

struct InstrBinaryOpInt : public BinaryOpInstr
{
    define_instr_members(Instr_BinaryOpInt, "BinaryOpInt");
    InstrBinaryOpInt(BinaryOp op, Traced<Value> method)
      : BinaryOpInstr(op), method_(method) {}

    virtual void traceChildren(Tracer& t) override {
        gc::trace(t, &method_);
    }

  private:
    Value method_;
};

struct InstrBinaryOpFallback : public BinaryOpInstr
{
    define_instr_members(Instr_BinaryOpFallback, "BinaryOpFallback");
    InstrBinaryOpFallback(BinaryOp op) : BinaryOpInstr(op) {}
};

struct InstrAugAssignUpdate : public BinaryOpInstr
{
    define_instr_members(Instr_AugAssignUpdate, "AugAssignUpdate");
    InstrAugAssignUpdate(BinaryOp op) : BinaryOpInstr(op) {}
};

define_simple_instr(ResumeGenerator);
define_simple_instr(LeaveGenerator);
define_simple_instr(SuspendGenerator);

#undef instr_type
#undef instr_name
#undef instr_execute
#undef define_instr_members
#undef define_simple_instr
#undef define_branch_instr
#undef define_ident_instr

#endif
