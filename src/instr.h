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
    instr(Abort, Instr)                                                      \
    instr(Return, Instr)

#define for_each_outofline_instr(instr)                                      \
    instr(Const, ValueInstr)                                                 \
    instr(GetStackLocal, IdentAndSlotInstr)                                  \
    instr(SetStackLocal, IdentAndSlotInstr)                                  \
    instr(DelStackLocal, IdentAndSlotInstr)                                  \
    instr(GetLexical, FrameAndIdentInstr)                                    \
    instr(SetLexical, FrameAndIdentInstr)                                    \
    instr(DelLexical, FrameAndIdentInstr)                                    \
    instr(GetGlobal, GlobalAndIdentInstr)                                    \
    instr(GetGlobalSlot, GlobalSlotInstr)                                    \
    instr(GetBuiltinsSlot, BuiltinsSlotInstr)                                \
    instr(SetGlobal, GlobalAndIdentInstr)                                    \
    instr(SetGlobalSlot, GlobalSlotInstr)                                    \
    instr(DelGlobal, GlobalAndIdentInstr)                                    \
    instr(GetAttr, IdentInstr)                                               \
    instr(SetAttr, IdentInstr)                                               \
    instr(DelAttr, IdentInstr)                                               \
    instr(GetMethod, IdentInstr)                                             \
    instr(GetMethodFallback, IdentInstr)                                     \
    instr(GetMethodBuiltin, BuiltinMethodInstr)                              \
    instr(Call, CountInstr)                                                  \
    instr(CallMethod, CountInstr)                                            \
    instr(CreateEnv, Instr)                                                  \
    instr(InitStackLocals, Instr)                                            \
    instr(In, Instr)                                                         \
    instr(Is, Instr)                                                         \
    instr(Not, Instr)                                                        \
    instr(BranchAlways, BranchInstr)                                         \
    instr(BranchIfTrue, BranchInstr)                                         \
    instr(BranchIfFalse, BranchInstr)                                        \
    instr(Or, BranchInstr)                                                   \
    instr(And, BranchInstr)                                                  \
    instr(Lambda, LambdaInstr)                                               \
    instr(Dup, IndexInstr)                                                   \
    instr(Pop, Instr)                                                        \
    instr(Swap, Instr)                                                       \
    instr(Tuple, CountInstr)                                                 \
    instr(List, CountInstr)                                                  \
    instr(Dict, CountInstr)                                                  \
    instr(Slice, Instr)                                                      \
    instr(AssertionFailed, Instr)                                            \
    instr(MakeClassFromFrame, IdentInstr)                                    \
    instr(Destructure, CountInstr)                                           \
    instr(DestructureList, CountInstr)                                       \
    instr(DestructureFallback, CountInstr)                                   \
    instr(Raise, Instr)                                                      \
    instr(GetIterator, Instr)                                                \
    instr(IteratorNext, Instr)                                               \
    instr(BinaryOp, BinaryOpInstr)                                           \
    instr(CompareOp, CompareOpInstr)                                         \
    instr(AugAssignUpdate, BinaryOpInstr)                                    \
    instr(StartGenerator, Instr)                                             \
    instr(ResumeGenerator, Instr)                                            \
    instr(LeaveGenerator, Instr)                                             \
    instr(SuspendGenerator, Instr)                                           \
    instr(EnterCatchRegion, BranchInstr)                                     \
    instr(LeaveCatchRegion, Instr)                                           \
    instr(MatchCurrentException, Instr)                                      \
    instr(HandleCurrentException, Instr)                                     \
    instr(EnterFinallyRegion, BranchInstr)                                   \
    instr(LeaveFinallyRegion, Instr)                                         \
    instr(FinishExceptionHandler, Instr)                                     \
    instr(LoopControlJump, LoopControlJumpInstr)                             \
    instr(ListAppend, Instr)                                                 \
    instr(AssertStackDepth, CountInstr)

