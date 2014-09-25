#include "object.h"

#include "class.h"
#include "none.h"

#include <cassert>
#include <iostream>

Object::Object(Class *cls)
  : cls(cls), layout(new Layout(nullptr, "__class__"))
{
    initAttrs();
}

Object::Object(Class *cls, const Layout *layout)
  : cls(cls), layout(layout)
{
    // todo: check that layout is compatible with class, i.e. that the class'
    // layout is an ancestor of the one specified
    assert(layout->subsumes(cls->getLayout()));
    initAttrs();
}

Object::~Object()
{
}

void Object::initAttrs()
{
    slots.resize(layout->slotCount());
    for (unsigned i = 0; i < layout->slotCount(); ++i)
        slots[i] = UninitializedSlot;
    setProp("__class__", cls);
}

bool Object::getProp(Name name, Value& valueOut) const
{
    int slot = layout->lookupName(name);
    if (slot == -1) {
        // todo: check this is how python actually works
        if (cls)
            return cls->getProp(name, valueOut);

        // todo: raise exception here
        cerr << "Object has no attribute '" << name << "'" << endl;
        return false;
    }
    assert(slot >= 0 && slot < slots.size());
    valueOut = slots[slot];
    if (valueOut == UninitializedSlot) {
        // todo: raise exception here
        cerr << "Reference to uninitialized attribute '" << name << "'" << endl;
        return false;
    }
    return true;
}

void Object::setProp(Name name, Value value)
{
    int slot = layout->lookupName(name);
    if (slot == -1) {
        layout = layout->addName(name);
        slot = slots.size();
        assert(layout->lookupName(name) == slot);
        slots.resize(slots.size() + 1);
    }
    assert(slot >= 0 && slot < slots.size());
    slots[slot] = value;
}

void Object::print(ostream& s) const {
    s << "Object@" << reinterpret_cast<uintptr_t>(this);
}
