#include "specials.h"

const char* BinaryOpNames[] = {
    "+",
    "-",
    "*",
    "/",
    "//",
    "%",
    "**",
    "|",
    "^",
    "&",
    "<<",
    ">>"
};
static_assert(sizeof(BinaryOpNames) / sizeof(*BinaryOpNames) == CountBinaryOp,
              "Number of names must match number of binary operations");

const char* BinaryOpMethodNames[] = {
    "__add__",
    "__sub__",
    "__mul__",
    "__div__",
    "__floordiv__",
    "__mod__",
    "__pow__",
    "__or__",
    "__xor__",
    "__and__",
    "__lshift__",
    "__rshift__"
};
static_assert(sizeof(BinaryOpMethodNames) / sizeof(*BinaryOpMethodNames) == CountBinaryOp,
              "Number of method names must match number of binary operations");

const char* BinaryOpReflectedMethodNames[] = {
    "__radd__",
    "__rsub__",
    "__rmul__",
    "__rdiv__",
    "__rfloordiv__",
    "__rmod__",
    "__rpow__",
    "__ror__",
    "__rxor__",
    "__rand__",
    "__rlshift__",
    "__rrshift__"
};
static_assert(sizeof(BinaryOpReflectedMethodNames) / sizeof(*BinaryOpReflectedMethodNames) == CountBinaryOp,
              "Number of method names must match number of binary operations");

const char* AugAssignMethodNames[] = {
    "__iadd__",
    "__isub__",
    "__imul__",
    "__idiv__",
    "__ifloordiv__",
    "__imod__",
    "__ipow__",
    "__ior__",
    "__ixor__",
    "__iand__",
    "__ilshift__",
    "__irshift__"
};
static_assert(sizeof(AugAssignMethodNames) / sizeof(*AugAssignMethodNames) == CountBinaryOp,
              "Number of method names must match number of binary operations");

const char* CompareOpNames[] = {
    "<", "<=", ">", ">=", "==", "!="
};
static_assert(sizeof(CompareOpNames) / sizeof(*CompareOpNames) == CountCompareOp,
              "Number of names must match number of compare operations");
