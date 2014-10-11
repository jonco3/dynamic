#ifndef __INSTR_H__
#define __INSTR_H__

#include "bool.h"
#include "callable.h"
#include "frame.h"
#include "integer.h"
#include "interp.h"
#include "name.h"
#include "none.h"
#include "object.h"
#include "repr.h"

#include "value-inl.h"

#include <ostream>

using namespace std;

#define for_each_instr(instr)                                                \
    instr(ConstNone)                                                         \
    instr(ConstInteger)                                                      \
    instr(GetLocal)                                                          \
    instr(SetLocal)                                                          \
    instr(GetLexical)                                                          \
    instr(SetLexical)                                                          \
    instr(GetProp)                                                           \
    instr(SetProp)                                                           \
    instr(GetMethod)                                                         \
    instr(Call)                                                              \
    instr(Return)                                                            \
    instr(In)                                                                \
    instr(Is)                                                                \
    instr(Not)                                                               \
    instr(BranchAlways)                                                      \
    instr(BranchIfTrue)                                                      \
    instr(BranchIfFalse)                                                     \
    instr(Lambda)

enum InstrType
{
#define instr_enum(name) Instr_##name,
    for_each_instr(instr_enum)
#undef instr_enum
    InstrTypeCount
};

struct Branch;

struct Instr
{
    template <typename T> bool is() { return type() == T::Type; }

    template <typename T> T* as() {
        assert(is<T>());
        return static_cast<T*>(this);
    }

    virtual ~Instr() {}
    virtual InstrType type() const = 0;
    virtual string name() const = 0;
    virtual bool execute(Interpreter& interp, Frame* frame) = 0;

    virtual bool isBranch() const { return false; };
    inline Branch* asBranch();

    virtual void print(ostream& s, Instr** loc = nullptr) const {
        s << name();
    }
};

inline ostream& operator<<(ostream& s, const Instr* i) {
    i->print(s);
    return s;
}

#define instr_type(it)                                                       \
    virtual InstrType type() const { return Type; }                          \
    static const InstrType Type = it

#define instr_name(nameStr)                                                    \
    virtual string name() const { return nameStr; }

struct InstrConstInteger : public Instr
{
    InstrConstInteger(int v) : value(Integer::get(v)) {}

    instr_type(Instr_ConstInteger);
    instr_name("ConstInteger");

    virtual void print(ostream& s, Instr** loc) const {
        s << name() << " " << value;
    }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        interp.pushStack(value);
        return true;
    }

    Value getValue() { return value; }

  private:
    Value value;
};

struct InstrConstNone : public Instr
{
    instr_type(Instr_ConstNone);
    instr_name("ConstNone");

    virtual bool execute(Interpreter& interp, Frame* frame) {
        interp.pushStack(None);
        return true;
    }
};

struct InstrGetLocal : public Instr
{
    InstrGetLocal(Name name) : localName(name) {}

    instr_type(Instr_GetLocal);
    instr_name("GetLocal");

    virtual void print(ostream& s, Instr** loc) const {
        s << name() << " " << localName;
    }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        assert(frame->hasProp(localName));
        Value value;
        frame->getProp(localName, value);
        interp.pushStack(value);
        return true;
    }

  private:
    Name localName;
};

struct InstrSetLocal : public Instr
{
    InstrSetLocal(Name name) : localName(name) {}

    instr_type(Instr_SetLocal);
    instr_name("SetLocal");

    virtual void print(ostream& s, Instr** loc) const {
        s << name() << " " << localName;
    }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        frame->setProp(localName, interp.popStack());
        return true;
    }

  private:
    Name localName;
};

struct InstrGetLexical : public Instr
{
    InstrGetLexical(int frame, Name name) : frameIndex(frame), lexicalName(name) {}

    instr_type(Instr_GetLexical);
    instr_name("GetLexical");

    virtual void print(ostream& s, Instr** loc) const {
        s << name() << " " << frameIndex << " " << lexicalName;
    }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Frame* f = frame->ancestor(frameIndex);
        assert(f->hasProp(lexicalName));
        Value value;
        f->getProp(lexicalName, value);
        interp.pushStack(value);
        return true;
    }

  private:
    int frameIndex;
    Name lexicalName;
};

