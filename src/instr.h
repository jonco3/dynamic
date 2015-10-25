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

#define for_each_simple_binary_op_type(op)                                   \
    op(Add)                                                                  \
    op(Sub)                                                                  \
    op(Mul)                                                                  \
    op(TrueDiv)

#define for_each_binary_op_type(op)                                          \
    for_each_simple_binary_op_type(op)                                       \
    op(FloorDiv)                                                             \
    op(Modulo)                                                               \
    op(Power)                                                                \
    op(Or)                                                                   \
    op(Xor)                                                                  \
    op(And)                                                                  \
    op(LeftShift)                                                            \
    op(RightShift)

#define for_each_int_binary_op_to_inline(op)                                 \
    for_each_binary_op_type(op)

#define for_each_float_binary_op_to_inline(op)                               \
    for_each_simple_binary_op_type(op)

#define for_each_int_aug_assign_op_to_inline(op)                             \
    for_each_simple_binary_op_type(op)

#define for_each_float_aug_assign_op_to_inline(op)                           \
    for_each_simple_binary_op_type(op)

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
    instr(GetGlobalSlot, InstrGetGlobalSlot)                                 \
    instr(GetBuiltinsSlot, InstrGetBuiltinsSlot)                             \
    instr(SetGlobal, InstrSetGlobal)                                         \
    instr(SetGlobalSlot, InstrSetGlobalSlot)                                 \
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
    instr(BinaryOpInt_FloorDiv, InstrBinaryOpInt)                            \
    instr(BinaryOpInt_Modulo, InstrBinaryOpInt)                              \
    instr(BinaryOpInt_Power, InstrBinaryOpInt)                               \
    instr(BinaryOpInt_Or, InstrBinaryOpInt)                                  \
    instr(BinaryOpInt_Xor, InstrBinaryOpInt)                                 \
    instr(BinaryOpInt_And, InstrBinaryOpInt)                                 \
    instr(BinaryOpInt_LeftShift, InstrBinaryOpInt)                           \
    instr(BinaryOpInt_RightShift, InstrBinaryOpInt)                          \
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
    instr(ListAppend, InstrListAppend)                                       \
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

// Base class for instruction data.
struct Instr : public Cell
{
    Instr(InstrType type)
      : type_(type)
    {
        assert(type < InstrTypeCount);
    }

    InstrType type() const { return type_; }
    bool is(InstrType t) const { return type() == t; }

    virtual bool isBranch() const { return false; };
    inline Branch* asBranch();

    void print(ostream& s) const override {}

  private:
    const InstrType type_;
};

#define define_simple_instr(it)                                               \
    struct Instr##it : public Instr                                           \
    {                                                                         \
        Instr##it() : Instr(Instr_##it) {}                                    \
    }

struct IdentInstrBase : public Instr
{
    IdentInstrBase(InstrType type, Name ident) : Instr(type), ident(ident) {}

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
        Instr##name(Name ident) : IdentInstrBase(Instr_##name, ident) {}      \
    }

struct IdentAndSlotInstrBase : public Instr
{
    IdentAndSlotInstrBase(InstrType type, Name ident, unsigned slot)
      : Instr(type), ident(ident), slot(slot) {}

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
          : IdentAndSlotInstrBase(Instr_##name, ident, slot)                  \
        {}                                                                    \
    }

struct FrameAndIdentInstrBase : public Instr
{
    FrameAndIdentInstrBase(InstrType type, unsigned frameIndex, Name ident)
      : Instr(type), frameIndex(frameIndex), ident(ident) {}

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
          : FrameAndIdentInstrBase(Instr_##name, frameIndex, ident)           \
        {}                                                                    \
    }

struct GlobalInstrBase : public IdentInstrBase
{
    GlobalInstrBase(InstrType type, Traced<Object*> global, Name ident,
                  bool fallback)
      : IdentInstrBase(type, ident), global_(global), fallback_(fallback)
    {
        assert(global);
    }

    const Heap<Object*>& global() const { return global_; }
    bool isFallback() const { return fallback_; }

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &global_);
    }

  private:
    Heap<Object*> global_;
    bool fallback_;
};

#define define_global_instr(name)                                             \
    struct Instr##name : public GlobalInstrBase                               \
    {                                                                         \
        Instr##name(Traced<Object*> global, Name ident,                       \
                    bool fallback = false)                                    \
          : GlobalInstrBase(Instr_##name, global, ident, fallback)            \
        {}                                                                    \
    }

struct SlotGuard
{
    SlotGuard(Object* object, Name name)
      : layout_(object->layout()), slot_(object->findOwnAttr(name))
    {
        assert(slot_ != Layout::NotFound);
    }

    void traceChildren(Tracer& t) {
        gc.trace(t, &layout_);
    }

    int slot() const {
        return slot_;
    }

    int check(Traced<Object*> object) const {
        return object->layout() == layout_ ? slot_ : Layout::NotFound;
    }

