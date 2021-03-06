#include "string.h"

#include "callable.h"
#include "exception.h"
#include "interp.h"
#include "list.h"
#include "singletons.h"
#include "slice.h"

#include "value-inl.h"

#include <iostream>

typedef bool (StringCompareOp)(const string&, const string&);
static bool stringLT(const string& a, const string& b) { return a < b; }
static bool stringLE(const string& a, const string& b) { return a <= b; }
static bool stringGT(const string& a, const string& b) { return a > b; }
static bool stringGE(const string& a, const string& b) { return a >= b; }
static bool stringEQ(const string& a, const string& b) { return a == b; }
static bool stringNE(const string& a, const string& b) { return a != b; }

// todo: use this to replace str() function.
static bool valueToString(Traced<Value> value, MutableTraced<Value> resultOut)
{
    if (value.is<String>()) {
        resultOut = value;
        return true;
    }

    Stack<Value> strFunc;
    if (!value.maybeGetAttr(Names::__str__, strFunc) &&
        !value.maybeGetAttr(Names::__repr__, strFunc))
    {
        return Raise<TypeError>("Object has no __str__ or __repr__ method",
                                resultOut);
    }

    if (!interp->call(strFunc, value, resultOut))
        return false;

    if (!resultOut.is<String>()) {
        return Raise<TypeError>("__str__ method should return a string",
                                resultOut);
    }

    return true;
}

static bool str_new(NativeArgs args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;

    if (args.size() == 1) {
        resultOut = String::get("");
        return true;
    }

    return valueToString(args[1], resultOut);
}