#define for_each_stub_instr(instr)                                           \
    instr(BinaryOpInt_Add, BinaryOpStubInstr)                                \
    instr(BinaryOpInt_Sub, BinaryOpStubInstr)                                \
    instr(BinaryOpInt_Mul, BinaryOpStubInstr)                                \
    instr(BinaryOpInt_TrueDiv, BinaryOpStubInstr)                            \
    instr(BinaryOpInt_FloorDiv, BinaryOpStubInstr)                           \
    instr(BinaryOpInt_Modulo, BinaryOpStubInstr)                             \
    instr(BinaryOpInt_Power, BinaryOpStubInstr)                              \
    instr(BinaryOpInt_Or, BinaryOpStubInstr)                                 \
    instr(BinaryOpInt_Xor, BinaryOpStubInstr)                                \
    instr(BinaryOpInt_And, BinaryOpStubInstr)                                \
    instr(BinaryOpInt_LeftShift, BinaryOpStubInstr)                          \
    instr(BinaryOpInt_RightShift, BinaryOpStubInstr)                         \
    instr(BinaryOpInteger_Add, BinaryOpStubInstr)                            \
    instr(BinaryOpInteger_Sub, BinaryOpStubInstr)                            \
    instr(BinaryOpInteger_Mul, BinaryOpStubInstr)                            \
    instr(BinaryOpInteger_TrueDiv, BinaryOpStubInstr)                        \
    instr(BinaryOpInteger_FloorDiv, BinaryOpStubInstr)                       \
    instr(BinaryOpInteger_Modulo, BinaryOpStubInstr)                         \
    instr(BinaryOpInteger_Power, BinaryOpStubInstr)                          \
    instr(BinaryOpInteger_Or, BinaryOpStubInstr)                             \
    instr(BinaryOpInteger_Xor, BinaryOpStubInstr)                            \
    instr(BinaryOpInteger_And, BinaryOpStubInstr)                            \
    instr(BinaryOpInteger_LeftShift, BinaryOpStubInstr)                      \
    instr(BinaryOpInteger_RightShift, BinaryOpStubInstr)                     \
    instr(BinaryOpFloat_Add, BinaryOpStubInstr)                              \
    instr(BinaryOpFloat_Sub, BinaryOpStubInstr)                              \
    instr(BinaryOpFloat_Mul, BinaryOpStubInstr)                              \
    instr(BinaryOpFloat_TrueDiv, BinaryOpStubInstr)                          \
    instr(BinaryOpBuiltin, BuiltinBinaryOpInstr)                             \
    instr(CompareOpInt_LT, CompareOpStubInstr)                               \
    instr(CompareOpInt_LE, CompareOpStubInstr)                               \
    instr(CompareOpInt_GT, CompareOpStubInstr)                               \
    instr(CompareOpInt_GE, CompareOpStubInstr)                               \
    instr(CompareOpInt_EQ, CompareOpStubInstr)                               \
    instr(CompareOpInt_NE, CompareOpStubInstr)                               \
    instr(CompareOpFloat_LT, CompareOpStubInstr)                             \
    instr(CompareOpFloat_LE, CompareOpStubInstr)                             \
    instr(CompareOpFloat_GT, CompareOpStubInstr)                             \
    instr(CompareOpFloat_GE, CompareOpStubInstr)                             \
    instr(CompareOpFloat_EQ, CompareOpStubInstr)                             \
    instr(CompareOpFloat_NE, CompareOpStubInstr)                             \
    instr(AugAssignUpdateInt_Add, BinaryOpStubInstr)                         \
    instr(AugAssignUpdateInt_Sub, BinaryOpStubInstr)                         \
    instr(AugAssignUpdateInt_Mul, BinaryOpStubInstr)                         \
    instr(AugAssignUpdateInt_TrueDiv, BinaryOpStubInstr)                     \
    instr(AugAssignUpdateFloat_Add, BinaryOpStubInstr)                       \
    instr(AugAssignUpdateFloat_Sub, BinaryOpStubInstr)                       \
    instr(AugAssignUpdateFloat_Mul, BinaryOpStubInstr)                       \
    instr(AugAssignUpdateFloat_TrueDiv, BinaryOpStubInstr)                   \
    instr(AugAssignUpdateBuiltin, BuiltinBinaryOpInstr)

#define for_each_instr(instr)                                                \
    for_each_inline_instr(instr)                                             \
    for_each_outofline_instr(instr)                                          \
    for_each_stub_instr(instr)

#define define_instr_enum(name, cls)                                         \
    Instr_##name,

enum InstrCode
{
    for_each_instr(define_instr_enum)
    InstrCodeCount
};

#undef define_instr_enum

extern const char* instrName(InstrCode code);

struct BranchInstr;

