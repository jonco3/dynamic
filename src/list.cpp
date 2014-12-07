#include "list.h"

#include "callable.h"
#include "exception.h"

GlobalRoot<Class*> Tuple::ObjectClass;
GlobalRoot<Tuple*> Tuple::Empty;
GlobalRoot<Class*> List::ObjectClass;

struct TupleClass : public Class
{
    TupleClass() : Class("tuple") {}

    static bool tuple_getitem(Traced<Value> arg1, Traced<Value> arg2,
                              Root<Value>& resultOut) {
        Tuple* tuple = arg1.toObject()->as<Tuple>();
        return tuple->getitem(arg2, resultOut);
    }

    void initNatives() {
        Root<Value> value(new Native2(tuple_getitem));
        setAttr("__getitem__", value);
    }
};

struct ListClass : public Class
{
    ListClass() : Class("list") {}

    static bool list_getitem(Traced<Value> arg1, Traced<Value> arg2,
                              Root<Value>& resultOut) {
        List* list = arg1.toObject()->as<List>();
        return list->getitem(arg2, resultOut);
    }

    void initNatives() {
        Root<Value> value(new Native2(list_getitem));
        setAttr("__getitem__", value);
    }
};

ListBase::ListBase(Traced<Class*> cls, unsigned size, const Value* values)
  : Object(cls), elements_(size)
{
    // todo: use vector constructor for this
    for (unsigned i = 0; i < size; ++i)
        elements_[i] = values[i];
}

void ListBase::traceChildren(Tracer& t)
{
    for (unsigned i = 0; i < elements_.size(); ++i)
        gc::trace(t, &elements_[i]);
}

bool ListBase::getitem(Traced<Value> index, Root<Value>& resultOut)
{
    if (!index.isInt32()) {
        resultOut = new Exception("Type error on index");
        return false;
    }

    int32_t i = index.asInt32();
    if (i < 0)
        i = elements_.size() + i;
    if (i < 0 || i >= elements_.size()) {
        resultOut = new Exception(listName() + " index out of range");
        return false;
    }

    resultOut = elements_[i];
    return true;
}

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
  : ListBase(ObjectClass, size, values) {}

const string& Tuple::listName() const
{
    static string name("tuple");
    return name;
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

void List::init()
{
    Root<ListClass*> cls(new ListClass);
    cls->initNatives();
    ObjectClass.init(cls);
}

List::List(unsigned size, const Value* values)
  : ListBase(ObjectClass, size, values)
{}

const string& List::listName() const
{
    static string name("list");
    return name;
}

void List::print(ostream& s) const
{
    s << "[";
    for (unsigned i = 0; i < elements_.size(); ++i) {
        if (i != 0)
            s << ", ";
        s << elements_[i];
    }
    s << "]";
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

testcase(list)
{
    testInterp("[]", "[]");
    testInterp("[1,2]", "[1, 2]");
    testInterp("[1,2,]", "[1, 2]");
    testInterp("[1,]", "[1]");
    testInterp("[1 + 2, 3 + 4]", "[3, 7]");
    testInterp("[1][0]", "1");
    testInterp("[1,2][1]", "2");
    testInterp("[1,2][-1]", "2");

    testException("1[0]", "'int' object has no attribute '__getitem__'");
    testException("[][0]", "list index out of range");
    testException("[1][1]", "list index out of range");
}

#endif
