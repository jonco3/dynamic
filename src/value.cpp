#include "value-inl.h"

#include "object.h"
#include "exception.h"
#include "interp.h"

RootVector<Value> EmptyValueArray(0);

uint64_t Value::canonicalizeNaN(uint64_t bits)
{
    uint64_t mantissa = (bits & DoubleMantissaMask);
    uint64_t rest = (bits & ~DoubleMantissaMask);
    mantissa = mantissa != 0 ? 1 : 0;
    return rest | mantissa;
}

ostream& operator<<(ostream& s, const Value& v) {
    if (v.isInt32())
        s << v.asInt32();
    else if (v.isDouble())
        s << v.asDouble();
    else if (v.asObject())
        s << *v.asObject();
    else
        s << "nullptr";
    return s;
}

bool Value::toInt32(int32_t& out) const
{
    assert(isInt());
    if (isInt32()) {
        out = asInt32();
        return true;
    }

    return as<Integer>()->toSigned(out);
}

bool Value::toSize(size_t& out) const
{
    assert(isInt());
    if (isInt32()) {
        int32_t i = asInt32();
        if (i < 0)
            return false;

        out = asInt32();
        return true;
    }

    return as<Integer>()->toUnsigned(out);
}

size_t ValueHash::operator()(Value vArg) const
{
    Stack<Value> value(vArg);
    Stack<Value> hashFunc;
    Stack<Value> result;
    if (!value.maybeGetAttr(Names::__hash__, hashFunc))
        ThrowException<TypeError>("Object has no __hash__ method");

    if (!interp->call(hashFunc, value, result))
        throw PythonException(result);

    if (result.isInt32())
        return result.asInt32();

    if (!result.is<Integer>())
        ThrowException<TypeError>("__hash__ method should return an int");

    return result.as<Integer>()->value().get_ui(); // todo: truncates result
}

bool ValuesEqual::operator()(Value a, Value b) const
{
    Stack<Value> result;
    Stack<Value> eqFunc;
    if (!a.maybeGetAttr(Names::__eq__, eqFunc))
        ThrowException<TypeError>("Object has no __eq__ method");

    if (!interp->call(eqFunc, a, b, result))
        throw PythonException(result);

    Stack<Object*> obj(result.toObject());
    if (!obj->is<Boolean>())
        ThrowException<TypeError>("__eq__ method should return a bool");

    return obj->as<Boolean>()->boolValue();
}