// Base class for instruction data.
struct Instr : public Cell
{
    Instr(InstrCode code)
      : code_(code)
    {
        assert(code < InstrCodeCount);
    }

    InstrCode code() const { return code_; }
    bool is(InstrCode t) const { return code() == t; }

    virtual bool isBranch() const { return false; };
    BranchInstr* asBranch();

    void print(ostream& s) const override;

  private:
    const InstrCode code_;
};

struct IdentInstr : public Instr
{
    IdentInstr(InstrCode code, Name ident) : Instr(code), ident(ident) {}

    void print(ostream& s) const override;

    // todo: move to Interpreter
    bool raiseAttrError(Traced<Value> value, Interpreter& interp);

    const Name ident;
};

struct IdentAndSlotInstr : public IdentInstr
{
    IdentAndSlotInstr(InstrCode code, Name ident, unsigned slot)
      : IdentInstr(code, ident), slot(slot)
    {}

    void print(ostream& s) const override;

    // todo: move to Interpreter
    bool raiseAttrError(Traced<Value> value, Interpreter& interp);

    const unsigned slot;
};

struct FrameAndIdentInstr : public IdentInstr
{
    FrameAndIdentInstr(InstrCode code, unsigned frameIndex, Name ident)
      : IdentInstr(code, ident), frameIndex(frameIndex)
    {}

    void print(ostream& s) const override;

    const unsigned frameIndex;
};

struct GlobalAndIdentInstr : public IdentInstr
{
    GlobalAndIdentInstr(InstrCode code, Traced<Object*> global, Name ident,
                        bool fallback = false)
      : IdentInstr(code, ident), global_(global), fallback_(fallback)
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

struct GlobalSlotInstr : public IdentInstr
{
    GlobalSlotInstr(InstrCode code, Traced<Object*> global, Name ident)
      : IdentInstr(code, ident), global_(global), globalSlot_(global, ident)
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

    GlobalSlotInstr(InstrCode code, Traced<Object*> global,
                       Name globalIdent, Name ident)
      : IdentInstr(code, ident),
        global_(global),
        globalSlot_(global, globalIdent)
    {
        assert(global);
    }
};

struct BuiltinsSlotInstr : public GlobalSlotInstr
{
    BuiltinsSlotInstr(InstrCode code, Traced<Object*> global, Name ident)
      : GlobalSlotInstr(code, global, Name::__builtins__, ident),
        builtinsSlot_(global->getSlot(globalSlot_.slot()).toObject(), ident)
    {}

    void traceChildren(Tracer& t) override {
        GlobalSlotInstr::traceChildren(t);
        builtinsSlot_.traceChildren(t);
    }

    int builtinsSlot(Traced<Object*> builtins) const {
        return builtinsSlot_.check(builtins);
    }

  private:
    SlotGuard builtinsSlot_;
};

struct CountInstr : public Instr
{
    CountInstr(InstrCode code, unsigned count) : Instr(code), count(count) {}

    void print(ostream& s) const override;

    const unsigned count;
};

struct IndexInstr : public Instr
{
    IndexInstr(InstrCode code, unsigned index) : Instr(code), index(index) {}

    void print(ostream& s) const override;

    const unsigned index;
};

struct ValueInstr : public Instr
{
    ValueInstr(InstrCode code, Traced<Value> v)
        : Instr(code), value_(v)
    {
        assert(code == Instr_Const);
    }

    void print(ostream& s) const override;
    void traceChildren(Tracer& t) override;

    Value value() { return value_; }

  private:
    Heap<Value> value_;
};

struct BuiltinMethodInstr : public IdentInstr
{
    BuiltinMethodInstr(InstrCode code, Name name, Traced<Class*> cls,
                       Traced<Value> result)
      : IdentInstr(code, name),
        class_(cls),
        result_(result)
    {}

    void traceChildren(Tracer& t) override;

    Heap<Class*> class_;
    Heap<Value> result_;
};

struct BranchInstr : public Instr
{
    BranchInstr(InstrCode code, int offset = 0)
      : Instr(code), offset_(offset)
    {}

    bool isBranch() const override {
        return true;
    };

    void setOffset(int offset) {
        assert(offset && !offset_);
        offset_ = offset;
    }

    int offset() const {
        return offset_;
    }

    void print(ostream& s) const override;

