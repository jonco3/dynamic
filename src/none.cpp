#include "none.h"

Class NoneObject::Class;
static NoneObject NoneInstance;
NoneObject* None = &NoneInstance;

Class UninitializedSlotObject::Class;
static UninitializedSlotObject UninitializedSlotInstance;
UninitializedSlotObject* UninitializedSlot = &UninitializedSlotInstance;
