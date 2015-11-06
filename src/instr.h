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

#define for_each_instr_type(type)                                            \
    type(Instr)                                                              \
    type(IdentInstr)                                                         \
    type(StackSlotInstr)                                                     \
    type(LexicalFrameInstr)                                                  \
    type(GlobalNameInstr)                                                    \
    type(GlobalSlotInstr)                                                    \
    type(BuiltinsSlotInstr)                                                  \
    type(CountInstr)                                                         \
    type(IndexInstr)                                                         \
    type(ValueInstr)                                                         \
    type(BuiltinMethodInstr)                                                 \
    type(BranchInstr)                                                        \
    type(LambdaInstr)                                                        \
    type(BinaryOpInstr)                                                      \
    type(BinaryOpStubInstr)                                                  \
    type(BuiltinBinaryOpInstr)                                               \
    type(CompareOpInstr)                                                     \
    type(CompareOpStubInstr)                                                 \
    type(LoopControlJumpInstr)

#define for_each_inline_instr(instr)                                         \
    instr(Abort, Instr)                                                      \
    instr(Return, Instr)

#define for_each_outofline_instr(instr)                                      \
    instr(Const, ValueInstr)                                                 \
    instr(GetStackLocal, StackSlotInstr)                                     \
    instr(SetStackLocal, StackSlotInstr)                                     \
    instr(DelStackLocal, StackSlotInstr)                                     \
    instr(GetLexical, LexicalFrameInstr)                                     \
    instr(SetLexical, LexicalFrameInstr)                                     \
    instr(DelLexical, LexicalFrameInstr)                                     \
    instr(GetGlobal, GlobalNameInstr)                                        \
    instr(GetGlobalSlot, GlobalSlotInstr)                                    \
    instr(GetBuiltinsSlot, BuiltinsSlotInstr)                                \
    instr(SetGlobal, GlobalNameInstr)                                        \
    instr(SetGlobalSlot, GlobalSlotInstr)                                    \
    instr(DelGlobal, GlobalNameInstr)                                        \
    instr(GetAttr, IdentInstr)                                               \
    instr(SetAttr, IdentInstr)                                               \
    instr(DelAttr, IdentInstr)                                               \
    instr(GetMethod, IdentInstr)                                             \
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
    instr(DestructureTuple, CountInstr)                                      \
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
    instr(GetMethodBuiltin, BuiltinMethodInstr)                              \
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

enum InstrType
{
#define define_instr_type_enum(type)                                         \
    InstrType_##type,

    for_each_instr_type(define_instr_type_enum)
    InstrTypeCount

#undef define_instr_type_enum
};

enum InstrCode : uint8_t
{
#define define_instr_code_enum(name, cls)                                    \
    Instr_##name,

    for_each_instr(define_instr_code_enum)
    InstrCodeCount

#undef define_instr_code_enum
};

extern InstrType instrType(InstrCode code);
extern const char* instrName(InstrCode code);

struct BranchInstr;

#define define_instr_type(t)                                                 \
    static const InstrType Type = InstrType_##t

// Base class for instruction data.
struct Instr : public Cell
{
    Instr(InstrCode code)
      : code_(code)
    {
        assert(code < InstrCodeCount);
    }

    InstrCode code() const { return code_; }

    InstrType type() const { return instrType(code_); }

    template <typename T>
    bool is() const {
        static_assert(is_base_of<Instr, T>::value, "Invalid type parameter");
        return type() == T::Type;
    }

    template <typename T>
    T* as() {
        assert(is<T>());
        return static_cast<T*>(this);
    }

    void print(ostream& s) const override;

    static const unsigned MaxStubCount = 8;
    bool canAddStub() { return stubCount_ < MaxStubCount; }
    void incStubCount() {
        assert(canAddStub());
        stubCount_++;
    }

  private:
    const InstrCode code_;
    uint8_t stubCount_;
};

struct IdentInstrBase : public Instr
{
    IdentInstrBase(InstrCode code, Name ident) : Instr(code), ident(ident)
    {}

    void print(ostream& s) const override;

    // todo: move to Interpreter
    bool raiseAttrError(Traced<Value> value, Interpreter& interp);

    const Name ident;
};

struct IdentInstr : public IdentInstrBase
{
    static const InstrType Type = InstrType_IdentInstr;

    IdentInstr(InstrCode code, Name ident) : IdentInstrBase(code, ident)
    {
        assert(instrType(code) == Type);
    }
};

struct StackSlotInstr : public IdentInstrBase
{
    define_instr_type(StackSlotInstr);

    StackSlotInstr(InstrCode code, Name ident, unsigned slot)
      : IdentInstrBase(code, ident), slot(slot)
    {
        assert(instrType(code) == Type);
    }

    void print(ostream& s) const override;

    // todo: move to Interpreter
    bool raiseAttrError(Traced<Value> value, Interpreter& interp);

    const unsigned slot;
};

struct LexicalFrameInstr : public IdentInstrBase
{
    define_instr_type(LexicalFrameInstr);

    LexicalFrameInstr(InstrCode code, unsigned frameIndex, Name ident)
      : IdentInstrBase(code, ident), frameIndex(frameIndex)
    {
        assert(instrType(code) == Type);
    }

    void print(ostream& s) const override;

    const unsigned frameIndex;
};

struct GlobalNameInstr : public IdentInstrBase
{
    define_instr_type(GlobalNameInstr);

