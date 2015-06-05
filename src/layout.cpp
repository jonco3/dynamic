#include "layout.h"

#include <cassert>
#include <iostream>

GlobalRoot<Layout*> Layout::None;

Layout::Layout(Traced<Layout*> parent, Name name)
  : parent_(parent), name_(name)
{
    assert(parent_ != this);
    slot_ = parent_ ? parent_->slotIndex() + 1 : 0;
}

void Layout::sweep()
{
    assert(isDying());
    assert(parent_ != this);
    if (parent_ && !parent_->isDying())
        parent_->removeChild(this);
}

void Layout::removeChild(Layout* child)
{
    auto i = children_.find(child->name());
    assert(i != children_.end());
    children_.erase(i);
}

bool Layout::subsumes(Layout* other)
{
    if (!other || this == other)
        return true;
    Layout* layout = this;
    while (layout) {
        if (layout == other)
            return true;
        assert(layout->parent_ != layout);
        layout = layout->parent_;
    }
    return false;
}

Layout* Layout::findAncestor(Name name)
{
    Layout* layout = this;
    while (layout) {
        if (layout->name_ == name)
            break;
        assert(layout->parent_ != layout);
        layout = layout->parent_;
    }
    return layout;
}

int Layout::lookupName(Name name)
{
    Layout* layout = findAncestor(name);
    return layout ? layout->slotIndex() : NotFound;
}

Layout* Layout::addName(Name name)
{
    assert(!hasName(name));
    auto i = children_.find(name);
    if (i != children_.end())
        return i->second;

    Root<Layout*> self(this);
    Layout* child = gc.create<Layout>(self, name);
    children_.emplace(name, child);
    return child;
}

Layout* Layout::maybeAddName(Name name)
{
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
    const Layout* l = this;
    while (l) {
        s << l->name();
        l = l->parent();
        if (l)
            s << ", ";
    }
    s << "}";
}
