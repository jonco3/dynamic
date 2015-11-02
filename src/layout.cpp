#include "layout.h"
#include "name.h"

#include <cassert>
#include <iostream>

GlobalRoot<Layout*> Layout::Empty;

static bool layoutInitialized = false;

inline Layout::Children::Children()
  : hasMany_(false), single_(nullptr)
{}

inline Layout::Children::~Children()
{
    if (hasMany_) {
        assert(many_);
        delete many_;
    }
}

inline bool Layout::Children::has(Name name)
{
    if (hasMany_)
        return many_->find(name) != many_->end();

    return single_ && single_->name() == name;
}

inline void Layout::Children::add(Layout* layout)
{
    assert(!has(layout->name()));

    if (!hasMany_) {
        if (!single_) {
            single_ = layout;
            return;
        }

        Layout* current = single_;
        hasMany_ = true;
        many_ = new Map();
        many_->emplace(current->name(), current);
    }

    assert(many_);
    many_->emplace(layout->name(), layout);
}

inline void Layout::Children::remove(Name name)
{
    if (!hasMany_) {
        assert(single_ && single_->name() == name);
        single_ = nullptr;
        return;
    }

    assert(many_);
    auto i = many_->find(name);
    assert(i != many_->end());
    many_->erase(i);
}

inline Layout* Layout::Children::get(Name name)
{
    if (!hasMany_) {
        if (single_ && single_->name() == name)
            return single_;

        return nullptr;
    }

    assert(many_);
    auto i = many_->find(name);
    if (i != many_->end())
        return i->second;

    return nullptr;
}

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
    parent->children_.add(this);
    slot_ = parent_->slotIndex() + 1;
}

inline bool Layout::hasChild(Layout* child)
{
    return children_.get(child->name()) == child;
}

inline void Layout::removeChild(Layout* child)
{
    children_.remove(child->name());
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
    Layout* child = children_.get(name);
    if (child)
        return child;

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
