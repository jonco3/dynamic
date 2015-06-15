#include "src/layout.h"

#include "src/test.h"

testcase(layout)
{
    Stack<Layout*> l1(Layout::Empty->addName("root"));

    testEqual(l1->parent(), Layout::Empty.get());
    testEqual(l1->slotCount(), 1u);
    testEqual(l1->slotIndex(), 0u);
    testEqual(l1->name(), "root");
    testTrue(l1->hasName("root"));
    testEqual(l1->findAncestor("root"), l1);
    testEqual(l1->lookupName("root"), 0);

    Stack<Layout*> l2(l1->addName("a"));
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

    Stack<Layout*> l3(l2->addName("b"));
    testEqual(l3->slotCount(), 3u);
    testTrue(l3->hasName("a"));
    testTrue(l3->hasName("b"));
}
