#ifndef __INSTR_H__
#define __INSTR_H__

#include "callable.h"
#include "gcdefs.h"
#include "name.h"

#include "value-inl.h"

#include <iostream>
#include <ostream>

using namespace std;

struct Interpreter;

#define for_each_binary_op_to_inline(op)                                     \
    op(Add)                                                                  \
    op(Sub)                                                                  \
    op(Mul)                                                                  \
    op(TrueDiv)

#define for_each_inline_instr(instr)                                         \
    instr(Abort, InstrAbort)                                                 \
    instr(Return, InstrReturn)

#define for_each_outofline_instr(instr)                                      \
    instr(Const, InstrConst)                                                 \
    instr(GetStackLocal, InstrGetStackLocal)                                 \
    instr(SetStackLocal, InstrSetStackLocal)                                 \
    instr(DelStackLocal, InstrDelStackLocal)                                 \
    instr(GetLexical, InstrGetLexical)                                       \
    instr(SetLexical, InstrSetLexical)                                       \
    instr(DelLexical, InstrDelLexical)                                       \
    instr(GetGlobal, InstrGetGlobal)                                         \
    instr(SetGlobal, InstrSetGlobal)                                         \
    instr(DelGlobal, InstrDelGlobal)                                         \
    instr(GetAttr, InstrGetAttr)                                             \
    instr(SetAttr, InstrSetAttr)                                             \
    instr(DelAttr, InstrDelAttr)                                             \
    instr(GetMethod, InstrGetMethod)                                         \
    instr(GetMethodFallback, InstrGetMethodFallback)                         \
    instr(GetMethodBuiltin, InstrGetMethodBuiltin)                           \
    instr(Call, InstrCall)                                                   \
    instr(CallMethod, InstrCallMethod)                                       \
    instr(CreateEnv, InstrCreateEnv)                                         \
    instr(InitStackLocals, InstrInitStackLocals)                             \
    instr(In, InstrIn)                                                       \
    instr(Is, InstrIs)                                                       \
    instr(Not, InstrNot)                                                     \
    instr(BranchAlways, InstrBranchAlways)                                   \
    instr(BranchIfTrue, InstrBranchIfTrue)                                   \
    instr(BranchIfFalse, InstrBranchIfFalse)                                 \
    instr(Or, InstrOr)                                                       \
    instr(And, InstrAnd)                                                     \
    instr(Lambda, InstrLambda)                                               \
    instr(Dup, InstrDup)                                                     \
    instr(Pop, InstrPop)                                                     \
    instr(Swap, InstrSwap)                                                   \
    instr(Tuple, InstrTuple)                                                 \
    instr(List, InstrList)                                                   \
    instr(Dict, InstrDict)                                                   \
    instr(Slice, InstrSlice)                                                 \
    instr(AssertionFailed, InstrAssertionFailed)                             \
    instr(MakeClassFromFrame, InstrMakeClassFromFrame)                       \
    instr(Destructure, InstrDestructure)                                     \
    instr(DestructureList, InstrDestructure)                                 \
    instr(DestructureFallback, InstrDestructure)                             \
    instr(Raise, InstrRaise)                                                 \
    instr(GetIterator, InstrGetIterator)                                     \
    instr(IteratorNext, InstrIteratorNext)                                   \
    instr(BinaryOp, InstrBinaryOp)                                           \
    instr(BinaryOpFallback, InstrBinaryOpFallback)                           \
    instr(BinaryOpInt_Add, InstrBinaryOpInt)                                 \
    instr(BinaryOpInt_Sub, InstrBinaryOpInt)                                 \
    instr(BinaryOpInt_Mul, InstrBinaryOpInt)                                 \
    instr(BinaryOpInt_TrueDiv, InstrBinaryOpInt)                             \
    instr(BinaryOpFloat_Add, InstrBinaryOpFloat)                             \
    instr(BinaryOpFloat_Sub, InstrBinaryOpFloat)                             \
    instr(BinaryOpFloat_Mul, InstrBinaryOpFloat)                             \
    instr(BinaryOpFloat_TrueDiv, InstrBinaryOpFloat)                         \
    instr(BinaryOpBuiltin, InstrBinaryOpBuiltin)                             \
    instr(CompareOp, InstrCompareOp)                                         \
    instr(CompareOpFallback, InstrCompareOpFallback)                         \
    instr(CompareOpInt_LT, InstrCompareOpInt)                                \
    instr(CompareOpInt_LE, InstrCompareOpInt)                                \
    instr(CompareOpInt_GT, InstrCompareOpInt)                                \
    instr(CompareOpInt_GE, InstrCompareOpInt)                                \
    instr(CompareOpInt_EQ, InstrCompareOpInt)                                \
    instr(CompareOpInt_NE, InstrCompareOpInt)                                \
    instr(CompareOpFloat_LT, InstrCompareOpFloat)                            \
    instr(CompareOpFloat_LE, InstrCompareOpFloat)                            \
    instr(CompareOpFloat_GT, InstrCompareOpFloat)                            \
    instr(CompareOpFloat_GE, InstrCompareOpFloat)                            \
    instr(CompareOpFloat_EQ, InstrCompareOpFloat)                            \
    instr(CompareOpFloat_NE, InstrCompareOpFloat)                            \
    instr(AugAssignUpdate, InstrAugAssignUpdate)                             \
    instr(AugAssignUpdateFallback, InstrAugAssignUpdateFallback)             \
    instr(AugAssignUpdateInt_Add, InstrAugAssignUpdateInt)                   \
    instr(AugAssignUpdateInt_Sub, InstrAugAssignUpdateInt)                   \
    instr(AugAssignUpdateInt_Mul, InstrAugAssignUpdateInt)                   \
    instr(AugAssignUpdateInt_TrueDiv, InstrAugAssignUpdateInt)               \
    instr(AugAssignUpdateFloat_Add, InstrAugAssignUpdateFloat)               \
    instr(AugAssignUpdateFloat_Sub, InstrAugAssignUpdateFloat)               \
    instr(AugAssignUpdateFloat_Mul, InstrAugAssignUpdateFloat)               \
    instr(AugAssignUpdateFloat_TrueDiv, InstrAugAssignUpdateFloat)           \
    instr(AugAssignUpdateBuiltin, InstrAugAssignUpdateBuiltin)               \
    instr(StartGenerator, InstrStartGenerator)                               \
    instr(ResumeGenerator, InstrResumeGenerator)                             \
    instr(LeaveGenerator, InstrLeaveGenerator)                               \
    instr(SuspendGenerator, InstrSuspendGenerator)                           \
    instr(EnterCatchRegion, InstrEnterCatchRegion)                           \
    instr(LeaveCatchRegion, InstrLeaveCatchRegion)                           \
    instr(MatchCurrentException, InstrMatchCurrentException)                 \
    instr(HandleCurrentException, InstrHandleCurrentException)               \
    instr(EnterFinallyRegion, InstrEnterFinallyRegion)                       \
    instr(LeaveFinallyRegion, InstrLeaveFinallyRegion)                       \
    instr(FinishExceptionHandler, InstrFinishExceptionHandler)               \
    instr(LoopControlJump, InstrLoopControlJump)                             \
    instr(AssertStackDepth, InstrAssertStackDepth)

