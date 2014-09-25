#ifndef __LAYOUT_H__
#define __LAYOUT_H__

#include "name.h"

#include <vector>
#include <unordered_map>

using namespace std;

// Layout defines a mapping from names to slots and is used for object storage
// and execution frames.

struct Layout
{
    Layout(const Layout* parent, Name name);

    unsigned slotCount() const;
    bool subsumes(const Layout* other) const;
    int lookupName(Name name) const;

    Layout* addName(Name name) const;

  private:
    const Layout* parent;
    const Name name;
    mutable unordered_map<Name, Layout*> children;
};

#endif
