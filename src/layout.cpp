#include "layout.h"

Layout::Layout(const Layout* parent, Name name)
  : parent(parent), name(name)
{}

unsigned Layout::slotCount() const
{
    const Layout *layout = this;
    unsigned count = 0;
    while (layout) {
        ++count;
        layout = layout->parent;
    }
    return count;
}

bool Layout::subsumes(const Layout* other) const
{
    const Layout *layout = this;
    while (layout) {
        if (layout == other)
            return true;
        layout = layout->parent;
    }
    return false;
}

int Layout::lookupName(Name name) const {
    const Layout *layout = this;
    while (layout) {
        if (layout->name == name)
            break;
        layout = layout->parent;
    }
    if (!layout)
        return -1;
    return layout->slotCount() - 1;
}

Layout* Layout::addName(Name name) const
{
    auto i = children.find(name);
    if (i != children.end())
        return i->second;

    Layout* child = new Layout(this, name);
    children.emplace(name, child);
    return child;
}
