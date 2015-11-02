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

struct LayoutChildren;

struct Layout : public SweptCell
{
    static const int NotFound = -1;

    static GlobalRoot<Layout*> Empty;

    static void init();

    Layout(Traced<Layout*> parent, Name name);
    Layout(); // Only used during initialization.

    Layout* parent() const { return parent_; }
    unsigned slotIndex() { return slot_; }
    unsigned slotCount() { return slot_ + 1; }
    Name name() const { return name_; }

    bool subsumes(Traced<Layout*> other);
    Layout* findAncestor(Name name);
    int lookupName(Name name);
    int hasName(Name name) { return lookupName(name) != NotFound; }

    Layout* addName(Name name);
    Layout* maybeAddName(Name name);

    void traceChildren(Tracer& t) override;
    void print(ostream& s) const override;
    void sweep() override;

  private:
    struct Children
    {
        Children();
        ~Children();
        bool has(Name name);
        void add(Layout* layout);
        void remove(Name name);
        Layout* get(Name name);

      private:
        typedef unordered_map<Name, Layout*> Map;

        bool hasMany_;
        union {
            Layout* single_;
            Map* many_;
        };
    };

    Heap<Layout*> parent_;
    int slot_;
    Name name_;
    Children children_;

    bool hasChild(Layout* child);
    void removeChild(Layout* child);
};

#endif
