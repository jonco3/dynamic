#include "layout.h"

int Layout::lookupName(Name name) {
    for (int i = 0; i < names.size(); ++i) {
        if (names[i] == name)
            return i;
    }
    return -1;
}

int Layout::addName(Name name) {
    names.push_back(name);
    return names.size() - 1;
}
