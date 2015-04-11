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
    Layout(Layout* parent, Name name);

    Layout* parent() const { return parent_; }
    const string& name() const { return name_; }

    unsigned slotCount();
    bool subsumes(Layout* other);
    int lookupName(Name name);
    int hasName(Name name) { return lookupName(name) != -1; }

    Layout* addName(Name name);
    Layout* maybeAddName(Name name);

    virtual void traceChildren(Tracer& t) override;
    virtual size_t size() const { return sizeof(*this); }
    virtual void print(ostream& s) const;
    virtual void sweep();

  private:
    Layout* parent_;
    Name name_;
    unordered_map<Name, Layout*> children_;

    void removeChild(Layout* child);
};

#endif
