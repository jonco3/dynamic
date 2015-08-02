#include "specials.h"

const char* BinaryOpNames[] = {
#define define_name(name, token, method, rmethod, imethod)                    \
    token,

    for_each_binary_op(define_name)

#undef define_enum
};
static_assert(sizeof(BinaryOpNames) / sizeof(*BinaryOpNames) == CountBinaryOp,
              "Number of names must match number of binary operations");

const char* CompareOpNames[] = {
#define define_enum(name, token, method, rmethod)                             \
    token,

    for_each_compare_op(define_enum)

#undef define_enum
};
static_assert(sizeof(CompareOpNames) / sizeof(*CompareOpNames) == CountCompareOp,
              "Number of names must match number of compare operations");
