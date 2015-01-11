#include "list.h"

#include "bool.h"
#include "callable.h"
#include "exception.h"
#include "singletons.h"

struct ListIter : public Object
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    ListIter(Traced<ListBase*> list);

    virtual void traceChildren(Tracer& t) override;

    bool iter(Root<Value>& resultOut);
    bool next(Root<Value>& resultOut);

  private:
    ListBase* list_;
    int index_;
};

GlobalRoot<Class*> Tuple::ObjectClass;
GlobalRoot<Tuple*> Tuple::Empty;
GlobalRoot<Class*> List::ObjectClass;
GlobalRoot<Class*> ListIter::ObjectClass;

#ifdef DEBUG
static bool isListBase(Object *o)
{
    return o->is<Tuple>() || o->is<List>();
}
#endif

static ListBase* asListBase(Object *o)
{
    assert(isListBase(o));
    return static_cast<ListBase*>(o);
}

static bool listBase_len(Traced<Value> arg, Root<Value>& resultOut)
{
    ListBase* l = asListBase(arg.toObject());
    return l->len(resultOut);
}

static bool listBase_getitem(Traced<Value> arg1, Traced<Value> arg2,
                             Root<Value>& resultOut)
{
    ListBase* l = asListBase(arg1.toObject());
    return l->getitem(arg2, resultOut);
}

static bool listBase_contains(Traced<Value> arg1, Traced<Value> arg2,
                              Root<Value>& resultOut)
{
    ListBase* l = asListBase(arg1.toObject());
    return l->contains(arg2, resultOut);
}

static bool listBase_iter(Traced<Value> arg, Root<Value>& resultOut)
{
    ListBase* l = asListBase(arg.toObject());
    return l->iter(resultOut);
}

static void listBase_initNatives(Traced<Class*> cls)
{
    Root<Value> value;
    value = new Native1(listBase_len); cls->setAttr("__len__", value);
    value = new Native2(listBase_getitem); cls->setAttr("__getitem__", value);
    value = new Native2(listBase_contains); cls->setAttr("__contains__", value);
    value = new Native1(listBase_iter); cls->setAttr("__iter__", value);
}

static bool list_setitem(Traced<Value> arg1, Traced<Value> arg2, Traced<Value> arg3,
                         Root<Value>& resultOut) {
    List* list = arg1.toObject()->as<List>();
    return list->setitem(arg2, arg3, resultOut);
}

static bool list_append(Traced<Value> arg1, Traced<Value> arg2, Root<Value>& resultOut) {
    List* list = arg1.toObject()->as<List>();
    return list->append(arg2, resultOut);
}

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

bool ListBase::len(Root<Value>& resultOut)
{
    resultOut = Integer::get(elements_.size());
    return true;
}

bool ListBase::getitem(Traced<Value> index, Root<Value>& resultOut)
{
    if (!index.isInt32()) {
        resultOut = new Exception("TypeError",
                                  listName() + " indices must be integers");
        return false;
    }

    int32_t i = index.asInt32();
    if (i < 0)
        i = elements_.size() + i;
    if (i < 0 || i >= elements_.size()) {
        resultOut = new Exception("IndexError", listName() + " index out of range");
        return false;
    }

    resultOut = elements_[i];
    return true;
}

bool ListBase::contains(Traced<Value> element, Root<Value>& resultOut)
{
    bool found = find(elements_.begin(), elements_.end(), element) != elements_.end();
    resultOut = Boolean::get(found);
    return true;
}

bool ListBase::iter(Root<Value>& resultOut)
{
    Root<ListBase*> self(this);
    resultOut = new ListIter(self);
    return true;
}

void Tuple::init()
{
    Root<Class*> cls(new Class("tuple"));
    listBase_initNatives(cls);
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
    Root<Class*> cls(new Class("list"));
    listBase_initNatives(cls);
    Root<Value> value;
    value = new Native3(list_setitem); cls->setAttr("__setitem__", value);
    value = new Native2(list_append); cls->setAttr("append", value);
    ObjectClass.init(cls);

    ListIter::init();  // todo: have a single static init function per file
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
        resultOut = new Exception("TypeError",
                                  listName() + " indices must be integers");
        return false;
    }

    int32_t i = index.asInt32();
    if (i < 0)
        i = elements_.size() + i;
    if (i < 0 || i >= elements_.size()) {
        resultOut = new Exception("IndexError", listName() + " index out of range");
        return false;
    }

    elements_[i] = value;
    resultOut = value;
    return true;
}

bool List::append(Traced<Value> element, Root<Value>& resultOut)
{
    elements_.push_back(element);
    resultOut = None;
    return true;
}

static bool listIter_iter(Traced<Value> arg, Root<Value>& resultOut)
{
    ListIter* i = arg.toObject()->as<ListIter>();
    return i->iter(resultOut);
}

static bool listIter_next(Traced<Value> arg, Root<Value>& resultOut)
{
    ListIter* i = arg.toObject()->as<ListIter>();
    return i->next(resultOut);
}

void ListIter::init()
{
    Root<Class*> cls(new Class("listiterator"));
    Root<Value> value;
    value = new Native1(listIter_iter); cls->setAttr("__iter__", value);
    value = new Native1(listIter_next); cls->setAttr("next", value);
    ObjectClass.init(cls);
}

ListIter::ListIter(Traced<ListBase*> list)
  : Object(ObjectClass), list_(list), index_(0)
{
}

void ListIter::traceChildren(Tracer& t)
{
    gc::trace(t, &list_);
}

bool ListIter::iter(Root<Value>& resultOut)
{
    Root<ListIter*> self(this);
    resultOut = self;
    return true;
}

bool ListIter::next(Root<Value>& resultOut)
{
    if (index_ == -1 || index_ >= list_->elements_.size()) {
        index_ = -1;
        resultOut = new StopIteration;
        return false;
    }

    resultOut = list_->elements_[index_];
    index_++;
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
    testInterp("2 in (1,)", "False");
    testInterp("2 in (1, 2, 3)", "True");

    testException("1[0]", "'int' object has no attribute '__getitem__'");
    testException("()[0]", "tuple index out of range");
    testException("(1,)[1]", "tuple index out of range");
    // todo: should be TypeError: 'tuple' object does not support item assignment
    testException("(1,)[0] = 2", "'tuple' object has no attribute '__setitem__'");
    // todo: should be TypeError: argument of type 'int' is not iterable
    testException("(1,) in 1", "TypeError: Argument is not iterable");
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
    testInterp("2 in [1]", "False");
    testInterp("2 in [1, 2, 3]", "True");
    testInterp("[1].append(2)", "None");
    testInterp("a = [1]; a.append(2); a", "[1, 2]");

    testException("1[0]", "'int' object has no attribute '__getitem__'");
    testException("[][0]", "list index out of range");
    testException("[1][1]", "list index out of range");
}

#endif