  private:
    Heap<Layout*> layout_;
    int slot_;
};

struct GlobalSlotInstrBase : public IdentInstrBase
{
    GlobalSlotInstrBase(InstrType type, Traced<Object*> global, Name ident)
      : IdentInstrBase(type, ident), global_(global), globalSlot_(global, ident)
    {
        assert(global);
    }

    void traceChildren(Tracer& t) override {
        gc.trace(t, &global_);
        globalSlot_.traceChildren(t);
    }

    Object* global() const { return global_; }

    int globalSlot() const {
        return globalSlot_.check(global_);
    }

  protected:
    Heap<Object*> global_;
    SlotGuard globalSlot_;

    GlobalSlotInstrBase(InstrType type, Traced<Object*> global,
                        Name globalIdent, Name ident)
      : IdentInstrBase(type, ident),
        global_(global),
        globalSlot_(global, globalIdent)
    {
        assert(global);
    }
};

#define define_global_slot_instr(name)                                        \
    struct Instr##name : public GlobalSlotInstrBase                           \
    {                                                                         \
        Instr##name(Traced<Object*> global, Name ident)                       \
          : GlobalSlotInstrBase(Instr_##name, global, ident)                  \
        {}                                                                    \
    }

struct BuiltinsSlotInstrBase : public GlobalSlotInstrBase
{
    BuiltinsSlotInstrBase(InstrType type, Traced<Object*> global, Name ident)
      : GlobalSlotInstrBase(type, global, Name::__builtins__, ident),
        builtinsSlot_(global->getSlot(globalSlot_.slot()).toObject(), ident)
    {}

    void traceChildren(Tracer& t) override {
        GlobalSlotInstrBase::traceChildren(t);
        builtinsSlot_.traceChildren(t);
    }

    int builtinsSlot(Traced<Object*> builtins) const {
        return builtinsSlot_.check(builtins);
    }

  private:
    SlotGuard builtinsSlot_;
};

#define define_builtins_slot_instr(name)                                      \
    struct Instr##name : public BuiltinsSlotInstrBase                         \
    {                                                                         \
        Instr##name(Traced<Object*> global, Name ident)                       \
          : BuiltinsSlotInstrBase(Instr_##name, global, ident)               \
        {}                                                                    \
    }

define_simple_instr(Abort);
define_simple_instr(Return);

struct InstrConst : public Instr
{
    explicit InstrConst(Traced<Value> v) : Instr(Instr_Const), value_(v) {}

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
define_global_slot_instr(GetGlobalSlot);
define_builtins_slot_instr(GetBuiltinsSlot);

define_global_instr(SetGlobal);
define_global_slot_instr(SetGlobalSlot);

define_global_instr(DelGlobal);

define_ident_instr(GetAttr);
define_ident_instr(SetAttr);
define_ident_instr(DelAttr);
define_ident_instr(GetMethod);
define_ident_instr(GetMethodFallback);

struct InstrGetMethodBuiltin : public IdentInstrBase
{
    InstrGetMethodBuiltin(Name name, Traced<Class*> cls, Traced<Value> result)
      : IdentInstrBase(Instr_GetMethodBuiltin, name),
        class_(cls),
        result_(result)
    {}

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &class_);
        gc.trace(t, &result_);
    }

    Heap<Class*> class_;
    Heap<Value> result_;
};

struct CallInstrBase : public Instr
{
    CallInstrBase(InstrType type, unsigned count)
        : Instr(type), count(count)
    {}

    void print(ostream& s) const override {
        s << " " << count;
    }

    const unsigned count;
};

struct InstrCall : public CallInstrBase
{
    InstrCall(unsigned count) : CallInstrBase(Instr_Call, count) {}
};

struct InstrCallMethod : public CallInstrBase
{
    InstrCallMethod(unsigned count) : CallInstrBase(Instr_CallMethod, count) {}
};

define_simple_instr(CreateEnv);
define_simple_instr(InitStackLocals);
define_simple_instr(In);
define_simple_instr(Is);
define_simple_instr(Not);

struct Branch : public Instr
{
    Branch(InstrType type, int offset = 0)
      : Instr(type), offset_(offset)
    {}

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

#define define_branch_instr(name)                                             \
    struct Instr##name : public Branch                                        \
    {                                                                         \
        Instr##name(int offset = 0)                                           \
          : Branch(Instr_##name, offset)                                      \
        {}                                                                    \
    }

define_branch_instr(BranchAlways);
define_branch_instr(BranchIfTrue);
define_branch_instr(BranchIfFalse);
define_branch_instr(Or);
define_branch_instr(And);

struct InstrLambda : public Instr
{
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
    InstrDup(unsigned index = 0) : Instr(Instr_Dup), index(index) {}

    void print(ostream& s) const override {
        s << " " << index;
    }

    const unsigned index;
};

define_simple_instr(Pop);
define_simple_instr(Swap);

struct InstrTuple : public Instr
{
    InstrTuple(unsigned size) : Instr(Instr_Tuple), size(size) {}
    const unsigned size;
};

struct InstrList : public Instr
{
    InstrList(unsigned size) : Instr(Instr_List), size(size) {}
    const unsigned size;
};

struct InstrDict : public Instr
{
    InstrDict(unsigned size) : Instr(Instr_Dict), size(size) {}
    const unsigned size;
};

define_simple_instr(Slice);

define_simple_instr(AssertionFailed);

define_ident_instr(MakeClassFromFrame);

struct InstrDestructure : public Instr
{
    InstrDestructure(unsigned count, InstrType type = Instr_Destructure)
      : Instr(type), count(count)
    {
        assert(type >= Instr_Destructure && type <= Instr_DestructureFallback);
    }

    const unsigned count;
};

define_simple_instr(Raise);
define_simple_instr(GetIterator);
define_simple_instr(IteratorNext);

struct BinaryOpInstrBase : public Instr
{
    BinaryOpInstrBase(unsigned type, BinaryOp op)
      : Instr(InstrType(type)), op(op)
    {}

    void print(ostream& s) const override;

    const BinaryOp op;
};

struct InstrBinaryOp : public BinaryOpInstrBase
{
    InstrBinaryOp(BinaryOp op)
      : BinaryOpInstrBase(Instr_BinaryOp, op)
    {}
};

struct InstrBinaryOpFallback : public BinaryOpInstrBase
{
    InstrBinaryOpFallback(BinaryOp op)
      : BinaryOpInstrBase(Instr_BinaryOpFallback, op)
    {}
};

struct InstrBinaryOpInt : public BinaryOpInstrBase
{
    InstrBinaryOpInt(BinaryOp op)
      : BinaryOpInstrBase(Instr_BinaryOpInt_Add + op, op)
    {}
};

struct InstrBinaryOpFloat : public BinaryOpInstrBase
{
    InstrBinaryOpFloat(BinaryOp op)
      : BinaryOpInstrBase(Instr_BinaryOpFloat_Add + op, op) {}
};

struct InstrBinaryOpBuiltin : public BinaryOpInstrBase
{
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

struct CompareOpInstrBase : public Instr
{
    CompareOpInstrBase(unsigned type, CompareOp op)
      : Instr(InstrType(type)), op(op)
    {}

    void print(ostream& s) const override;

    const CompareOp op;
};

struct InstrCompareOp : public CompareOpInstrBase
{
    InstrCompareOp(CompareOp op) : CompareOpInstrBase(Instr_CompareOp, op) {}
};

struct InstrCompareOpFallback : public CompareOpInstrBase
{
    InstrCompareOpFallback(CompareOp op)
      : CompareOpInstrBase(Instr_CompareOpFallback, op)
    {}
};

struct InstrCompareOpInt : public CompareOpInstrBase
{
    InstrCompareOpInt(CompareOp op)
      : CompareOpInstrBase(Instr_CompareOpInt_LT + op, op)
    {}
};

struct InstrCompareOpFloat : public CompareOpInstrBase
{
    InstrCompareOpFloat(CompareOp op)
      : CompareOpInstrBase(Instr_CompareOpFloat_LT + op, op)
    {}
};

struct InstrAugAssignUpdate : public BinaryOpInstrBase
{
    InstrAugAssignUpdate(BinaryOp op)
      : BinaryOpInstrBase(Instr_AugAssignUpdate, op)
    {}
};

struct InstrAugAssignUpdateFallback : public BinaryOpInstrBase
{
    InstrAugAssignUpdateFallback(BinaryOp op)
      : BinaryOpInstrBase(Instr_AugAssignUpdateFallback, op)
    {}
};

struct InstrAugAssignUpdateInt : public BinaryOpInstrBase
{
    InstrAugAssignUpdateInt(BinaryOp op)
      : BinaryOpInstrBase(Instr_AugAssignUpdateInt_Add + op, op) {}
};

struct InstrAugAssignUpdateFloat : public BinaryOpInstrBase
{
    InstrAugAssignUpdateFloat(BinaryOp op)
      : BinaryOpInstrBase(Instr_AugAssignUpdateFloat_Add + op, op) {}
};

struct InstrAugAssignUpdateBuiltin : public BinaryOpInstrBase
{
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
    InstrLoopControlJump(unsigned finallyCount, unsigned target = 0)
      : Instr(Instr_LoopControlJump),
        finallyCount_(finallyCount),
        target_(target)
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

define_simple_instr(ListAppend);

struct InstrAssertStackDepth : public Instr
{
    InstrAssertStackDepth(unsigned expected)
      : Instr(Instr_AssertStackDepth), expected(expected)
    {}

    void print(ostream& s) const override {
        s << " " << expected;
    }

    const unsigned expected;
};

#undef instr_type
#undef define_simple_instr
#undef define_branch_instr
#undef define_ident_instr

#endif
