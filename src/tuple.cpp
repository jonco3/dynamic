#include "tuple.h"

GlobalRoot<Class*> Tuple::ObjectClass;
GlobalRoot<Tuple*> Tuple::Empty;

struct TupleClass : public Class
{
    TupleClass() : Class("tuple") {}

    void initNatives() {}
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
}

#endif
