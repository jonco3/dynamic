#include "value.h"
#include "object.h"

std::ostream& operator<<(std::ostream& s, const Value& v) {
    s << v.asObject();
    return s;
}