struct InstrSetLexical : public Instr
{
    InstrSetLexical(unsigned frame, Name name) : frameIndex(frame), lexicalName(name) {}

    instr_type(Instr_SetLexical);
    instr_name("SetLexical");

    virtual void print(ostream& s, Instr** loc) const {
        s << name() << " " << frameIndex << " " << lexicalName;
    }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        frame->ancestor(frameIndex)->setProp(lexicalName, interp.popStack());
        return true;
    }

  private:
    unsigned frameIndex;
    Name lexicalName;
};

struct InstrGetProp : public Instr
{
    InstrGetProp(Name attr) : attrName(attr) {}

    instr_type(Instr_GetProp);
    instr_name("GetProp");

    virtual void print(ostream& s, Instr** loc) const {
        s << name() << " " << attrName;
    }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Value value;
        if (!interp.popStack().toObject()->getProp(attrName, value))
            return false;
        interp.pushStack(value);
        return true;
    }

  private:
    Name attrName;
};

struct InstrSetProp : public Instr
{
    InstrSetProp(Name name) : attrName(name) {}

    instr_type(Instr_SetProp);
    instr_name("SetProp");

    virtual void print(ostream& s, Instr** loc) const {
        s << name() << " " << attrName;
    }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Value value = interp.popStack();
        interp.popStack().toObject()->setProp(attrName, value);
        return true;
    }

  private:
    Name attrName;
};

struct InstrGetMethod : public Instr
{
    InstrGetMethod(Name name) : methodName(name) {}

    instr_type(Instr_GetMethod);
    instr_name("GetMethod");

    virtual void print(ostream& s, Instr** loc) const {
        s << name() << " " << methodName;
    }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Object* obj = interp.popStack().toObject();
        Value method;
        if (!obj->getProp(methodName, method))
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

    virtual void print(ostream& s, Instr** loc) const {
        s << name() << " " << args;
    }

    virtual bool execute(Interpreter& interp, Frame* frame) {
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
            Frame *callFrame = interp.pushFrame(function);
            for (int i = args - 1; i >= 0; --i)
                callFrame->setProp(function->paramName(i), interp.popStack());
            interp.popStack();
            frame->setReturn(interp);
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

    virtual bool execute(Interpreter& interp, Frame* frame) {
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

    virtual bool execute(Interpreter& interp, Frame* frame) {
        cerr << "not implemented" << endl;
        return false;
    }
};

struct InstrIs : public Instr
{
    instr_type(Instr_Is);
    instr_name("Is");

    virtual bool execute(Interpreter& interp, Frame* frame) {
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

    virtual bool execute(Interpreter& interp, Frame* frame) {
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

    virtual void print(ostream& s, Instr** loc) const {
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
    instr_type(Instr_BranchAlways);
    instr_name("BranchAlways");

    virtual bool execute(Interpreter& interp, Frame* frame) {
        interp.branch(offset_);
        return true;
    }
};

struct InstrBranchIfTrue : public Branch
{
    instr_type(Instr_BranchIfTrue);
    instr_name("BranchIfTrue");

    // The expression x or y first evaluates x; if x is true, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Object *x = interp.peekStack(0).toObject();
        if (x->isTrue())
            interp.branch(offset_);
        return true;
    }
};

struct InstrBranchIfFalse : public Branch
{
    instr_type(Instr_BranchIfFalse);
    instr_name("BranchIfFalse");

    // The expression x and y first evaluates x; if x is false, its value is
    // returned; otherwise, y is evaluated and the resulting value is returned.

    virtual bool execute(Interpreter& interp, Frame* frame) {
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

    virtual bool execute(Interpreter& interp, Frame* frame) {
        interp.pushStack(new Function(params_, block_.get(), frame));
        return true;
    }

    virtual void print(ostream& s, Instr** loc) const {
        s << name();
        for (auto i = params_.begin(); i != params_.end(); ++i)
            s << " " << *i;
        s << ": { " << block_.get() << " }";
    }

  private:
    vector<Name> params_;
    unique_ptr<Block> block_;
};

#endif
