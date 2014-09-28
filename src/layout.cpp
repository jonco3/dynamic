#include "layout.h"

#include <cassert>
#include <iostream>

Layout::Layout(const Layout* parent, Name name)
  : parent_(parent), name_(name)
{}

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

Layout* Layout::addName(Name name) const
{
    assert(lookupName(name) == -1);
    auto i = children_.find(name);
    if (i != children_.end())
        return i->second;

    Layout* child = new Layout(this, name);
    children_.emplace(name, child);
    return child;
}

ostream& operator<<(ostream& s, const Layout* layout)
{
    if (!layout) {
        s << "<empty>";
        return s;
    }
    if (layout->parent())
        s << layout->parent() << ", ";
    s << layout->name() << "@" << (void*)layout;
    return s;
}
