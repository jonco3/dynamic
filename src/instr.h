#ifndef __INSTR_H__
#define __INSTR_H__

#include "callable.h"
#include "gcdefs.h"
#include "name.h"

#include "value-inl.h"

#include <iostream>
#include <ostream>

using namespace std;

#define for_each_inline_instr(instr)                                         \
    instr(Abort)                                                             \
    instr(Return)

#define for_each_outofline_instr(instr)                                      \
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
    instr(GetMethodFallback)                                                 \
    instr(GetMethodBuiltin)                                                  \
    instr(Call)                                                              \
    instr(CallMethod)                                                        \
    instr(CreateEnv)                                                         \
    instr(InitStackLocals)                                                   \
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
    instr(Raise)                                                             \
    instr(GetIterator)                                                       \
    instr(IteratorNext)                                                      \
    instr(BinaryOp)                                                          \
    instr(BinaryOpFallback)                                                  \
    instr(BinaryOpInt_Add)                                                  \
    instr(BinaryOpInt_Minus)                                                 \
    instr(BinaryOpInt_Multiply)                                              \
    instr(BinaryOpInt_TrueDiv)                                               \
    instr(BinaryOpInt_FloorDiv)                                              \
    instr(BinaryOpInt_Modulo)                                                \
    instr(BinaryOpInt_Power)                                                 \
    instr(BinaryOpInt_Or)                                                    \
    instr(BinaryOpInt_Xor)                                                   \
    instr(BinaryOpInt_And)                                                   \
    instr(BinaryOpInt_LeftShift)                                             \
    instr(BinaryOpInt_RightShift)                                            \
    instr(BinaryOpBuiltin)                                                   \
    instr(CompareOp)                                                         \
    instr(CompareOpFallback)                                                 \
    instr(CompareOpInt_LT)                                                   \
    instr(CompareOpInt_LE)                                                   \
    instr(CompareOpInt_GT)                                                   \
    instr(CompareOpInt_GE)                                                   \
    instr(CompareOpInt_EQ)                                                   \
    instr(CompareOpInt_NE)                                                   \
    instr(AugAssignUpdate)                                                   \
    instr(AugAssignUpdateFallback)                                           \
    instr(AugAssignUpdateBuiltin)                                            \
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
    for_each_inline_instr(instr_enum)
    for_each_outofline_instr(instr_enum)
#undef instr_enum
    InstrTypeCount
};

extern const char* instrName(InstrType type);

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
    static const InstrType Type = it;                                         \
    virtual InstrType type() const { return Type; }

#define instr_name(nameStr)                                                   \
    virtual string name() const { return nameStr; }

#define define_instr_members(it)                                              \
    instr_type(Instr_##it);                                                   \
    instr_name(#it)

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

struct InstrBinaryOp : public BinaryOpInstr
{
    define_instr_members(BinaryOp);
    InstrBinaryOp(BinaryOp op) : BinaryOpInstr(op) {}
    Instr* specializeForInt();
};

struct InstrBinaryOpFallback : public BinaryOpInstr
{
    define_instr_members(BinaryOpFallback);
    InstrBinaryOpFallback(BinaryOp op) : BinaryOpInstr(op) {}
};

template <BinaryOp Op>
struct InstrBinaryOpInt : public BinaryOpInstr
{
    instr_type(static_cast<InstrType>(Instr_BinaryOpInt_Add + Op));
    instr_name("BinaryOpInt");
    InstrBinaryOpInt() : BinaryOpInstr(Op) {}
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
    Class* left_;
    Class* right_;
    Value method_;
};

#define typedef_binary_op_int(name, token, method, rmethod, imethod)          \
    typedef InstrBinaryOpInt<Binary##name> InstrBinaryOpInt_##name;
    for_each_binary_op(typedef_binary_op_int)
#undef typedef_binary_op_int

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
    Instr* specializeForInt();
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
    instr_name("CompareOpInt");
    InstrCompareOpInt() : CompareOpInstr(Op) {}
};

#define typedef_compare_op_int(name, token, method, rmethod)                  \
    typedef InstrCompareOpInt<Compare##name> InstrCompareOpInt_##name;
    for_each_compare_op(typedef_compare_op_int)
#undef typedef_compare_op_int

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
#undef instr_name
#undef define_instr_members
#undef define_simple_instr
#undef define_branch_instr
#undef define_ident_instr

#endif
