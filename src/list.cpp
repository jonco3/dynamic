#include "list.h"

#include "builtin.h"
#include "callable.h"
#include "exception.h"
#include "interp.h"
#include "numeric.h"
#include "singletons.h"
#include "slice.h"

#include "value-inl.h"

#include <algorithm>

template <typename T>
struct ListIterImpl : public Object
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    ListIterImpl(Traced<T*> list);

    void traceChildren(Tracer& t) override;

    bool iter(MutableTraced<Value> resultOut);
    bool next(MutableTraced<Value> resultOut);

  private:
    Heap<T*> list_;
    int index_;
};

GlobalRoot<Class*> Tuple::ObjectClass;
GlobalRoot<Tuple*> Tuple::Empty;
GlobalRoot<Class*> List::ObjectClass;
template <typename T>
GlobalRoot<Class*> ListIterImpl<T>::ObjectClass;

template struct ListIterImpl<Tuple>;
template struct ListIterImpl<List>;

typedef ListIterImpl<Tuple> TupleIter;
typedef ListIterImpl<List> ListIter;

inline static size_t allocSize(size_t size)
{
    return sizeof(Tuple) + size * sizeof(Heap<Value>);
}

/* static */ Tuple* Tuple::get(const TracedVector<Value>& values)
{
    if (values.size() == 0)
        return Empty;

    return gc.createSized<Tuple>(allocSize(values.size()), values);
}

/* static */ Tuple* Tuple::get(Traced<Class*> cls, Traced<Tuple*> init)
{
    return gc.createSized<Tuple>(allocSize(init->len()), cls, init);
}

/* static */ Tuple* Tuple::get(Traced<Class*> cls, Traced<List*> init)
{
    return gc.createSized<Tuple>(allocSize(init->len()), cls, init);
}

/* static */ Tuple* Tuple::getUninitialised(size_t size, Traced<Class*> cls)
{
    if (size == 0 && cls == ObjectClass)
        return Empty;

    return gc.createSized<Tuple>(allocSize(size), size, cls);
}

Tuple::Tuple(const TracedVector<Value>& values)
  : Object(ObjectClass), size_(values.size())
{
    for (unsigned i = 0; i < values.size(); i++)
        elements_[i] = values[i];
}

Tuple::Tuple(size_t size, Traced<Class*> cls)
  : Object(ObjectClass), size_(size)
{
    assert(cls->isDerivedFrom(ObjectClass));
#ifdef DEBUG
    for (unsigned i = 0; i < size; i++)
        elements_[i] = UninitializedSlot;
#endif
}

Tuple::Tuple(Traced<Class*> cls, Traced<Tuple*> init)
  : Object(cls), size_(init->len())
{
    assert(cls->isDerivedFrom(ObjectClass));
    for (size_t i = 0; i < size_; i++)
        elements_[i] = init->getitem(i);
}

Tuple::Tuple(Traced<Class*> cls, Traced<List*> init)
  : Object(cls), size_(init->len())
{
    assert(cls->isDerivedFrom(ObjectClass));
    for (size_t i = 0; i < size_; i++)
        elements_[i] = init->getitem(i);
}

void Tuple::initElement(size_t index, const Value& value)
{
    assert(index < size_);
    assert(elements_[index].asObject() == UninitializedSlot);
    elements_[index] = value;
}

void Tuple::print(ostream& s) const
{
    s << "(";
    for (unsigned i = 0; i < size_; ++i) {
        if (i != 0)
            s << ", ";
        s << elements_[i];
    }
    if (size_ == 1)
            s << ",";
    s << ")";
}

void Tuple::traceChildren(Tracer& t)
{
    for (size_t i = 0; i < size_; i++)
        gc.trace(t, &elements_[i]);
}

/* static */ List* List::get(Traced<Class*> cls, Traced<Tuple*> init)
{
    return gc.create<List>(cls, init);
}

/* static */ List* List::get(Traced<Class*> cls, Traced<List*> init)
{
    return gc.create<List>(cls, init);
}

