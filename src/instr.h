#ifndef __INSTR_H__
#define __INSTR_H__

#include "bool.h"
#include "callable.h"
#include "dict.h"
#include "exception.h"
#include "frame.h"
#include "gcdefs.h"
#include "numeric.h"
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
    instr(GetStackLocal)                                                     \
    instr(SetStackLocal)                                                     \
    instr(DelStackLocal)                                                     \
    instr(GetLexical)                                                        \
    instr(SetLexical)                                                        \
    instr(DelLexical)                                                        \
    instr(GetGlobal)                                                         \
    instr(SetGlobal)                                                         \
    instr(DelGlobal)                                                         \
    instr(GetAttr)                                                           \
    instr(SetAttr)                                                           \
    instr(DelAttr)                                                           \
    instr(GetMethod)                                                         \
    instr(GetMethodInt)                                                      \
    instr(Call)                                                              \
    instr(CallMethod)                                                        \
    instr(CreateEnv)                                                         \
    instr(InitStackLocals)                                                   \
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
    instr(Slice)                                                             \
    instr(AssertionFailed)                                                   \
    instr(MakeClassFromFrame)                                                \
    instr(Destructure)                                                       \
    instr(Raise)                                                             \
    instr(IteratorNext)                                                      \
    instr(BinaryOp)                                                          \
    instr(BinaryOpInt)                                                       \
    instr(AugAssignUpdate)                                                   \
    instr(StartGenerator)                                                    \
    instr(ResumeGenerator)                                                   \
    instr(LeaveGenerator)                                                    \
    instr(SuspendGenerator)                                                  \
    instr(EnterCatchRegion)                                                  \
    instr(LeaveCatchRegion)                                                  \
    instr(MatchCurrentException)                                             \
    instr(HandleCurrentException)                                            \
    instr(EnterFinallyRegion)                                                \
    instr(LeaveFinallyRegion)                                                \
    instr(FinishExceptionHandler)                                            \
    instr(LoopControlJump)                                                   \
    instr(AssertStackDepth)

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

    virtual bool isBranch() const { return false; };
    inline Branch* asBranch();

    virtual void print(ostream& s) const {
        s << name();
    }
};

#define instr_type(it)                                                        \
    virtual InstrType type() const { return Type; }                           \
    static const InstrType Type = Instr_##it

#define instr_name(nameStr)                                                   \
    virtual string name() const { return nameStr; }

