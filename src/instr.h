#ifndef __INSTR_H__
#define __INSTR_H__

#include "callable.h"
#include "gcdefs.h"
#include "name.h"

#include "value-inl.h"

#include <iostream>
#include <ostream>

using namespace std;

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
    instr(Raise, InstrRaise)                                                 \
    instr(GetIterator, InstrGetIterator)                                     \
    instr(IteratorNext, InstrIteratorNext)                                   \
    instr(BinaryOp, InstrBinaryOp)                                           \
    instr(BinaryOpFallback, InstrBinaryOpFallback)                           \
    instr(BinaryOpInt_Add, InstrBinaryOpInt)                                 \
    instr(BinaryOpInt_Sub, InstrBinaryOpInt)                                 \
    instr(BinaryOpInt_Mul, InstrBinaryOpInt)                                 \
    instr(BinaryOpInt_TrueDiv, InstrBinaryOpInt)                             \
    instr(BinaryOpFloat_Add, InstrBinaryOpFloat_Add)                         \
    instr(BinaryOpFloat_Sub, InstrBinaryOpFloat_Sub)                         \
    instr(BinaryOpFloat_Mul, InstrBinaryOpFloat_Mul)                         \
    instr(BinaryOpFloat_TrueDiv, InstrBinaryOpFloat_TrueDiv)                 \
    instr(BinaryOpBuiltin, InstrBinaryOpBuiltin)                             \
    instr(CompareOp, InstrCompareOp)                                         \
    instr(CompareOpFallback, InstrCompareOpFallback)                         \
    instr(CompareOpInt_LT, InstrCompareOpInt_LT)                             \
    instr(CompareOpInt_LE, InstrCompareOpInt_LE)                             \
    instr(CompareOpInt_GT, InstrCompareOpInt_GT)                             \
    instr(CompareOpInt_GE, InstrCompareOpInt_GE)                             \
    instr(CompareOpInt_EQ, InstrCompareOpInt_EQ)                             \
    instr(CompareOpInt_NE, InstrCompareOpInt_NE)                             \
    instr(CompareOpFloat_LT, InstrCompareOpFloat_LT)                         \
    instr(CompareOpFloat_LE, InstrCompareOpFloat_LE)                         \
    instr(CompareOpFloat_GT, InstrCompareOpFloat_GT)                         \
    instr(CompareOpFloat_GE, InstrCompareOpFloat_GE)                         \
    instr(CompareOpFloat_EQ, InstrCompareOpFloat_EQ)                         \
    instr(CompareOpFloat_NE, InstrCompareOpFloat_NE)                         \
    instr(AugAssignUpdate, InstrAugAssignUpdate)                             \
    instr(AugAssignUpdateFallback, InstrAugAssignUpdateFallback)             \
    instr(AugAssignUpdateInt_Add, InstrAugAssignUpdateInt_Add)               \
    instr(AugAssignUpdateInt_Sub, InstrAugAssignUpdateInt_Sub)               \
    instr(AugAssignUpdateInt_Mul, InstrAugAssignUpdateInt_Mul)               \
    instr(AugAssignUpdateInt_TrueDiv, InstrAugAssignUpdateInt_TrueDiv)       \
    instr(AugAssignUpdateFloat_Add, InstrAugAssignUpdateFloat_Add)           \
    instr(AugAssignUpdateFloat_Sub, InstrAugAssignUpdateFloat_Sub)           \
    instr(AugAssignUpdateFloat_Mul, InstrAugAssignUpdateFloat_Mul)           \
    instr(AugAssignUpdateFloat_TrueDiv, InstrAugAssignUpdateFloat_TrueDiv)   \
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

    const char* name() const { return instrName(type()); }
    virtual void print(ostream& s) const { s << name(); }
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

    virtual void print(ostream& s) const {
        s << name() << " " << ident;
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
        s << name() << " " << ident << " " << slot;
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
        s << name() << " " << frameIndex << " " << ident;
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
    Object* global_;
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
        s << name() << " " << value_;
    }

    Value value() { return value_; }

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &value_);
    }

  private:
    Value value_;
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

    Class* class_;
    Value result_;
};

struct InstrCall : public Instr
{
    define_instr_members(Call);
    InstrCall(unsigned count) : count(count) {}

