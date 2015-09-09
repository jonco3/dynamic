#include "list.h"

#include "bool.h"
#include "builtin.h"
#include "callable.h"
#include "exception.h"
#include "interp.h"
#include "numeric.h"
#include "singletons.h"

#include "value-inl.h"

#include <algorithm>

struct ListIter : public Object
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    ListIter(Traced<ListBase*> list);

    void traceChildren(Tracer& t) override;

    bool iter(MutableTraced<Value> resultOut);
    bool next(MutableTraced<Value> resultOut);

  private:
    ListBase* list_;
    int index_;
};

GlobalRoot<Class*> Tuple::ObjectClass;
GlobalRoot<Tuple*> Tuple::Empty;
GlobalRoot<Class*> List::ObjectClass;
GlobalRoot<Class*> ListIter::ObjectClass;
GlobalRoot<Class*> Slice::ObjectClass;
GlobalRoot<Layout*> Slice::InitialLayout;

void initList()
{
    List::init();
    Tuple::init();
    ListIter::init();
    Slice::init();
}

#ifdef DEBUG
static bool isListBase(Object *o)
{
    return o->isInstanceOf(Tuple::ObjectClass) ||
        o->isInstanceOf(List::ObjectClass);
}
#endif

static ListBase* asListBase(Object *o)
{
    assert(isListBase(o));
    return static_cast<ListBase*>(o);
}

template <class T>
static bool listBase_new(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;

    Stack<ListBase*> init;
    Stack<Class*> cls(args[0].asObject()->as<Class>());
    assert(cls->isDerivedFrom(List::ObjectClass) ||
           cls->isDerivedFrom(Tuple::ObjectClass));

    if (args.size() == 1) {
        resultOut = gc.create<T>(cls, init);
        return true;
    }

    Stack<Object*> arg(args[1].toObject());
    if (arg->isInstanceOf(List::ObjectClass)) {
        init = arg->as<List>();
        resultOut = gc.create<T>(cls, init);
        return true;
    }

    if (arg->isInstanceOf(Tuple::ObjectClass)) {
        init = arg->as<Tuple>();
        resultOut = gc.create<T>(cls, init);
        return true;
    }

    Stack<Value> result;
    interp.pushStack(arg);
    if (!interp.call(IterableToList, 1, result)) {
        resultOut = result;
        return false;
    }

    if (cls->isDerivedFrom(List::ObjectClass)) {
        resultOut = result;
        return true;
    }

    init = result.as<List>();
    resultOut = gc.create<T>(cls, init);
    return true;
}

static bool listBase_len(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    ListBase* l = asListBase(args[0].toObject());
    return l->len(resultOut);
}

static bool listBase_getitem(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    ListBase* l = asListBase(args[0].toObject());
    return l->getitem(args[1], resultOut);
}

static bool listBase_contains(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    ListBase* l = asListBase(args[0].toObject());
    return l->contains(args[1], resultOut);
}

static bool listBase_iter(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    ListBase* l = asListBase(args[0].toObject());
    return l->iter(resultOut);
}

static void listBase_initNatives(Traced<Class*> cls)
{
    Stack<Value> value;
    initNativeMethod(cls, "__len__", listBase_len, 1);
    initNativeMethod(cls, "__getitem__", listBase_getitem, 2);
    initNativeMethod(cls, "__contains__", listBase_contains, 2);
    initNativeMethod(cls, "__iter__", listBase_iter, 1);
    // __eq__ and __ne__ are supplied by lib/internal.py
}

ListBase::ListBase(Traced<Class*> cls, const TracedVector<Value>& values)
  : Object(cls), elements_(values.size())
{
    for (unsigned i = 0; i < values.size(); ++i)
        elements_[i] = values[i];
}

ListBase::ListBase(Traced<Class*> cls, size_t size)
  : Object(cls), elements_(size)
{
#ifdef DEBUG
    for (unsigned i = 0; i < size; ++i)
        elements_[i] = None;
#endif
}

void ListBase::traceChildren(Tracer& t)
{
    for (unsigned i = 0; i < elements_.size(); ++i)
        gc.trace(t, &elements_[i]);
}

bool ListBase::len(MutableTraced<Value> resultOut)
{
    resultOut = Integer::get(elements_.size());
    return true;
}

bool ListBase::checkIndex(int32_t index, MutableTraced<Value> resultOut)
{
    if (index < 0 || size_t(index) >= elements_.size()) {
        resultOut = gc.create<IndexError>(listName() + " index out of range");
        return false;
    }
    return true;
}