#define instr_execute(it)                                                     \
    static bool execute(Traced<Instr##it*> self, Interpreter& interp)

#define define_instr_members(it)                                              \
    instr_type(it);                                                           \
    instr_name(#it);                                                          \
    instr_execute(it)

#define define_simple_instr(it)                                               \
    struct Instr##it : public Instr                                           \
    {                                                                         \
        define_instr_members(it);                                             \
    }

struct InstrConst : public Instr
{
    define_instr_members(Const);
    explicit InstrConst(Traced<Value> v) : value(v) {}

    virtual void print(ostream& s) const {
        s << name() << " " << value;
    }

    Value getValue() { return value; }

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &value);
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

    const Name ident;
};

#define define_ident_instr(name)                                              \
    struct Instr##name : public IdentInstrBase                                \
    {                                                                         \
        Instr##name(Name ident) : IdentInstrBase(ident) {}                    \
        define_instr_members(name);                                           \
    }

struct InstrGetStackLocal : public IdentInstrBase
{
    define_instr_members(GetStackLocal);
    InstrGetStackLocal(Name ident, unsigned slot)
      : IdentInstrBase(ident), slot_(slot)
    {}

    virtual void print(ostream& s) const {
        s << name() << " " << ident << " " << slot_;
    }

  private:
    unsigned slot_;
};

struct InstrSetStackLocal : public IdentInstrBase
{
    define_instr_members(SetStackLocal);
    InstrSetStackLocal(Name ident, unsigned slot)
      : IdentInstrBase(ident), slot_(slot)
    {}

    virtual void print(ostream& s) const {
        s << name() << " " << ident << " " << slot_;
    }

  private:
    unsigned slot_;
};

struct InstrDelStackLocal : public IdentInstrBase
{
    define_instr_members(DelStackLocal);
    InstrDelStackLocal(Name ident, unsigned slot)
      : IdentInstrBase(ident), slot_(slot)
    {}

    virtual void print(ostream& s) const {
        s << name() << " " << ident << " " << slot_;
    }

  private:
    unsigned slot_;
};

struct InstrGetLexical : public IdentInstrBase
{
    define_instr_members(GetLexical);
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
    define_instr_members(SetLexical);
    InstrSetLexical(unsigned frame, Name ident)
      : IdentInstrBase(ident), frameIndex(frame) {}

    virtual void print(ostream& s) const {
        s << name() << " " << frameIndex << " " << ident;
    }

  private:
    unsigned frameIndex;
};

struct InstrDelLexical : public IdentInstrBase
{
    define_instr_members(DelLexical);
    InstrDelLexical(unsigned frame, Name ident)
      : IdentInstrBase(ident), frameIndex(frame) {}

    virtual void print(ostream& s) const {
        s << name() << " " << frameIndex << " " << ident;
    }

  private:
    unsigned frameIndex;
};

struct InstrGetGlobal : public IdentInstrBase
{
    define_instr_members(GetGlobal);

    InstrGetGlobal(Object* global, Name ident)
      : IdentInstrBase(ident), global(global)
    {
        assert(global);
    }

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &global);
    }

  private:
    Object* global;
};

struct InstrSetGlobal : public IdentInstrBase
{
    define_instr_members(SetGlobal);

    InstrSetGlobal(Object* global, Name ident)
      : IdentInstrBase(ident), global(global)
    {
        assert(global);
    }

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &global);
    }

  private:
    Object* global;
};

struct InstrDelGlobal : public IdentInstrBase
{
    define_instr_members(DelGlobal);

    InstrDelGlobal(Object* global, Name ident)
      : IdentInstrBase(ident), global(global)
    {
        assert(global);
    }

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &global);
    }

  private:
    Object* global;
};

define_ident_instr(GetAttr);
define_ident_instr(SetAttr);
define_ident_instr(DelAttr);

struct InstrGetMethod : public IdentInstrBase
{
    define_instr_members(GetMethod);
    InstrGetMethod(Name name) : IdentInstrBase(name) {}

    static bool fallback(Traced<InstrGetMethod*> self, Interpreter& interp);
};

struct InstrGetMethodInt : public IdentInstrBase
{
    define_instr_members(GetMethodInt);
    InstrGetMethodInt(Name name, Traced<Value> result)
      : IdentInstrBase(name), result_(result) {}

     virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &result_);
    }

  private:
    Value result_;
};

struct InstrCall : public Instr
{
    define_instr_members(Call);
    InstrCall(unsigned count) : count_(count) {}

    virtual void print(ostream& s) const {
        s << name() << " " << count_;
    }

  protected:
    unsigned count_;
};

struct InstrCallMethod : public InstrCall
{
    define_instr_members(CallMethod);
    InstrCallMethod(unsigned count) : InstrCall(count) {}
};

define_simple_instr(CreateEnv);
define_simple_instr(InitStackLocals);
define_simple_instr(Return);
define_simple_instr(In);
define_simple_instr(Is);
define_simple_instr(Not);

struct Branch : public Instr
{
    Branch(int offset = 0) : offset_(offset) {}
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
    define_instr_members(BranchAlways);
    InstrBranchAlways(int offset = 0) : Branch(offset) {}
};

#define define_branch_instr(name)                                             \
    struct Instr##name : public Branch                                        \
    {                                                                         \
        define_instr_members(name);                                           \
    }