static bool str_add(NativeArgs args, MutableTraced<Value> resultOut) {
    if (!args[0].isInstanceOf<String>() || !args[1].isInstanceOf<String>()) {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = String::get(args[0].as<String>()->value() +
                            args[1].as<String>()->value());
    return true;
}

static bool str_str(NativeArgs args, MutableTraced<Value> resultOut) {
    resultOut = args[0];
    return true;
}

static bool stringValue(Traced<Value> value, string& out)
{
    if (!value.is<String>())
        return false;

    out = value.as<const String>()->value();
    return true;
}

template <StringCompareOp op>
static bool stringCompareOp(NativeArgs args, MutableTraced<Value> resultOut)
{
    string a;
    if (!stringValue(args[0], a)) {
        resultOut = NotImplemented;
        return true;
    }

    string b;
    if (!stringValue(args[1], b)) {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Boolean::get(op(a, b));
    return true;
}

static bool str_len(NativeArgs args, MutableTraced<Value> resultOut)
{
    const string& s = args[0].as<String>()->value();
    resultOut = Integer::get(s.size());
    return true;
}

static bool str_getitem(NativeArgs args, MutableTraced<Value> resultOut)
{
    Stack<String*> s(args[0].as<String>());
    return s->getitem(args[1], resultOut);
}

static bool str_hash(NativeArgs args, MutableTraced<Value> resultOut) {
    const string& a = args[0].as<String>()->value();
    // todo: should probably use python hash algorithm for this.
    resultOut = Integer::get(std::hash<string>()(a));
    return true;
}

static bool str_contains(NativeArgs args, MutableTraced<Value> resultOut) {
    const string& a = args[0].as<String>()->value();
    const string& b = args[1].as<String>()->value();
    resultOut = Boolean::get(a.find(b) != string::npos);
    return true;
}

static bool str_print(NativeArgs args, MutableTraced<Value> resultOut) {
    const string& a = args[0].as<String>()->value();
    cout << a << endl;
    resultOut = None;
    return true;
}

static bool str_split(NativeArgs args, MutableTraced<Value> resultOut) {
    if (!checkInstanceOf(args[0], String::ObjectClass, resultOut))
        return false;

    const string& str = args[0].as<String>()->value();
    Stack<List*> result(List::getUninitialised(0));
    Stack<Value> value;

    if (args.size() == 1) {
        // Split on whitespace.
        // todo: check whether this matches python's concept of whitespace.
        const string spaces = " \t\n\r";
        size_t pos = str.find_first_not_of(spaces, 0);
        size_t last = pos;
        while (pos = str.find_first_of(spaces, pos), pos != string::npos) {
            value = gc.create<String>(str.substr(last, pos - last));
            result->append(value);
            pos = str.find_first_not_of(spaces, pos);
            last = pos;
        }
        if (last != string::npos) {
            value = gc.create<String>(str.substr(last, pos - last));
            result->append(value);
        }

        resultOut = Value(result);
        return true;
    }

    if (!checkInstanceOf(args[1], String::ObjectClass, resultOut))
        return false;

    const string& sep = args[1].as<String>()->value();
    size_t pos = 0;
    size_t last = 0;
    while (pos = str.find(sep, pos), pos != string::npos) {
        value = gc.create<String>(str.substr(last, pos - last));
        result->append(value);
        pos += sep.size();
        last = pos;
    }
    value = gc.create<String>(str.substr(last, pos - last));
    result->append(value);
    resultOut = Value(result);
    return true;
}

GlobalRoot<Class*> String::ObjectClass;
GlobalRoot<String*> String::EmptyString;

void String::init()
{
    ObjectClass.init(gc.create<Class>("str",
                                      Object::ObjectClass,
                                      Class::InitialLayout,
                                      true));

    initNames();

    // Can't call Class::createNative before initNames().
    initNativeMethod(ObjectClass, "__new__", str_new, 1, 2);

    initNativeMethod(ObjectClass, "__len__", str_len, 1);
    initNativeMethod(ObjectClass, "__getitem__", str_getitem, 2);
    initNativeMethod(ObjectClass, "__add__", str_add, 2);
    initNativeMethod(ObjectClass, "__str__", str_str, 1);
    initNativeMethod(ObjectClass, "__eq__", stringCompareOp<stringEQ>, 2);
    initNativeMethod(ObjectClass, "__ne__", stringCompareOp<stringNE>, 2);
    initNativeMethod(ObjectClass, "__lt__", stringCompareOp<stringLT>, 2);
    initNativeMethod(ObjectClass, "__le__", stringCompareOp<stringLE>, 2);
    initNativeMethod(ObjectClass, "__gt__", stringCompareOp<stringGT>, 2);
    initNativeMethod(ObjectClass, "__ge__", stringCompareOp<stringGE>, 2);
    initNativeMethod(ObjectClass, "__hash__", str_hash, 1);
    initNativeMethod(ObjectClass, "__contains__", str_contains, 2);
    initNativeMethod(ObjectClass, "_print", str_print, 1);
    initNativeMethod(ObjectClass, "split", str_split, 1, 2);

    EmptyString.init(Names::emptyString);
}

String::String(const string& v)
  : Object(ObjectClass), value_(v)
{}

String::String(Traced<Class*> cls)
  : Object(cls)
{
    assert(cls);
    assert(cls->isDerivedFrom(ObjectClass));
}

String* String::get(const string& v)
{
    if (v == "")
        return EmptyString;
    // todo: can intern short strings here
    return gc.create<String>(v);
}

void String::print(ostream& s) const
{
    s << "'" << value_ << "'";
}


static bool raiseOutOfRange(MutableTraced<Value> resultOut)
{
    return Raise<IndexError>("string index out of range", resultOut);
}

bool String::getitem(Traced<Value> index, MutableTraced<Value> resultOut)
{
    size_t len = value_.size();
    if (index.isInt()) {
        int32_t i;
        if (!index.toInt32(i))
            return raiseOutOfRange(resultOut);
        i = WrapIndex(i, len);
        if (i < 0 || size_t(i) >= len)
            return raiseOutOfRange(resultOut);
        resultOut = get(string(1, value_[i]));
        return true;
    } else if (index.isInstanceOf(Slice::ObjectClass)) {
        Stack<Slice*> slice(index.as<Slice>());
        int32_t start, count, step;
        if (!slice->getIterationData(len, start, count, step, resultOut))
            return false;

        string result;
        int32_t src = start;
        for (size_t i = 0; i < count; i++) {
            assert(src < len);
            result += value_[src];
            src += step;
        }

        resultOut = String::get(result);
        return true;
    } else {
        return Raise<TypeError>("string indices must be integers or slices",
                                resultOut);
    }
}