static int32_t WrapIndex(int32_t index, int32_t length)
{
    return min(index >= 0 ? index : length + index, length);
}

static int32_t ClampIndex(int32_t index, int32_t length, bool inclusive)
{
    return max(min(index, length - (inclusive ? 1 : 0)), 0);
}

int32_t Slice::getSlotOrDefault(unsigned slot, int32_t def)
{
    Value value = getSlot(slot);
    if (value == Value(None))
        return def;
    return value.toInt();
}

void Slice::indices(int32_t length, int32_t& start, int32_t& stop, int32_t& step)
{
    // todo: should be a python method
    start = getSlotOrDefault(StartSlot, 0);
    stop = getSlotOrDefault(StopSlot, length);
    step = getSlotOrDefault(StepSlot, 1);
    if (step == 0)
        return;

    start = ClampIndex(WrapIndex(start, length), length, true);
    stop = ClampIndex(WrapIndex(stop, length), length, false);
}

bool ListBase::getitem(Traced<Value> index, MutableTraced<Value> resultOut)
{
    if (index.isInt()) {
        int32_t i = WrapIndex(index.toInt(), len());
        if (!checkIndex(i, resultOut))
            return false;
        resultOut = elements_[i];
        return true;
    } else if (index.isInstanceOf(Slice::ObjectClass)) {
        Stack<Slice*> slice(index.as<Slice>());
        int32_t start, stop, step;
        slice->indices(len(), start, stop, step);
        if (step == 0) {
            resultOut = gc.create<ValueError>("slice step cannot be zero");
            return false;
        }

        int32_t step_sgn = step > 0 ? 1 : -1;
        size_t size = max((stop - start + step - step_sgn) / step, 0);

        Stack<ListBase*> result(createDerived(size));
        int32_t src = start;
        for (size_t i = 0; i < size; i++) {
            assert(src < len());
            result->elements_[i] = elements_[src];
            src += step;
        }

        resultOut = Value(result);
        return true;
    } else {
        resultOut = gc.create<TypeError>(
            listName() + " indices must be integers");
        return false;
    }
}

bool ListBase::contains(Traced<Value> element, MutableTraced<Value> resultOut)
{
    bool found = find(elements_.begin(), elements_.end(), element) != elements_.end();
    resultOut = Boolean::get(found);
    return true;
}

bool ListBase::iter(MutableTraced<Value> resultOut)
{
    Stack<ListBase*> self(this);
    resultOut = gc.create<ListIter>(self);
    return true;
}

bool ListBase::operator==(const ListBase& other)
{
    return false;
}

void Tuple::init()
{
    ObjectClass.init(Class::createNative("tuple", listBase_new<Tuple>, 2));
    listBase_initNatives(ObjectClass);
    Empty.init(gc.create<Tuple>(EmptyValueArray));
}

/* static */ Tuple* Tuple::get(const TracedVector<Value>& values)
{
    if (values.size() == 0)
        return Empty;
    return gc.create<Tuple>(values);
}

/* static */ Tuple* Tuple::createUninitialised(size_t size)
{
    return gc.create<Tuple>(size);
}

Tuple::Tuple(const TracedVector<Value>& values)
  : ListBase(ObjectClass, values) {}

Tuple::Tuple(size_t size)
  : ListBase(ObjectClass, size) {}

Tuple::Tuple(Traced<Class*> cls, Traced<ListBase*> init)
  : ListBase(cls, 0)
{
    assert(cls->isDerivedFrom(ObjectClass));
    if (init)
        elements_ = init->elements();
}

void Tuple::initElement(size_t index, const Value& value)
{
    assert(elements_[index].asObject() == None);
    elements_[index] = value;
}

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

static bool list_setitem(TracedVector<Value> args, MutableTraced<Value> resultOut) {
    List* list = args[0].as<List>();
    return list->setitem(args[1], args[2], resultOut);
}

static bool list_delitem(TracedVector<Value> args, MutableTraced<Value> resultOut) {
    List* list = args[0].as<List>();
    return list->delitem(args[1], resultOut);
}

static bool list_append(TracedVector<Value> args, MutableTraced<Value> resultOut) {
    List* list = args[0].as<List>();
    return list->append(args[1], resultOut);
}

