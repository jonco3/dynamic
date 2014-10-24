#ifndef __LAYOUT_H__
#define __LAYOUT_H__

#include "gc.h"
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

    const Layout* parent() const { return parent_; }
    const string& name() const { return name_; }

    unsigned slotCount() const;
    bool subsumes(const Layout* other) const;
    int lookupName(Name name) const;
    int hasName(Name name) const { return lookupName(name) != -1; }

    Layout* addName(Name name);

    virtual void traceChildren(Tracer& t);
    virtual size_t size() const { return sizeof(*this); }
    virtual void print(ostream& s) const;
    virtual void sweep();

  private:
    Layout* parent_;
    Name name_;
    unordered_map<Name, Layout*> children_;

    void removeChild(const Layout* child);
};

inline ostream& operator<<(ostream& s, const Layout* layout) {
    if (layout)
        layout->print(s);
    else
        s << "Layout {}";
    return s;
}

#endif
