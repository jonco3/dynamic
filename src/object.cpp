#include "object.h"
#include "class.h"

#include <cassert>
#include <iostream>

bool Object::getProp(Name name, Value& valueOut)
{
    int slot = layout->lookupName(name);
    if (slot == -1) {
        // todo: check this is how python actually works
        if (cls)
            return cls->getProp(name, valueOut);

        // todo: raise exception here
        std::cerr << "Object has no attribute '" << name << "'" << std::endl;
        return false;
    }
    assert(slot >= 0 && slot < slots.size());
    valueOut = slots[slot];
    return true;
}

void Object::setProp(Name name, Value value)
{
    int slot = layout->lookupName(name);
    if (slot == -1) {
        slot = layout->addName(name);
        assert(slot == slots.size());
        slots.resize(slots.size() + 1);
    }
    assert(slot >= 0 && slot < slots.size());
    slots[slot] = value;
}

void Object::print(std::ostream& s) const {
    s << "Object@" << reinterpret_cast<uintptr_t>(this);
}