enum InstrType
{
#define instr_enum(name, cls) Instr_##name,
    for_each_inline_instr(instr_enum)
    for_each_outofline_instr(instr_enum)
#undef instr_enum
    InstrTypeCount
};

extern const char* instrName(InstrType type);

struct Branch;

// Base class for instruction data.  By default each instruction type has its
// own class.
struct Instr : public Cell
{
    bool is(InstrType t) const { return type() == t; }

    virtual ~Instr() {}
    virtual InstrType type() const = 0;

    virtual bool isBranch() const { return false; };
    inline Branch* asBranch();

    void print(ostream& s) const override {}
};

// Base class for instruction data that can be used by more than one instruction
// type.
struct SharedInstrBase : public Instr
{
    SharedInstrBase(InstrType type) : type_(type) {}
    InstrType type() const override { return type_; }

  private:
    const InstrType type_;
};

#define instr_type(it)                                                        \
    InstrType type() const override { return it; }

#define define_instr_members(it)                                              \
    instr_type(Instr_##it);

#define define_simple_instr(it)                                               \
    struct Instr##it : public Instr                                           \
    {                                                                         \
        define_instr_members(it);                                             \
    }

struct IdentInstrBase : public Instr
{
    IdentInstrBase(Name ident) : ident(ident) {}

    void print(ostream& s) const override {
        s << " " << ident;
    }

    // todo: move to Interpreter
    bool raiseAttrError(Traced<Value> value, Interpreter& interp);

    const Name ident;
};

#define define_ident_instr(name)                                              \
    struct Instr##name : public IdentInstrBase                                \
    {                                                                         \
        Instr##name(Name ident) : IdentInstrBase(ident) {}                    \
        define_instr_members(name);                                           \
    }

struct IdentAndSlotInstrBase : public Instr
{
    IdentAndSlotInstrBase(Name ident, unsigned slot)
      : ident(ident), slot(slot) {}

    void print(ostream& s) const override {
        s << " " << ident << " " << slot;
    }

    // todo: move to Interpreter
    bool raiseAttrError(Traced<Value> value, Interpreter& interp);

    const Name ident;
    const unsigned slot;
};

#define define_ident_and_slot_instr(name)                                     \
    struct Instr##name : public IdentAndSlotInstrBase                         \
    {                                                                         \
        Instr##name(Name ident, unsigned slot)                                \
          : IdentAndSlotInstrBase(ident, slot)                                \
        {}                                                                    \
        define_instr_members(name);                                           \
    }

struct FrameAndIdentInstrBase : public Instr
{
    FrameAndIdentInstrBase(unsigned frameIndex, Name ident)
      : frameIndex(frameIndex), ident(ident) {}

    void print(ostream& s) const override {
        s << " " << frameIndex << " " << ident;
    }

    const unsigned frameIndex;
    const Name ident;
};

#define define_frame_and_ident_instr(name)                                    \
    struct Instr##name : public FrameAndIdentInstrBase                        \
    {                                                                         \
        Instr##name(unsigned frameIndex, Name ident)                          \
          : FrameAndIdentInstrBase(frameIndex, ident)                         \
        {}                                                                    \
        define_instr_members(name);                                           \
    }

struct GlobalInstrBase : public IdentInstrBase
{
    GlobalInstrBase(Traced<Object*> global, Name ident)
      : IdentInstrBase(ident), global_(global)
    {
        assert(global);
    }

    Object* global() const { return global_; }

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &global_);
    }

  private:
    Heap<Object*> global_;
};

