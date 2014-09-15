#ifndef __LAYOUT_H__
#define __LAYOUT_H__

#include "name.h"

#include <vector>

struct Layout
{
    // todo: eventually we will need layout lineage a.k.a shape tree
    std::vector<Name> names;
    int lookupName(Name name);
    int addName(Name name);
};

#endif
