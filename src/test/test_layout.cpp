#include "src/layout.h"

#include "src/test.h"

testcase(layout)
{
    Root<Layout*> l1(gc.create<Layout>(Layout::None, "root"));

    testEqual(l1->parent(), Layout::None.get());
    testEqual(l1->slotCount(), 1u);
    testEqual(l1->slotIndex(), 0u);
    testEqual(l1->name(), "root");
    testTrue(l1->hasName("root"));
    testEqual(l1->findAncestor("root"), l1);
    testEqual(l1->lookupName("root"), 0);

    Root<Layout*> l2(l1->addName("a"));
    testEqual(l2->parent(), l1);
    testEqual(l2->slotCount(), 2u);
    testEqual(l2->slotIndex(), 1u);
    testEqual(l2->name(), "a");
    testTrue(l2->hasName("a"));
    testTrue(l2->hasName("root"));
    testEqual(l2->findAncestor("a"), l2);
    testEqual(l2->findAncestor("root"), l1);
    testEqual(l2->lookupName("a"), 1);
    testEqual(l2->lookupName("root"), 0);

    Root<Layout*> l3(l2->addName("b"));
    testEqual(l3->slotCount(), 3u);
    testTrue(l3->hasName("a"));
    testTrue(l3->hasName("b"));
}