void List::init()
{
    ObjectClass.init(Class::createNative("list", listBase_new<List>, 2));
    listBase_initNatives(ObjectClass);
    initNativeMethod(ObjectClass, "__setitem__", list_setitem, 3);
    initNativeMethod(ObjectClass, "__delitem__", list_delitem, 2);
    initNativeMethod(ObjectClass, "append", list_append, 2);
}

List::List(const TracedVector<Value>& values)
  : ListBase(ObjectClass, values)
{}

List::List(size_t size)
  : ListBase(ObjectClass, size)
{}

List::List(Traced<Class*> cls, Traced<ListBase*> init)
  : ListBase(cls, 0)
{
    assert(cls->isDerivedFrom(ObjectClass));
    if (init)
        elements_ = init->elements();
}

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

bool List::setitem(Traced<Value> index, Traced<Value> value, MutableTraced<Value> resultOut)
{
    if (!index.isInt()) {
        resultOut = gc.create<TypeError>(
            listName() + " indices must be integers");
        return false;
    }

    int32_t i = index.toInt();
    if (i < 0)
        i = elements_.size() + i;
    if (i < 0 || size_t(i) >= elements_.size()) {
        resultOut = gc.create<IndexError>(
            listName() + " index out of range");
        return false;
    }

    elements_[i] = value;
    resultOut = value;
    return true;
}

bool List::delitem(Traced<Value> index, MutableTraced<Value> resultOut)
{
    if (!index.isInt()) {
        resultOut = gc.create<TypeError>(
            listName() + " indices must be integers");
        return false;
    }

    int32_t i = index.toInt();
    if (i < 0)
        i = elements_.size() + i;
    if (i < 0 || size_t(i) >= elements_.size()) {
        resultOut = gc.create<IndexError>(
            listName() + " index out of range");
        return false;
    }

    elements_.erase(elements_.begin() + i);
    resultOut = None;
    return true;
}

bool List::append(Traced<Value> element, MutableTraced<Value> resultOut)
{
    elements_.push_back(element);
    resultOut = None;
    return true;
}

static bool listIter_iter(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    ListIter* i = args[0].as<ListIter>();
    return i->iter(resultOut);
}

static bool listIter_next(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    ListIter* i = args[0].as<ListIter>();
    return i->next(resultOut);
}

void ListIter::init()
{
    ObjectClass.init(Class::createNative("listiterator", nullptr));
    initNativeMethod(ObjectClass, Name::__iter__, listIter_iter, 1);
    initNativeMethod(ObjectClass, Name::__next__, listIter_next, 1);
}

ListIter::ListIter(Traced<ListBase*> list)
  : Object(ObjectClass), list_(list), index_(0)
{}

void ListIter::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc.trace(t, &list_);
}

bool ListIter::iter(MutableTraced<Value> resultOut)
{
    Stack<ListIter*> self(this);
    resultOut = Value(self);
    return true;
}

bool ListIter::next(MutableTraced<Value> resultOut)
{
    assert(index_ >= -1);
    if (index_ == -1 || size_t(index_) >= list_->elements_.size()) {
        index_ = -1;
        resultOut = StopIterationException;
        return false;
    }

    assert(index_ >= 0 && (size_t)index_ < list_->elements_.size());
    resultOut = list_->elements_[index_];
    index_++;
    return true;
}

void Slice::init()
{
    ObjectClass.init(Class::createNative<Slice>("slice"));

    static const Name StartAttr = "start";
    static const Name StopAttr = "stop";
    static const Name StepAttr = "step";

    Stack<Layout*> layout = Object::InitialLayout;
    layout = layout->addName(StartAttr);
    layout = layout->addName(StopAttr);
    layout = layout->addName(StepAttr);
    assert(layout->lookupName(StartAttr) == StartSlot);
    assert(layout->lookupName(StopAttr) == StopSlot);
    assert(layout->lookupName(StepAttr) == StepSlot);
    InitialLayout.init(layout);
}

bool IsInt32OrNone(Value value)
{
    return value == Value(None) || value.isInt();
}

Slice::Slice(TracedVector<Value> args) :
  Object(ObjectClass, InitialLayout)
{
    assert(args.size() == 3);
    assert(IsInt32OrNone(args[0]));
    assert(IsInt32OrNone(args[1]));
    assert(IsInt32OrNone(args[2]));
    setSlot(StartSlot, args[0]);
    setSlot(StopSlot, args[1]);
    setSlot(StepSlot, args[2]);
}

Slice::Slice(Traced<Class*> cls)
  : Object(cls)
{
    assert(cls->isDerivedFrom(ObjectClass));
}