#define define_global_instr(name)                                             \
    struct Instr##name : public GlobalInstrBase                               \
    {                                                                         \
        Instr##name(Traced<Object*> global, Name ident)                       \
          : GlobalInstrBase(global, ident)                                    \
        {}                                                                    \
        define_instr_members(name);                                           \
    }

define_simple_instr(Abort);
define_simple_instr(Return);

struct InstrConst : public Instr
{
    define_instr_members(Const);
    explicit InstrConst(Traced<Value> v) : value_(v) {}

    void print(ostream& s) const override {
        s << " " << value_;
    }

    Value value() { return value_; }

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &value_);
    }

  private:
    Heap<Value> value_;
};

define_ident_and_slot_instr(GetStackLocal);
define_ident_and_slot_instr(SetStackLocal);
define_ident_and_slot_instr(DelStackLocal);

define_frame_and_ident_instr(GetLexical);
define_frame_and_ident_instr(SetLexical);
define_frame_and_ident_instr(DelLexical);

define_global_instr(GetGlobal);
define_global_instr(SetGlobal);
define_global_instr(DelGlobal);

define_ident_instr(GetAttr);
define_ident_instr(SetAttr);
define_ident_instr(DelAttr);
define_ident_instr(GetMethod);
define_ident_instr(GetMethodFallback);

struct InstrGetMethodBuiltin : public IdentInstrBase
{
    define_instr_members(GetMethodBuiltin);
    InstrGetMethodBuiltin(Name name, Traced<Class*> cls, Traced<Value> result)
      : IdentInstrBase(name), class_(cls), result_(result) {}

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &class_);
        gc.trace(t, &result_);
    }

    Heap<Class*> class_;
    Heap<Value> result_;
};

