#include "list.h"

#include "bool.h"
#include "callable.h"
#include "exception.h"
#include "singletons.h"

#include <algorithm>

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

void initList()
{
    List::init();
    Tuple::init();
    ListIter::init();
}

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

static bool listBase_len(TracedVector<Value> args, Root<Value>& resultOut)
{
    ListBase* l = asListBase(args[0].toObject());
    return l->len(resultOut);
}

static bool listBase_getitem(TracedVector<Value> args, Root<Value>& resultOut)
{
    ListBase* l = asListBase(args[0].toObject());
    return l->getitem(args[1], resultOut);
}

static bool listBase_contains(TracedVector<Value> args, Root<Value>& resultOut)
{
    ListBase* l = asListBase(args[0].toObject());
    return l->contains(args[1], resultOut);
}

static bool listBase_iter(TracedVector<Value> args, Root<Value>& resultOut)
{
    ListBase* l = asListBase(args[0].toObject());
    return l->iter(resultOut);
}

static void listBase_initNatives(Traced<Class*> cls)
{
    Root<Value> value;
    initNativeMethod(cls, "__len__", listBase_len, 1);
    initNativeMethod(cls, "__getitem__", listBase_getitem, 2);
    initNativeMethod(cls, "__contains__", listBase_contains, 2);
    initNativeMethod(cls, "__iter__", listBase_iter, 1);
}

static bool list_setitem(TracedVector<Value> args, Root<Value>& resultOut) {
    List* list = args[0].toObject()->as<List>();
    return list->setitem(args[1], args[2], resultOut);
}

static bool list_append(TracedVector<Value> args, Root<Value>& resultOut) {
    List* list = args[0].toObject()->as<List>();
    return list->append(args[1], resultOut);
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
        resultOut = gc::create<Exception>("TypeError",
                                  listName() + " indices must be integers");
        return false;
    }

    int32_t i = index.asInt32();
    if (i < 0)
        i = elements_.size() + i;
    if (i < 0 || size_t(i) >= elements_.size()) {
        resultOut = gc::create<Exception>("IndexError", listName() + " index out of range");
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
    resultOut = gc::create<ListIter>(self);
    return true;
}

void Tuple::init()
{
    Root<Class*> cls(gc::create<Class>("tuple"));
    listBase_initNatives(cls);
    ObjectClass.init(cls);
    RootVector<Value> contents;
    Empty.init(gc::create<Tuple>(contents));
}

/* static */ Tuple* Tuple::get(const TracedVector<Value>& values)
{
    if (values.size() == 0)
        return Empty;
    return gc::create<Tuple>(values);
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
    ObjectClass.init(gc::create<Class>("list"));
    listBase_initNatives(ObjectClass);
    initNativeMethod(ObjectClass, "__setitem__", list_setitem, 3);
    initNativeMethod(ObjectClass, "append", list_append, 2);
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
        resultOut = gc::create<Exception>("TypeError",
                                  listName() + " indices must be integers");
        return false;
    }

    int32_t i = index.asInt32();
    if (i < 0)
        i = elements_.size() + i;
    if (i < 0 || size_t(i) >= elements_.size()) {
        resultOut = gc::create<Exception>("IndexError", listName() + " index out of range");
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

static bool listIter_iter(TracedVector<Value> args, Root<Value>& resultOut)
{
    ListIter* i = args[0].toObject()->as<ListIter>();
    return i->iter(resultOut);
}

static bool listIter_next(TracedVector<Value> args, Root<Value>& resultOut)
{
    ListIter* i = args[0].toObject()->as<ListIter>();
    return i->next(resultOut);
}

void ListIter::init()
{
    ObjectClass.init(gc::create<Class>("listiterator"));
    initNativeMethod(ObjectClass, "__iter__", listIter_iter, 1);
    initNativeMethod(ObjectClass, "next", listIter_next, 1);
}

ListIter::ListIter(Traced<ListBase*> list)
  : Object(ObjectClass), list_(list), index_(0)
{
}

void ListIter::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
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
    assert(index_ >= -1);
    if (index_ == -1 || size_t(index_) >= list_->elements_.size()) {
        index_ = -1;
        resultOut = gc::create<StopIteration>();
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