/* static */ List* List::getUninitialised(size_t size, Traced<Class*> cls)
{
    return gc.create<List>(size, cls);
}

List::List(const TracedVector<Value>& values)
  : Object(ObjectClass), elements_(values.size())
{
    for (unsigned i = 0; i < values.size(); ++i)
        elements_[i] = values[i];
}

List::List(size_t size, Traced<Class*> cls)
  : Object(ObjectClass)
{
    assert(cls->isDerivedFrom(ObjectClass));
    elements_.reserve(size);
}

List::List(Traced<Class*> cls, Traced<List*> init)
  : Object(cls), elements_(init->elements_)
{
    assert(cls->isDerivedFrom(ObjectClass));
}

List::List(Traced<Class*> cls, Traced<Tuple*> init)
  : Object(cls), elements_(init->len())
{
    assert(cls->isDerivedFrom(ObjectClass));
    for (size_t i = 0; i < elements_.size(); ++i)
        elements_[i] = init->getitem(i);
}

void List::initElement(size_t index, const Value& value)
{
    assert(elements_.size() == index);
    elements_.push_back_reserved(value);
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

void List::traceChildren(Tracer& t)
{
    gc.traceVector(t, &elements_);
}

static bool RaiseOutOfRange(MutableTraced<Value> resultOut)
{
    return Raise<IndexError>("index out of range", resultOut);
}

template <typename T>
inline static bool
GetIndex(T* self, Traced<Value> index,
         int32_t* intOut, MutableTraced<Value> resultOut)
{
    if (!index.isInt())
        return Raise<TypeError>("indices must be integers", resultOut);

    int32_t i;
    if (!index.toInt32(i))
        return RaiseOutOfRange(resultOut);

    size_t len = self->len();
    i = WrapIndex(i, len);
    if (i < 0 || size_t(i) >= len)
        return RaiseOutOfRange(resultOut);

    *intOut = i;
    return true;
}

void List::setitem(int32_t index, Value value)
{
    assert(index < elements_.size());
    elements_[index] = value;
}

bool List::delitem(Traced<Value> index, MutableTraced<Value> resultOut)
{

    int32_t i;
    if (!GetIndex(this, index, &i, resultOut))
        return false;

    elements_.erase(elements_.begin() + i);
    resultOut = None;
    return true;
}

void List::replaceitems(int32_t start, int32_t count, Traced<List*> list)
{
    // todo: could be more clever here.
    assert(start + count <= len());
    auto i = elements_.begin() + start;
    elements_.erase(i, i + count);
    elements_.insert(i, list->elements_.begin(), list->elements_.end());
}

void List::append(Traced<Value> element)
{
    elements_.push_back(element);
}

static bool compareElements(Value aArg, Value bArg)
{
    Stack<Value> a(aArg);
    Stack<Value> b(bArg);
    Stack<Value> compare;
    Stack<Value> result;
    if (!a.maybeGetAttr(Names::compareMethod[CompareLT], compare))
        ThrowException<TypeError>("Object has no __lt__ method");

    if (!interp->call(compare, a, b, result))
        throw PythonException(result);

    return Value::IsTrue(result);
}

void List::sort()
{
    ::sort(elements_.begin(), elements_.end(), compareElements);
}

template <typename T>
ListIterImpl<T>::ListIterImpl(Traced<T*> list)
  : Object(ObjectClass), list_(list), index_(0)
{}

template <typename T>
void ListIterImpl<T>::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc.trace(t, &list_);
}

template <typename T>
bool ListIterImpl<T>::iter(MutableTraced<Value> resultOut)
{
    Stack<ListIterImpl*> self(this);
    resultOut = Value(self);
    return true;
}

template <typename T>
bool ListIterImpl<T>::next(MutableTraced<Value> resultOut)
{
    assert(index_ >= -1);
    if (index_ == -1 || size_t(index_) >= list_->len()) {
        index_ = -1;
        resultOut = StopIterationException;
        return false;
    }

    assert(index_ >= 0 && (size_t)index_ < list_->len());
    resultOut = list_->getitem(index_);
    index_++;
    return true;
}

