#ifndef __MODULE_H__
#define __MODULE_H__

#include "string.h"

struct Dict;

extern void initModules();

struct Module : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Module*> Sys;
    static GlobalRoot<Dict*> Cache;

    Module(Traced<String*> name,
           Traced<Class*> cls = ObjectClass,
           Traced<Layout*> layout = InitialLayout);

  private:
    friend void initModules();
    static void Init();
    static bool New(NativeArgs args, MutableTraced<Value> resultOut);

    static const int NameSlot = 0;
    static GlobalRoot<Layout*> InitialLayout;
};

struct Package : public Module
{
    static GlobalRoot<Class*> ObjectClass;

    Package(Traced<String*> name, Traced<String*> path,
            Traced<Class*> cls = ObjectClass,
            Traced<Layout*> layout = InitialLayout);

  private:
    friend void initModules();
    static void Init();
    static bool New(NativeArgs args, MutableTraced<Value> resultOut);

    static const int PathSlot = 0;
    static GlobalRoot<Layout*> InitialLayout;
};

#endif
