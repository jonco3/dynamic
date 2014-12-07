#include "tuple.h"

#include "callable.h"
#include "exception.h"

GlobalRoot<Class*> Tuple::ObjectClass;
GlobalRoot<Tuple*> Tuple::Empty;

struct TupleClass : public Class
{
    TupleClass() : Class("tuple") {}

    static bool tuple_getitem(Traced<Value> arg1, Traced<Value> arg2,
                              Root<Value>& resultOut) {
        Tuple* tuple = arg1.toObject()->as<Tuple>();
        if (!arg2.isInt32()) {
            resultOut = new Exception("Type error on index");
            return false;
        }
        int32_t index = arg2.asInt32();
        return tuple->getitem(index, resultOut);
    }

    void initNatives() {
        Root<Value> value(new Native2(tuple_getitem));
        setAttr("__getitem__", value);
    }
};

void Tuple::init()
{
    Root<TupleClass*> cls(new TupleClass);
    cls->initNatives();
    ObjectClass.init(cls);
    Empty.init(new Tuple(0, nullptr));
}

/* static */ Tuple* Tuple::get(unsigned size, const Value* values)
{
    if (size == 0)
        return Empty;
    return new Tuple(size, values);
}

Tuple::Tuple(unsigned size, const Value* values)
  : Object(ObjectClass), elements_(size)
{
    for (unsigned i = 0; i < size; ++i)
        elements_[i] = values[i];
}

bool Tuple::getitem(int32_t index, Root<Value>& resultOut)
{
    if (index < 0)
        index = elements_.size() + index;
    if (index < 0 || index >= elements_.size()) {
        resultOut = new Exception("tuple index out of range");
        return false;
    }

    resultOut = elements_[index];
    return true;
}

void Tuple::traceChildren(Tracer& t)
{
    for (unsigned i = 0; i < elements_.size(); ++i)
        gc::trace(t, &elements_[i]);
}

void Tuple::print(ostream& s) const
{
    s << "(";
    for (unsigned i = 0; i < elements_.size(); ++i) {
        if (i != 0)
            s << ", ";
        s << elements_[i];
    }
    if (elements_.size() == 1)
            s << ",";
    s << ")";
}

#ifdef BUILD_TESTS

#include "test.h"
#include "interp.h"

testcase(tuple)
{
    testInterp("()", "()");
    testInterp("(1,2)", "(1, 2)");
    testInterp("(1,2,)", "(1, 2)");
    testInterp("(1,)", "(1,)");
    testInterp("(1 + 2, 3 + 4)", "(3, 7)");
    testInterp("(1,)[0]", "1");
    testInterp("(1,2)[1]", "2");
    testInterp("(1,2)[-1]", "2");

    testException("1[0]", "'int' object has no attribute '__getitem__'");
    testException("()[0]", "tuple index out of range");
    testException("(1,)[1]", "tuple index out of range");
}

#endif
