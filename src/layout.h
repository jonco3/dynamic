#ifndef __LAYOUT_H__
#define __LAYOUT_H__

#include "name.h"

#include <ostream>
#include <unordered_map>
#include <vector>

using namespace std;

// Layout defines a mapping from names to slots and is used for object storage
// and execution frames.

struct Layout
{
    Layout(const Layout* parent, Name name);

    const Layout* parent() const { return parent_; }
    const string& name() const { return name_; }

    unsigned slotCount() const;
    bool subsumes(const Layout* other) const;
    int lookupName(Name name) const;

    Layout* addName(Name name) const;

  private:
    const Layout* parent_;
    const Name name_;
    mutable unordered_map<Name, Layout*> children_;
};

ostream& operator<<(ostream& s, const Layout* layout);

#endif
