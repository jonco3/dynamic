#include "value.h"
#include "object.h"

ostream& operator<<(ostream& s, const Value& v) {
    s << v.asObject();
    return s;
}