template <class T>
static bool generic_new(NativeArgs args,
                        MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;

    Stack<Class*> cls(args[0].asObject()->as<Class>());
    assert(cls->isDerivedFrom(List::ObjectClass) ||
           cls->isDerivedFrom(Tuple::ObjectClass));

    if (args.size() == 1) {
        resultOut = T::getUninitialised(0, cls);
        return true;
    }

    Stack<Object*> arg(args[1].toObject());
    if (arg->isInstanceOf(List::ObjectClass)) {
        Stack<List*> init(arg->as<List>());
        resultOut = T::get(cls, static_cast<Traced<List*>>(init));
        return true;
    }

    if (arg->isInstanceOf(Tuple::ObjectClass)) {
        Stack<Tuple*> init(arg->as<Tuple>());
        resultOut = T::get(cls, static_cast<Traced<Tuple*>>(init));
        return true;
    }

    Stack<Value> result;
    if (!interp->call(IterableToList, Value(arg), result)) {
        resultOut = result;
        return false;
    }

    // todo: can skip copy here if T is List and cls is List::ObjectClass.

    Stack<List*> init(result.as<List>());
    resultOut = T::get(cls, static_cast<Traced<List*>>(init));
    return true;
}

template <class T>
static bool generic_len(NativeArgs args,
                        MutableTraced<Value> resultOut)
{
    resultOut = Integer::get(args[0].as<T>()->len());
    return true;
}


template <class T>
static bool generic_getitem(NativeArgs args,
                            MutableTraced<Value> resultOut)
{
    Stack<T*> self(args[0].as<T>());
    Stack<Value> index(args[1]);

    if (index.isInt()) {
        int32_t i;
        if (!GetIndex(self.get(), index, &i, resultOut))
            return false;
        resultOut = self->getitem(i);
        return true;
    } else if (index.isInstanceOf(Slice::ObjectClass)) {
        Stack<Slice*> slice(index.as<Slice>());
        int32_t start, count, step;
        if (!slice->getIterationData(self->len(), start, count, step,
                                     resultOut))
        {
            return false;
        }

        Stack<T*> result(T::getUninitialised(count));
        int32_t src = start;
        for (size_t i = 0; i < count; i++) {
            assert(src < self->len());
            result->initElement(i, self->getitem(src));
            src += step;
        }

        resultOut = Value(result);
        return true;
    } else {
        return Raise<TypeError>("indices must be integers or slices", resultOut);
    }
}

template <class T>
static bool generic_iter(NativeArgs args,
                         MutableTraced<Value> resultOut)
{
    Stack<T*> self(args[0].as<T>());
    resultOut = gc.create<ListIterImpl<T>>(self);
    return true;
}

template <class T>
static bool generic_add(NativeArgs args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], T::ObjectClass, resultOut) ||
        !checkInstanceOf(args[1], T::ObjectClass, resultOut))
    {
        return false;
    }

    Stack<T*> a(args[0].as<T>());
    Stack<T*> b(args[1].as<T>());
    size_t len = a->len() + b->len();
    Stack<T*> result(T::getUninitialised(len));
    for (size_t i = 0; i < a->len(); i++)
        result->initElement(i, a->getitem(i));
    for (size_t i = 0; i < b->len(); i++)
        result->initElement(i + a->len(), b->getitem(i));

    resultOut = Value(result);
    return true;
}

template <class T>
static bool generic_mul(NativeArgs args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], T::ObjectClass, resultOut) ||
        !checkInstanceOf(args[1], Integer::ObjectClass, resultOut))
    {
        return false;
    }

    Stack<T*> a(args[0].as<T>());
    size_t reps;
    if (!args[1].toSize(reps))
        return Raise<ValueError>("Too many repetitions", resultOut);

    size_t len = a->len() * reps; // todo: may overflow?
    Stack<T*> result(T::getUninitialised(len));
    for (size_t i = 0; i < reps; i++) {
        for (size_t j = 0; j < a->len(); j++)
            result->initElement(i * a->len() + j, a->getitem(j));
    }

    resultOut = Value(result);
    return true;
}

