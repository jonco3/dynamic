#include "layout.h"
#include "name.h"

#include <cassert>
#include <iostream>

GlobalRoot<Layout*> Layout::Empty;

static bool layoutInitialized = false;

Layout::Layout()
  : parent_(nullptr), slot_(-1), name_("")
{
    assert(!layoutInitialized);
}

Layout::Layout(Traced<Layout*> parent, Name name)
  : parent_(parent), name_(name)
{
    assert(parent);
    assert(name != "");
    assert(parent->children_.find(name) == children_.end());
    parent->children_.emplace(name, this);
    slot_ = parent_->slotIndex() + 1;
}

void Layout::sweep()
{
    assert(isDying());
    assert(parent_ != this);
    if (parent_) {
        assert(parent_->hasChild(this));
        if (!parent_->isDying())
            parent_->removeChild(this);
    }
}

bool Layout::hasChild(Layout* child)
{
    return children_.find(child->name()) != children_.end();
}

void Layout::removeChild(Layout* child)
{
    auto i = children_.find(child->name());
    assert(i != children_.end());
    children_.erase(i);
}

bool Layout::subsumes(Traced<Layout*> other)
{
    if (other == Empty)
        return true;

    AutoAssertNoGC nogc;
    Layout* layout = this;
    while (layout != Empty) {
        assert(layout);
        if (layout == other)
            return true;
        assert(layout->parent_ != layout);
        layout = layout->parent_;
    }
    return false;
}

Layout* Layout::findAncestor(Name name)
{
    assert(name != "");
    Layout* layout = this;
    while (layout != Empty) {
        assert(layout);
        if (layout->name_ == name)
            return layout;
        assert(layout->parent_ != layout);
        layout = layout->parent_;
    }
    return nullptr;
}

int Layout::lookupName(Name name)
{
    assert(name != "");
    Layout* layout = findAncestor(name);
    return layout ? layout->slotIndex() : NotFound;
}

Layout* Layout::addName(Name name)
{
    assert(name != "");
    assert(!hasName(name));
    auto i = children_.find(name);
    if (i != children_.end())
        return i->second;

    Stack<Layout*> self(this);
    return gc.create<Layout>(self, name);
}

Layout* Layout::maybeAddName(Name name)
{
    assert(name != "");

    if (hasName(name))
        return this;

    return addName(name);
}

void Layout::traceChildren(Tracer& t)
{
    gc.trace(t, &parent_);
}

void Layout::print(ostream& s) const
{
    s << "Layout {";
    const Layout* layout = this;
    while (layout != Empty) {
        assert(layout);
        s << layout->name();
        layout = layout->parent();
        if (layout)
            s << ", ";
    }
    s << "}";
}

/* static */ void Layout::init()
{
    Empty.init(gc.create<Layout>());
    layoutInitialized = true;
}
