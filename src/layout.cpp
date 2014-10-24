#include "layout.h"

#include <cassert>
#include <iostream>

Layout::Layout(Layout* parent, Name name)
  : parent_(parent), name_(name)
{}

void Layout::sweep()
{
    assert(isDying());
    if (!parent_->isDying())
        parent_->removeChild(this);
}

void Layout::removeChild(const Layout* child)
{
    auto i = children_.find(child->name());
    assert(i != children_.end());
    children_.erase(i);
}

unsigned Layout::slotCount() const
{
    const Layout *layout = this;
    unsigned count = 0;
    while (layout) {
        ++count;
        layout = layout->parent_;
    }
    return count;
}

bool Layout::subsumes(const Layout* other) const
{
    if (!other || this == other)
        return true;
    const Layout *layout = this;
    while (layout) {
        if (layout == other)
            return true;
        layout = layout->parent_;
    }
    return false;
}

int Layout::lookupName(Name name) const {
    const Layout *layout = this;
    while (layout) {
        if (layout->name_ == name)
            break;
        layout = layout->parent_;
    }
    if (!layout)
        return -1;
    return layout->slotCount() - 1;
}

Layout* Layout::addName(Name name)
{
    assert(lookupName(name) == -1);
    auto i = children_.find(name);
    if (i != children_.end())
        return i->second;

    Layout* child = new Layout(this, name);
    children_.emplace(name, child);
    return child;
}

void Layout::traceChildren(Tracer& t)
{
    gc::trace(t, &parent_);
}

void Layout::print(ostream& s) const
{
    s << "Layout {";
    const Layout *l = this;
    while (l) {
        s << l->name();
        l = l->parent();
        if (l)
            s << ", ";
    }
    s << "}";
}
