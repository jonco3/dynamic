#include "src/object.h"

#include "src/numeric.h"
#include "src/test.h"
#include "src/value-inl.h"

testcase(object)
{
    Stack<Object*> o(Object::create());

    Name foo("foo");
    Name bar("bar");
    Name baz("baz");

    Stack<Value> v;
    testFalse(o->hasAttr(foo));
    testFalse(o->maybeGetAttr(foo, v));

    Stack<Value> value(Integer::get(1));
    o->setAttr(foo, value);
    testTrue(o->hasAttr(foo));
    testTrue(o->maybeGetAttr(foo, v));
    Stack<Object*> obj(v.toObject());
    testEqual(obj->as<Integer>()->value(), 1);

    value = Integer::get(2);
    o->setAttr(bar, value);
    value = Integer::get(3);
    o->setAttr(baz, value);
    testTrue(o->hasAttr(foo));
    testTrue(o->hasAttr(bar));
    testTrue(o->hasAttr(baz));

    Name missing("missing");
    testFalse(o->maybeDelOwnAttr(missing));

    testTrue(o->maybeDelOwnAttr(bar));
    testTrue(o->hasAttr(foo));
    testFalse(o->hasAttr(bar));
    testTrue(o->hasAttr(baz));
    testEqual(o->getAttr(foo).asInt32(), 1);
    testEqual(o->getAttr(baz).asInt32(), 3);

    testTrue(o->maybeDelOwnAttr(baz));
    testTrue(o->hasAttr(foo));
    testFalse(o->hasAttr(baz));
    testEqual(o->getAttr(foo).asInt32(), 1);

    testTrue(o->maybeDelOwnAttr(foo));
    testFalse(o->hasAttr(foo));

    testFalse(o->maybeDelOwnAttr(foo));

    testFalse(None->isTrue());
    testFalse(Integer::get(0).isTrue());
    testTrue(Integer::get(1).isTrue());
}