    GlobalNameInstr(InstrCode code, Traced<Object*> global, Name ident,
                    bool fallback = false)
      : IdentInstrBase(code, ident), global_(global), fallback_(fallback)
    {
        assert(instrType(code) == Type);
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

struct GlobalSlotInstr : public IdentInstrBase
{
    define_instr_type(GlobalSlotInstr);

    GlobalSlotInstr(InstrCode code, Traced<Object*> global, Name ident)
      : IdentInstrBase(code, ident), global_(global), globalSlot_(global, ident)
    {
        assert(instrType(code) == Type);
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
      : IdentInstrBase(code, ident),
        global_(global),
        globalSlot_(global, globalIdent)
    {
        assert(global);
    }
};

struct BuiltinsSlotInstr : public GlobalSlotInstr
{
    define_instr_type(BuiltinsSlotInstr);

    BuiltinsSlotInstr(InstrCode code, Traced<Object*> global, Name ident)
      : GlobalSlotInstr(code, global, Name::__builtins__, ident),
        builtinsSlot_(global->getSlot(globalSlot_.slot()).toObject(), ident)
    {
        assert(instrType(code) == Type);
    }

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
    define_instr_type(CountInstr);

    CountInstr(InstrCode code, unsigned count) : Instr(code), count(count) {
        assert(instrType(code) == Type);
    }

    void print(ostream& s) const override;

    const unsigned count;
};

struct IndexInstr : public Instr
{
    define_instr_type(IndexInstr);

    IndexInstr(InstrCode code, unsigned index) : Instr(code), index(index) {
        assert(instrType(code) == Type);
    }

    void print(ostream& s) const override;

    const unsigned index;
};

struct ValueInstr : public Instr
{
    define_instr_type(ValueInstr);

    ValueInstr(InstrCode code, Traced<Value> v)
        : Instr(code), value_(v)
    {
        assert(instrType(code) == Type);
    }

    void print(ostream& s) const override;
    void traceChildren(Tracer& t) override;

    Value value() { return value_; }

  private:
    Heap<Value> value_;
};

struct StubInstr : public Instr
{
    StubInstr(InstrCode code, Traced<Instr*> next)
      : Instr(code), next_(next)
    {
        assert(next_);
    }

    const Heap<Instr*>& next() { return next_; }

    void traceChildren(Tracer& t) override;
    void print(ostream& s) const override;

  private:
    Heap<Instr*> next_;
};

struct BuiltinMethodInstr : public StubInstr
{
    define_instr_type(BuiltinMethodInstr);

    BuiltinMethodInstr(InstrCode code, Traced<Instr*> next,
                       Traced<Class*> cls, Traced<Value> result)
      : StubInstr(code, next),
        class_(cls),
        result_(result)
    {
        assert(instrType(code) == Type);
    }

    void traceChildren(Tracer& t) override;

    Heap<Class*> class_;
    Heap<Value> result_;
};

struct BranchInstr : public Instr
{
    define_instr_type(BranchInstr);

    BranchInstr(InstrCode code, int offset = 0)
      : Instr(code), offset_(offset)
    {
        assert(instrType(code) == Type);
    }

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
    define_instr_type(LambdaInstr);

    LambdaInstr(InstrCode code,
                Name name,
                const vector<Name>& paramNames,
                Traced<Block*> block,
                unsigned defaultCount = 0,
                bool takesRest = false,
                bool isGenerator = false)
      : Instr(Instr_Lambda),
        funcName_(name),
        info_(gc.create<FunctionInfo>(paramNames, block, defaultCount,
                                      takesRest, isGenerator))
    {
        assert(instrType(code) == Type);
    }

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

struct BinaryOpInstr : public Instr
{
    define_instr_type(BinaryOpInstr);

    BinaryOpInstr(InstrCode code, BinaryOp op)
      : Instr(code), op(op)
    {
        assert(instrType(code) == Type);
    }

    void print(ostream& s) const override;

    const BinaryOp op;
};

struct BinaryOpStubInstr : public StubInstr
{
    define_instr_type(BinaryOpStubInstr);

    BinaryOpStubInstr(InstrCode code, Traced<Instr*> next)
      : StubInstr(code, next)
    {
        assert(instrType(code) == Type);
    }
};

struct BuiltinBinaryOpInstr : public StubInstr
{
    define_instr_type(BuiltinBinaryOpInstr);

    BuiltinBinaryOpInstr(InstrCode code, Traced<Instr*> next,
                         Traced<Class*> left, Traced<Class*> right,
                         Traced<Value> method)
      : StubInstr(code, next),
        left_(left),
        right_(right),
        method_(method)
    {
        assert(instrType(code) == Type);
    }

    Class* left() { return left_; }
    Class* right() { return right_; }
    Value method() { return method_; }

    void print(ostream& s) const override;
    void traceChildren(Tracer& t) override;

  private:
    Heap<Class*> left_;
    Heap<Class*> right_;
    Heap<Value> method_;
};

struct CompareOpInstr : public Instr
{
    define_instr_type(CompareOpInstr);

    CompareOpInstr(InstrCode code, CompareOp op)
      : Instr(code), op(op)
    {
        assert(instrType(code) == Type);
    }

    void print(ostream& s) const override;

    const CompareOp op;
};

struct CompareOpStubInstr : public StubInstr
{
    define_instr_type(CompareOpStubInstr);

    CompareOpStubInstr(InstrCode code, Traced<Instr*> next)
      : StubInstr(code, next)
    {
        assert(instrType(code) == Type);
    }

    void print(ostream& s) const override;
};

struct LoopControlJumpInstr : public Instr
{
    define_instr_type(LoopControlJumpInstr);

    LoopControlJumpInstr(InstrCode code, unsigned finallyCount,
                         unsigned target = 0)
      : Instr(Instr_LoopControlJump),
        finallyCount_(finallyCount),
        target_(target)
    {
        assert(instrType(code) == Type);
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

#undef define_instr_type

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
for_each_stub_instr(define_instr_factory)

#undef define_instr_factory

extern Instr* getNextInstr(Instr* instr);
extern Instr* getFinalInstr(Instr* instr);

#endif
