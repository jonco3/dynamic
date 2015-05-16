#ifndef __LAYOUT_H__
#define __LAYOUT_H__

#include "gcdefs.h"
#include "name.h"

#include <ostream>
#include <unordered_map>
#include <vector>

using namespace std;

// Layout defines a mapping from names to slots and is used for object storage
// and execution frames.

struct Layout : public Cell
{
    static const int NotFound = -1;

    static GlobalRoot<Layout*> None;

    Layout(Traced<Layout*> parent, Name name);

    Layout* parent() const { return parent_; }
    unsigned slotIndex() { return slot_; }
    unsigned slotCount() { return slot_ + 1; }
    const string& name() const { return name_; }

    bool subsumes(Layout* other);
    Layout* findAncestor(Name name);
    int lookupName(Name name);
    int hasName(Name name) { return lookupName(name) != NotFound; }

    Layout* addName(Name name);
    Layout* maybeAddName(Name name);

    virtual void traceChildren(Tracer& t) override;
    virtual void print(ostream& s) const;
    virtual void sweep();

  private:
    Layout* parent_;
    unsigned slot_;
    Name name_;
    unordered_map<Name, Layout*> children_;

    void removeChild(Layout* child);
};

#endif