struct InstrCall : public Instr
{
    define_instr_members(Call);
    InstrCall(unsigned count) : count(count) {}

    void print(ostream& s) const override {
        s << " " << count;
    }

    const unsigned count;
};

struct InstrCallMethod : public InstrCall
{
    define_instr_members(CallMethod);
    InstrCallMethod(unsigned count) : InstrCall(count) {}
};

define_simple_instr(CreateEnv);
define_simple_instr(InitStackLocals);
define_simple_instr(In);
define_simple_instr(Is);
define_simple_instr(Not);

struct Branch : public Instr
{
    Branch(int offset = 0) : offset_(offset) {}
    bool isBranch() const override { return true; };

    void setOffset(int offset) {
        assert(!offset_);
        offset_ = offset;
    }

    void print(ostream& s) const override {
        s << " " << offset_;
    }

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
    FunctionInfo* functionInfo() const { return info_; }
    const vector<Name>& paramNames() const { return info_->params_; }
    Block* block() const { return info_->block_; }
    unsigned defaultCount() const { return info_->defaultCount_; }
    bool takesRest() const { return info_->takesRest_; }
    bool isGenerator() const { return info_->isGenerator_; }

    void traceChildren(Tracer& t) override {
        gc.trace(t, &info_);
    }

    void print(ostream& s) const override {
        for (auto i = info_->params_.begin(); i != info_->params_.end(); ++i) {
            s << " " << *i;
        }
    }

  private:
    Name funcName_;
    Heap<FunctionInfo*> info_;
};

struct InstrDup : public Instr
{
    define_instr_members(Dup);
    InstrDup(unsigned index = 0) : index(index) {}

    void print(ostream& s) const override {
        s << " " << index;
    }

    const unsigned index;
};

define_simple_instr(Pop);
define_simple_instr(Swap);

struct InstrTuple : public Instr
{
    define_instr_members(Tuple);
    InstrTuple(unsigned size) : size(size) {}
    const unsigned size;
};

struct InstrList : public Instr
{
    define_instr_members(List);
    InstrList(unsigned size) : size(size) {}
    const unsigned size;
};

struct InstrDict : public Instr
{
    define_instr_members(Dict);
    InstrDict(unsigned size) : size(size) {}
    const unsigned size;
};

define_simple_instr(Slice);

define_simple_instr(AssertionFailed);

define_ident_instr(MakeClassFromFrame);

struct InstrDestructure : public SharedInstrBase
{
    InstrDestructure(unsigned count, InstrType type = Instr_Destructure)
        : SharedInstrBase(type), count(count)
    {
        assert(type >= Instr_Destructure && type <= Instr_DestructureFallback);
    }

    const unsigned count;
};

define_simple_instr(Raise);
define_simple_instr(GetIterator);
define_simple_instr(IteratorNext);

struct BinaryOpInstr : public Instr
{
    BinaryOpInstr(BinaryOp op) : op(op) {}

    void print(ostream& s) const override;

    const BinaryOp op;
};

struct SharedBinaryOpInstr : public SharedInstrBase
{
    SharedBinaryOpInstr(unsigned type, BinaryOp op)
      : SharedInstrBase(InstrType(type)), op(op)
    {
        assert(type < InstrTypeCount);
    }

    void print(ostream& s) const override;

    const BinaryOp op;
};

struct InstrBinaryOp : public BinaryOpInstr
{
    define_instr_members(BinaryOp);
    InstrBinaryOp(BinaryOp op) : BinaryOpInstr(op) {}
};

struct InstrBinaryOpFallback : public BinaryOpInstr
{
    define_instr_members(BinaryOpFallback);
    InstrBinaryOpFallback(BinaryOp op) : BinaryOpInstr(op) {}
};

struct InstrBinaryOpInt : public SharedBinaryOpInstr
{
    InstrBinaryOpInt(BinaryOp op)
      : SharedBinaryOpInstr(Instr_BinaryOpInt_Add + op, op)
    {}
};

struct InstrBinaryOpFloat : public SharedBinaryOpInstr
{
    InstrBinaryOpFloat(BinaryOp op)
      : SharedBinaryOpInstr(Instr_BinaryOpFloat_Add + op, op) {}
};