static bool list_setitem(NativeArgs args,
                         MutableTraced<Value> resultOut)
{
    Stack<List*> self(args[0].as<List>());
    Stack<Value> index(args[1]);
    if (index.isInt()) {
        int32_t i;
        if (!GetIndex(self.get(), index, &i, resultOut))
            return false;

        self->setitem(i, args[2]);
        resultOut = args[2];
        return true;
    }

    if (index.isInstanceOf(Slice::ObjectClass)) {
        if (!args[2].is<List>()) {
            // todo: reference says this should be the same type, but cpython
            // accepts e.g. tuples.
            return Raise<TypeError>("expecting list", resultOut);
        }

        Stack<Slice*> slice(index.as<Slice>());
        int32_t start, count, step;
        if (!slice->getIterationData(self->len(), start, count, step,
                                     resultOut))
        {
            return false;
        }

        Stack<List*> list(args[2].as<List>());
        if (step == 1) {
            self->replaceitems(start, count, list);
        } else {
            if (count != list->len()) {
                return Raise<ValueError>("Assigned list is the wrong size",
                                         resultOut);
            }

            int32_t dest = start;
            for (size_t i = 0; i < count; i++) {
                assert(dest < self->len());
                self->setitem(dest, list->getitem(i));
                dest += step;
            }
        }

        resultOut = args[2];
        return true;
    }

    return Raise<TypeError>("indices must be integers or slices", resultOut);
}

static bool list_delitem(NativeArgs args,
                         MutableTraced<Value> resultOut)
{
    return args[0].as<List>()->delitem(args[1], resultOut);
}

static bool list_append(NativeArgs args, MutableTraced<Value> resultOut)
{
    args[0].as<List>()->append(args[1]);
    resultOut = None;
    return true;
}

static bool list_sort(NativeArgs args, MutableTraced<Value> resultOut)
{
    args[0].as<List>()->sort();
    resultOut = None;
    return true;
}

template <class T>
static void generic_initNatives(Traced<Class*> cls)
{
    Stack<Value> value;
    initNativeMethod(cls, "__len__", generic_len<T>, 1);
    initNativeMethod(cls, "__getitem__", generic_getitem<T>, 2);
    initNativeMethod(cls, "__iter__", generic_iter<T>, 1);
    initNativeMethod(cls, "__add__", generic_add<T>, 2);
    initNativeMethod(cls, "__mul__", generic_mul<T>, 2);
    // __eq__ and __ne__ are supplied by internals/internal.py
}

void Tuple::init()
{
    ObjectClass.init(Class::createNative("tuple", generic_new<Tuple>, 2));
    generic_initNatives<Tuple>(ObjectClass);
    Empty.init(gc.createSized<Tuple>(allocSize(0), EmptyValueArray));
}

void List::init()
{
    ObjectClass.init(Class::createNative("list", generic_new<List>, 2));
    generic_initNatives<List>(ObjectClass);
    initNativeMethod(ObjectClass, "__setitem__", list_setitem, 3);
    initNativeMethod(ObjectClass, "__delitem__", list_delitem, 2);
    initNativeMethod(ObjectClass, "append", list_append, 2);
    initNativeMethod(ObjectClass, "sort", list_sort, 1);
}

template <typename T>
static bool listIter_iter(NativeArgs args,
                          MutableTraced<Value> resultOut)
{
    ListIterImpl<T>* i = args[0].as<ListIterImpl<T>>();
    return i->iter(resultOut);
}

template <typename T>
static bool listIter_next(NativeArgs args,
                          MutableTraced<Value> resultOut)
{
    ListIterImpl<T>* i = args[0].as<ListIterImpl<T>>();
    return i->next(resultOut);
}

template<typename T>
void ListIterImpl<T>::init()
{
    ObjectClass.init(Class::createNative("listiterator", nullptr));
    initNativeMethod(ObjectClass, "__iter__", listIter_iter<T>, 1);
    initNativeMethod(ObjectClass, "__next__", listIter_next<T>, 1);
}

void initList()
{
    List::init();
    TupleIter::init();
    ListIter::init();
}