define_branch_instr(BranchIfTrue);
define_branch_instr(BranchIfFalse);
define_branch_instr(Or);
define_branch_instr(And);

struct InstrLambda : public Instr
{
    define_instr_members(Lambda);

    InstrLambda(Name name, const vector<Name>& paramNames, Traced<Block*> block,
                unsigned defaultCount = 0, bool takesRest = false,
                bool isGenerator = false);

    Name functionName() const { return funcName_; }
    const vector<Name>& paramNames() const { return info_->params_; }
    Block* block() const { return info_->block_; }
    unsigned defaultCount() const { return info_->defaultCount_; }
    bool takesRest() const { return info_->takesRest_; }
    bool isGenerator() const { return info_->isGenerator_; }

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &info_);
    }

    virtual void print(ostream& s) const {
        s << name();
        for (auto i = info_->params_.begin(); i != info_->params_.end(); ++i)
            s << " " << *i;
    }

  private:
    Name funcName_;
    FunctionInfo *info_;
};

struct InstrDup : public Instr
{
    define_instr_members(Dup);
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
    define_instr_members(Tuple);
    InstrTuple(unsigned size) : size(size) {}

  private:
    unsigned size;
};

struct InstrList : public Instr
{
    define_instr_members(List);
    InstrList(unsigned size) : size(size) {}

  private:
    unsigned size;
};

struct InstrDict : public Instr
{
    define_instr_members(Dict);
    InstrDict(unsigned size) : size(size) {}

  private:
    unsigned size;
};

define_simple_instr(Slice);

define_simple_instr(AssertionFailed);

define_ident_instr(MakeClassFromFrame);

struct InstrDestructure : public Instr
{
    define_instr_members(Destructure);
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
    define_instr_members(BinaryOp);
    InstrBinaryOp(BinaryOp op) : BinaryOpInstr(op) {}
    static bool fallback(Traced<InstrBinaryOp*> self, Interpreter& interp);
};

struct InstrBinaryOpInt : public BinaryOpInstr
{
    define_instr_members(BinaryOpInt);
    InstrBinaryOpInt(BinaryOp op, Traced<Value> method)
      : BinaryOpInstr(op), method_(method) {}

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &method_);
    }

  private:
    Value method_;
};

struct InstrAugAssignUpdate : public BinaryOpInstr
{
    define_instr_members(AugAssignUpdate);
    InstrAugAssignUpdate(BinaryOp op) : BinaryOpInstr(op) {}
};

define_simple_instr(StartGenerator);
define_simple_instr(ResumeGenerator);
define_simple_instr(LeaveGenerator);
define_simple_instr(SuspendGenerator);

define_branch_instr(EnterCatchRegion);
define_simple_instr(LeaveCatchRegion);
define_simple_instr(MatchCurrentException);
define_simple_instr(HandleCurrentException);
define_branch_instr(EnterFinallyRegion);
define_simple_instr(LeaveFinallyRegion);
define_simple_instr(FinishExceptionHandler);

struct InstrLoopControlJump : public Instr
{
    define_instr_members(LoopControlJump);

    InstrLoopControlJump(unsigned finallyCount, unsigned target = 0)
      : finallyCount_(finallyCount), target_(target)
    {}

    void setTarget(unsigned target) {
        assert(!target_);
        assert(target);
        target_ = target;
    }

    virtual void print(ostream& s) const {
        s << name() << " " << finallyCount_ << " " << target_;
    }

  private:
    unsigned finallyCount_;
    unsigned target_;
};

struct InstrAssertStackDepth : public Instr
{
    define_instr_members(AssertStackDepth);
    InstrAssertStackDepth(unsigned expected) : expected_(expected) {}

    virtual void print(ostream& s) const {
        s << name() << " " << expected_;
    }

  private:
    unsigned expected_;
};

#undef instr_type
#undef instr_name
#undef instr_execute
#undef define_instr_members
#undef define_simple_instr
#undef define_branch_instr
#undef define_ident_instr

#endif