struct InstrBinaryOpBuiltin : public BinaryOpInstr
{
    define_instr_members(BinaryOpBuiltin);
    InstrBinaryOpBuiltin(BinaryOp op, Traced<Class*> left, Traced<Class*> right,
                         Traced<Value> method);
    Class* left() { return left_; }
    Class* right() { return right_; }
    Value method() { return method_; }

    void traceChildren(Tracer& t) override;

  private:
    Heap<Class*> left_;
    Heap<Class*> right_;
    Heap<Value> method_;
};

struct CompareOpInstr : public Instr
{
    CompareOpInstr(CompareOp op) : op(op) {}

    void print(ostream& s) const override;

    const CompareOp op;
};

struct SharedCompareOpInstr : public SharedInstrBase
{
    SharedCompareOpInstr(unsigned type, CompareOp op)
      : SharedInstrBase(InstrType(type)), op(op)
    {
        assert(type < InstrTypeCount);
    }

    void print(ostream& s) const override;

    const CompareOp op;
};

struct InstrCompareOp : public CompareOpInstr
{
    define_instr_members(CompareOp);
    InstrCompareOp(CompareOp op) : CompareOpInstr(op) {}
};

struct InstrCompareOpFallback : public CompareOpInstr
{
    define_instr_members(CompareOpFallback);
    InstrCompareOpFallback(CompareOp op) : CompareOpInstr(op) {}
};

struct InstrCompareOpInt : public SharedCompareOpInstr
{
    InstrCompareOpInt(CompareOp op)
      : SharedCompareOpInstr(Instr_CompareOpInt_LT + op, op) {}
};

struct InstrCompareOpFloat : public SharedCompareOpInstr
{
    InstrCompareOpFloat(CompareOp op)
      : SharedCompareOpInstr(Instr_CompareOpFloat_LT + op, op) {}
};

struct InstrAugAssignUpdate : public BinaryOpInstr
{
    define_instr_members(AugAssignUpdate);
    InstrAugAssignUpdate(BinaryOp op) : BinaryOpInstr(op) {}
};

struct InstrAugAssignUpdateFallback : public BinaryOpInstr
{
    define_instr_members(AugAssignUpdateFallback);
    InstrAugAssignUpdateFallback(BinaryOp op) : BinaryOpInstr(op) {}
};

struct InstrAugAssignUpdateInt : public SharedBinaryOpInstr
{
    InstrAugAssignUpdateInt(BinaryOp op)
      : SharedBinaryOpInstr(Instr_AugAssignUpdateInt_Add + op, op) {}
};

struct InstrAugAssignUpdateFloat : public SharedBinaryOpInstr
{
    InstrAugAssignUpdateFloat(BinaryOp op)
      : SharedBinaryOpInstr(Instr_AugAssignUpdateFloat_Add + op, op) {}
};

struct InstrAugAssignUpdateBuiltin : public BinaryOpInstr
{
    define_instr_members(AugAssignUpdateBuiltin);
    InstrAugAssignUpdateBuiltin(BinaryOp op,
                                Traced<Class*> left, Traced<Class*> right,
                                Traced<Value> method);
    Class* left() { return left_; }
    Class* right() { return right_; }
    Value method() { return method_; }

    void traceChildren(Tracer& t) override;

  private:
    Heap<Class*> left_;
    Heap<Class*> right_;
    Heap<Value> method_;
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

    unsigned finallyCount() const { return finallyCount_; }
    unsigned target() const { return target_; }

    void setTarget(unsigned target) {
        assert(!target_);
        assert(target);
        target_ = target;
    }

    void print(ostream& s) const override {
        s << " " << finallyCount_ << " " << target_;
    }

  private:
    unsigned finallyCount_;
    unsigned target_;
};

struct InstrAssertStackDepth : public Instr
{
    define_instr_members(AssertStackDepth);
    InstrAssertStackDepth(unsigned expected) : expected(expected) {}

    void print(ostream& s) const override {
        s << " " << expected;
    }

    const unsigned expected;
};

#undef instr_type
#undef define_instr_members
#undef define_simple_instr
#undef define_branch_instr
#undef define_ident_instr

#endif