    virtual void print(ostream& s) const {
        s << name() << " " << count;
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
    virtual bool isBranch() const { return true; };

    void setOffset(int offset) {
        assert(!offset_);
        offset_ = offset;
    }

    virtual void print(ostream& s) const {
        s << name() << " " << offset_;
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
    InstrDup(unsigned index = 0) : index(index) {}

    virtual void print(ostream& s) const {
        s << name() << " " << index;
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

define_simple_instr(Raise);
define_simple_instr(GetIterator);
define_simple_instr(IteratorNext);

struct BinaryOpInstr : public Instr
{
    BinaryOpInstr(BinaryOp op) : op(op) {}

    virtual void print(ostream& s) const;

    const BinaryOp op;
};

struct SharedBinaryOpInstr : public SharedInstrBase
{
    SharedBinaryOpInstr(unsigned type, BinaryOp op)
      : SharedInstrBase(InstrType(type)), op(op)
    {
        assert(type < InstrTypeCount);
    }

    virtual void print(ostream& s) const;

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

template <BinaryOp Op>
struct InstrBinaryOpFloat : public BinaryOpInstr
{
    instr_type(static_cast<InstrType>(Instr_BinaryOpFloat_Add + Op));
    InstrBinaryOpFloat() : BinaryOpInstr(Op) {}
};

#define typedef_binary_op_float(name)                                         \
    typedef InstrBinaryOpFloat<Binary##name> InstrBinaryOpFloat_##name;
    for_each_binary_op_to_inline(typedef_binary_op_float)
#undef typedef_binary_op_float

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
    Class* left_;
    Class* right_;
    Value method_;
};

struct CompareOpInstr : public Instr
{
    CompareOpInstr(CompareOp op) : op(op) {}

    virtual void print(ostream& s) const;

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

template <CompareOp Op>
struct InstrCompareOpInt : public CompareOpInstr
{
    instr_type(static_cast<InstrType>(Instr_CompareOpInt_LT + Op));
    InstrCompareOpInt() : CompareOpInstr(Op) {}
};

#define typedef_compare_op_int(name, token, method, rmethod)                  \
    typedef InstrCompareOpInt<Compare##name> InstrCompareOpInt_##name;
    for_each_compare_op(typedef_compare_op_int)
#undef typedef_compare_op_int

template <CompareOp Op>
struct InstrCompareOpFloat : public CompareOpInstr
{
    instr_type(static_cast<InstrType>(Instr_CompareOpFloat_LT + Op));
    InstrCompareOpFloat() : CompareOpInstr(Op) {}
};

#define typedef_compare_op_float(name, token, method, rmethod)                \
    typedef InstrCompareOpFloat<Compare##name> InstrCompareOpFloat_##name;
    for_each_compare_op(typedef_compare_op_float)
#undef typedef_compare_op_float

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

template <BinaryOp Op>
struct InstrAugAssignUpdateInt : public BinaryOpInstr
{
    instr_type(static_cast<InstrType>(Instr_AugAssignUpdateInt_Add + Op));
    InstrAugAssignUpdateInt() : BinaryOpInstr(Op) {}
};

#define typedef_binary_op_int(name)                                           \
    typedef InstrAugAssignUpdateInt<Binary##name>                             \
        InstrAugAssignUpdateInt_##name;
    for_each_binary_op_to_inline(typedef_binary_op_int)
#undef typedef_binary_op_int

template <BinaryOp Op>
struct InstrAugAssignUpdateFloat : public BinaryOpInstr
{
    instr_type(static_cast<InstrType>(Instr_AugAssignUpdateFloat_Add + Op));
    InstrAugAssignUpdateFloat() : BinaryOpInstr(Op) {}
};

#define typedef_binary_op_float(name)                                         \
    typedef InstrAugAssignUpdateFloat<Binary##name>                           \
        InstrAugAssignUpdateFloat_##name;
    for_each_binary_op_to_inline(typedef_binary_op_float)
#undef typedef_binary_op_float

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
    Class* left_;
    Class* right_;
    Value method_;
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
    InstrAssertStackDepth(unsigned expected) : expected(expected) {}

    virtual void print(ostream& s) const {
        s << name() << " " << expected;
    }

    const unsigned expected;
};

#undef instr_type
#undef define_instr_members
#undef define_simple_instr
#undef define_branch_instr
#undef define_ident_instr

#endif
