#ifndef __MODULE_H__
#define __MODULE_H__

#include "frame.h"
#include "string.h"

struct Dict;

extern void initModules();

struct Module : public Env
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;
    static GlobalRoot<Module*> Sys;
    static GlobalRoot<Dict*> Cache;

    Module(Traced<String*> name,
           Traced<String*> package = String::EmptyString,
           Traced<Class*> cls = ObjectClass,
           Traced<Layout*> layout = InitialLayout);

  private:
    friend void initModules();
    static void Init();
    static bool New(NativeArgs args, MutableTraced<Value> resultOut);

    static const int NameSlot = 0;
    static const int PackageSlot = 1;
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

    static const int PathSlot = 2;
    static GlobalRoot<Layout*> InitialLayout;
};

#endif