  private:
    int offset_;
};

struct LambdaInstr : public Instr
{
    LambdaInstr(InstrCode code,
                Name name,
                const vector<Name>& paramNames,
                Traced<Block*> block,
                unsigned defaultCount = 0,
                bool takesRest = false,
                bool isGenerator = false);

    Name functionName() const { return funcName_; }
    FunctionInfo* functionInfo() const { return info_; }
    const vector<Name>& paramNames() const { return info_->params_; }
    Block* block() const { return info_->block_; }
    unsigned defaultCount() const { return info_->defaultCount_; }
    bool takesRest() const { return info_->takesRest_; }
    bool isGenerator() const { return info_->isGenerator_; }

    void traceChildren(Tracer& t) override;
    void print(ostream& s) const override;

  private:
    Name funcName_;
    Heap<FunctionInfo*> info_;
};

struct BinaryOpInstrBase : public Instr
{
    BinaryOpInstrBase(InstrCode code, BinaryOp op) : Instr(code), op(op) {}

    void print(ostream& s) const override;

    const BinaryOp op;
};

struct BinaryOpInstr : public BinaryOpInstrBase
{
    BinaryOpInstr(InstrCode code, BinaryOp op)
      : BinaryOpInstrBase(code, op), stubCount(0)
    {}

    void print(ostream& s) const override;

    unsigned stubCount;
};

struct BinaryOpStubInstr : public BinaryOpInstrBase
{
    BinaryOpStubInstr(InstrCode code, BinaryOp op, Traced<Instr*> next)
      : BinaryOpInstrBase(code, op), next_(next)
    {
        assert(next_);
    }

    const Heap<Instr*>& next() { return next_; }

    void traceChildren(Tracer& t) override;
    void print(ostream& s) const override;

  private:
    Heap<Instr*> next_;
};

struct BuiltinBinaryOpInstr : public BinaryOpStubInstr
{
    BuiltinBinaryOpInstr(InstrCode code, BinaryOp op, Traced<Instr*> next,
                         Traced<Class*> left, Traced<Class*> right,
                         Traced<Value> method)
      : BinaryOpStubInstr(code, op, next),
        left_(left),
        right_(right),
        method_(method)
    {}

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
    CompareOpInstrBase(InstrCode code, CompareOp op)
      : Instr(code), op(op)
    {}

    void print(ostream& s) const override;

    const CompareOp op;
};

struct CompareOpInstr : public CompareOpInstrBase
{
    CompareOpInstr(InstrCode code, CompareOp op)
      : CompareOpInstrBase(code, op), stubCount(0)
    {}

    void print(ostream& s) const override;

    unsigned stubCount;
};

struct CompareOpStubInstr : public CompareOpInstr
{
    CompareOpStubInstr(InstrCode code, CompareOp op, Traced<Instr*> next)
      : CompareOpInstr(code, op), next_(next)
    {}

    const Heap<Instr*>& next() { return next_; }

    void traceChildren(Tracer& t) override;
    void print(ostream& s) const override;

  private:
    Heap<Instr*> next_;
};

struct LoopControlJumpInstr : public Instr
{
    LoopControlJumpInstr(InstrCode code, unsigned finallyCount,
                         unsigned target = 0)
      : Instr(Instr_LoopControlJump),
        finallyCount_(finallyCount),
        target_(target)
    {
        assert(code == Instr_LoopControlJump);
    }

    unsigned finallyCount() const { return finallyCount_; }
    unsigned target() const { return target_; }

    void setTarget(unsigned target) {
        assert(!target_);
        assert(target);
        target_ = target;
    }

    void print(ostream& s) const override;

  private:
    unsigned finallyCount_;
    unsigned target_;
};

template <InstrCode Code>
struct InstrFactory
{};

#define define_instr_factory(code, cls)                                       \
    template <>                                                               \
    struct InstrFactory<Instr_##code>                                         \
    {                                                                         \
        template <typename... Args>                                           \
        static Instr* get(Args&&... args) {                                   \
            return gc.create<cls>(Instr_##code, forward<Args>(args)...);      \
        }                                                                     \
    };

for_each_inline_instr(define_instr_factory)
for_each_outofline_instr(define_instr_factory)

#undef define_instr_factory

extern Instr* getNextInstr(Instr* instr);
extern Instr* getFinalInstr(Instr* instr);

#endif
