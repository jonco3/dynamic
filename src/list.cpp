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

    static bool list_setitem(Traced<Value> arg1, Traced<Value> arg2, Traced<Value> arg3,
                              Root<Value>& resultOut) {
        List* list = arg1.toObject()->as<List>();
        return list->setitem(arg2, arg3, resultOut);
    }

    void initNatives() {
        Root<Value> value;
        value = new Native2(list_getitem); setAttr("__getitem__", value);
        value = new Native3(list_setitem); setAttr("__setitem__", value);
    }
};

ListBase::ListBase(Traced<Class*> cls, const TracedVector<Value>& values)
  : Object(cls), elements_(values.size())
{
    for (unsigned i = 0; i < values.size(); ++i)
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
    RootVector<Value> contents;
    Empty.init(new Tuple(contents));
}

/* static */ Tuple* Tuple::get(const TracedVector<Value>& values)
{
    if (values.size() == 0)
        return Empty;
    return new Tuple(values);
}

Tuple::Tuple(const TracedVector<Value>& values)
  : ListBase(ObjectClass, values) {}

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

List::List(const TracedVector<Value>& values)
  : ListBase(ObjectClass, values)
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

bool List::setitem(Traced<Value> index, Traced<Value> value, Root<Value>& resultOut)
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

    elements_[i] = value;
    resultOut = value;
    return true;
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
    // todo: should be TypeError: 'tuple' object does not support item assignment
    testException("(1,)[0] = 2", "'tuple' object has no attribute '__setitem__'");
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
    testInterp("[1][0] = 2", "2");
    testInterp("a = [1]; a[0] = 3; a", "[3]");

    testException("1[0]", "'int' object has no attribute '__getitem__'");
    testException("[][0]", "list index out of range");
    testException("[1][1]", "list index out of range");
}

#endif
