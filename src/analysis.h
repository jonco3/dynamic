#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

#include "layout.h"
#include "name.h"
#include "syntax.h"

extern Object* FindDefinitions(const Syntax& s,
                               Traced<Layout*> layout,
                               vector<Name>& globalsOut,
                               bool& hasNestedFuctionsOut);

#endif
