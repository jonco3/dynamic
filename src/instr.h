#ifndef __INSTR_H__
#define __INSTR_H__

#include "value.h"
#include "interp.h"
#include "name.h"
#include "integer.h"
#include "frame.h"
#include "object.h"
#include "callable.h"
#include "utility.h"

#include <ostream>

#define for_each_instr(instr)                                                \
    instr(Dup)                                                               \
    instr(Swap)                                                              \
    instr(ConstInteger)                                                      \
    instr(GetLocal)                                                          \
    instr(SetLocal)                                                          \
    instr(GetProp)                                                           \
    instr(SetProp)                                                           \
    instr(Call)                                                              \
    instr(Return)

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
    virtual void print(std::ostream& os) const = 0;
    virtual bool execute(Interpreter& interp, Frame* frame) = 0;
};

inline std::ostream& operator<<(std::ostream& s, Instr* i) {
    i->print(s);
    return s;
}

#define instr_type(it)                                                       \
    virtual InstrType type() const { return Type; }                          \
    static const InstrType Type = it

struct InstrDup : public Instr
{
    InstrDup() {}

    instr_type(Instr_Dup);

    virtual void print(std::ostream& s) const { s << "Dup"; }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        interp.pushStack(interp.peekStack(0));
        return true;
    }
};

struct InstrSwap : public Instr
{
    InstrSwap() {}

    instr_type(Instr_Swap);

    virtual void print(std::ostream& s) const { s << "Swap"; }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        interp.swapStack();
        return true;
    }
};

struct InstrConstInteger : public Instr
{
    InstrConstInteger(int v) : value(Integer::get(v)) {}

    instr_type(Instr_ConstInteger);

    virtual void print(std::ostream& s) const { s << "ConstInteger " << value; }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        interp.pushStack(value);
        return true;
    }

    Value getValue() { return value; }

  private:
    Value value;
};

struct InstrGetLocal : public Instr
{
    InstrGetLocal(Name name) : name(name) {}

    instr_type(Instr_GetLocal);

    virtual void print(std::ostream& s) const { s << "GetLocal " << name; }

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

    virtual void print(std::ostream& s) const { s << "SetLocal " << name; }

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

    virtual void print(std::ostream& s) const { s << "GetProp " << name; }

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

    virtual void print(std::ostream& s) const { s << "SetProp " << name; }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Value value = interp.popStack();
        interp.popStack().toObject()->setProp(name, value);
        return true;
    }

  private:
    Name name;
};

struct InstrCall : public Instr
{
    InstrCall(unsigned args) : args(args) {}

    instr_type(Instr_Call);

    virtual void print(std::ostream& s) const { s << "Call " << args; }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Object* target = interp.peekStack(args).toObject();
        if (target->is<Native>()) {
            Native* native = target->as<Native>();
            if (native->requiredArgs() < args)
                throw std::runtime_error("Not enough arguments");
            native->call(interp);
            return true;
        } else if (target->is<Function>()) {
            Function* function = target->as<Function>();
            if (function->requiredArgs() < args)
                throw std::runtime_error("Not enough arguments");
            // todo: argument checking etc.

            Frame *callFrame = interp.pushFrame(function);
            for (unsigned i = args - 1; i < args; --i)
                callFrame->setProp(function->argName(i), interp.popStack());
            interp.popStack();

            return true;
        } else {
            throw std::runtime_error("Attempt to call non-callable object: " +
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

    virtual void print(std::ostream& s) const { s << "Return"; }

    virtual bool execute(Interpreter& interp, Frame* frame) {
        Value value = interp.popStack();
        interp.popFrame();
        interp.pushStack(value);
        return true;
    }
};

#endif
