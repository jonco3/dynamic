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
#include "value.h"

#include <ostream>

using namespace std;

#define for_each_instr(instr)                                                \
    instr(ConstNone)                                                         \
    instr(ConstInteger)                                                      \
    instr(GetLocal)                                                          \
    instr(SetLocal)                                                          \
    instr(GetProp)                                                           \
    instr(SetProp)                                                           \
    instr(GetMethod)                                                         \
    instr(Call)                                                              \
    instr(Return)                                                            \
    instr(In)                                                                \
    instr(Is)                                                                \
    instr(Not)

enum InstrType
{
#define instr_enum(name) Instr_##name,
    for_each_instr(instr_enum)
#undef instr_enum
    InstrTypeCount
};

struct Instr
{
    template <typename T> bool is() { return type() == T::Type; }

    template <typename T> T* as() {
        assert(is<T>());
        return static_cast<T*>(this);
    }

    virtual ~Instr() {}
    virtual InstrType type() const = 0;
    virtual void print(ostream& os) const = 0;
    virtual bool execute(Interpreter& interp, Frame* frame) = 0;
};

inline ostream& operator<<(ostream& s, const Instr* i) {
    i->print(s);
    return s;
}

#define instr_type(it)                                                       \
    virtual InstrType type() const { return Type; }                          \
    static const InstrType Type = it

#define instr_print(name)                                                    \
    virtual void print(ostream& s) const { s << name; }

struct InstrConstInteger : public Instr
{
    InstrConstInteger(int v) : value(Integer::get(v)) {}

    instr_type(Instr_ConstInteger);

    virtual void print(ostream& s) const { s << "ConstInteger " << value; }

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

    virtual void print(ostream& s) const { s << "ConstNone"; }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        interp.pushStack(None);
        return true;
    }
};

struct InstrGetLocal : public Instr
{
    InstrGetLocal(Name name) : name(name) {}

    instr_type(Instr_GetLocal);

    virtual void print(ostream& s) const { s << "GetLocal " << name; }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Value value;
        if (!frame->getProp(name, value))
            return false;
        interp.pushStack(value);
        return true;
    }

  private:
    Name name;
};

struct InstrSetLocal : public Instr
{
    InstrSetLocal(Name name) : name(name) {}

    instr_type(Instr_SetLocal);

    virtual void print(ostream& s) const { s << "SetLocal " << name; }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        frame->setProp(name, interp.popStack());
        return true;
    }

  private:
    Name name;
};

struct InstrGetProp : public Instr
{
    InstrGetProp(Name name) : name(name) {}

    instr_type(Instr_GetProp);

    virtual void print(ostream& s) const { s << "GetProp " << name; }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Value value;
        if (!interp.popStack().toObject()->getProp(name, value))
            return false;
        interp.pushStack(value);
        return true;
    }

  private:
    Name name;
};

struct InstrSetProp : public Instr
{
    InstrSetProp(Name name) : name(name) {}

    instr_type(Instr_SetProp);

    virtual void print(ostream& s) const { s << "SetProp " << name; }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Value value = interp.popStack();
        interp.popStack().toObject()->setProp(name, value);
        return true;
    }

  private:
    Name name;
};

struct InstrGetMethod : public Instr
{
    InstrGetMethod(Name name) : name(name) {}

    instr_type(Instr_GetMethod);

    virtual void print(ostream& s) const { s << "GetMethod " << name; }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Object* obj = interp.popStack().toObject();
        Value method;
        if (!obj->getProp(name, method))
            return false;
        interp.pushStack(method);
        interp.pushStack(obj);
        return true;
    }

  private:
    Name name;
};

struct InstrCall : public Instr
{
    InstrCall(unsigned args) : args(args) {}

    instr_type(Instr_Call);

    virtual void print(ostream& s) const { s << "Call " << args; }

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
            // todo: argument checking etc.

            Frame *callFrame = interp.pushFrame(function);
            for (unsigned i = args - 1; i < args; --i)
                callFrame->setProp(function->argName(i), interp.popStack());
            interp.popStack();

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

    virtual void print(ostream& s) const { s << "Return"; }

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
    instr_print("In");

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
    instr_print("Is");

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
    instr_print("Not");

    // the following values are interpreted as false: False, None, numeric zero
    // of all types, and empty strings and containers (including strings,
    // tuples, lists, dictionaries, sets and frozensets).

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Object* obj = interp.popStack().toObject();
        bool result =
            obj != None &&
            // obj != False &&
            obj != Integer::Zero;
        interp.pushStack(Boolean::get(result));
        return true;
    }
};

#endif
