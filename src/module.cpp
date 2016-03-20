#include "module.h"

#include "dict.h"
#include "exception.h"
#include "list.h"

#include "value-inl.h"

GlobalRoot<Class*> Module::ObjectClass;
GlobalRoot<Layout*> Module::InitialLayout;
GlobalRoot<Module*> Module::Sys;
GlobalRoot<Dict*> Module::Cache;
GlobalRoot<Class*> Package::ObjectClass;
GlobalRoot<Layout*> Package::InitialLayout;

void Module::Init()
{
    ObjectClass.init(Class::createNative("module", New, 2, Env::ObjectClass));
    InitialLayout.init(Object::InitialLayout->addName(Names::__name__)
                                            ->addName(Names::__package__));
    assert(InitialLayout->lookupName(Names::__name__) == NameSlot);
    assert(InitialLayout->lookupName(Names::__package__) == PackageSlot);

    Stack<String*> name(Names::sys);
    Sys.init(gc.create<Module>(name));
    Cache.init(gc.create<Dict>());

    Sys->setAttr(Names::modules, Cache);

    // todo: get this from command line or env
    Stack<String*> defaultPath(gc.create<String>("lib"));

    Stack<List*> path(List::getUninitialised(1));
    path->initElement(0, Value(defaultPath));
    Stack<Value> value(path);
    Sys->setAttr(Names::path, value);

    Stack<Value> key(name);
    value = Sys;
    Cache->setitem(key, value);
}

/* static */ bool Module::New(NativeArgs args,
                              MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;

    if (!args[1].is<String>())
        return Raise<TypeError>("Expecting string argument", resultOut);

    Stack<Class*> cls(args[0].as<Class>());
    Stack<String*> name(args[1].as<String>());
    resultOut = gc.create<Module>(name, nullptr, cls);
    return true;
}

Module::Module(Traced<String*> name,
               Traced<String*> package,
               Traced<Class*> cls,
               Traced<Layout*> layout)
  : Env(nullptr, layout, cls)
{
    assert(cls->isDerivedFrom(ObjectClass));
    assert(layout->subsumes(InitialLayout));
    setSlot(NameSlot, Value(name));
    setSlot(PackageSlot, package ? Value(package) : Value(None));
}

void Package::Init()
{
    ObjectClass.init(Class::createNative("package", New, 3,
                                         Module::ObjectClass));
    InitialLayout.init(Module::InitialLayout->addName(Names::__path__));
    assert(InitialLayout->lookupName(Names::__path__) == PathSlot);
}

/* static */ bool Package::New(NativeArgs args,
                               MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;

    if (!args[1].is<String>() || !args[2].is<String>())
        return Raise<TypeError>("Expecting string argument", resultOut);

    Stack<Class*> cls(args[0].as<Class>());
    Stack<String*> name(args[1].as<String>());
    Stack<String*> path(args[2].as<String>());
    resultOut = gc.create<Package>(name, path, cls);
    return true;
}

Package::Package(Traced<String*> name, Traced<String*> path,
                 Traced<Class*> cls, Traced<Layout*> layout)
  : Module(name, name, cls, layout)
{
    assert(cls->isDerivedFrom(ObjectClass));
    assert(layout->subsumes(InitialLayout));
    //assert(layout()->lookupName(Names::__path__) == PathSlot);
    setSlot(PathSlot, Value(path));
}

void initModules()
{
    Module::Init();
    Package::Init();

}
