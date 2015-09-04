#ifndef __SPECIALS_H__
#define __SPECIALS_H__

#define for_each_binary_op(op)                                                \
    op(Add,        "+",  "__add__",      "__radd__",      "__iadd__")         \
    op(Sub,        "-",  "__sub__",      "__rsub__",      "__isub__")         \
    op(Multiply,   "*",  "__mul__",      "__rmul__",      "__imul__")         \
    op(TrueDiv,    "/",  "__truediv__",  "__rtruediv__",  "__itruediv__")     \
    op(FloorDiv,   "//", "__floordiv__", "__rfloordiv__", "__ifloordiv__")    \
    op(Modulo,     "%" , "__mod__",      "__rmod__",      "__imod__")         \
    op(Power,      "**", "__pow__",      "__rpow__",      "__ipow__")         \
    op(Or,         "|",  "__or__",       "__ror__",       "__ior__")          \
    op(Xor,        "^",  "__xor__",      "__rxor__",      "__ixor__")         \
    op(And,        "&",  "__and__",      "__rand__",      "__iand__")         \
    op(LeftShift,  "<<", "__lshift__",   "__rlshift__",   "__ilshift__")      \
    op(RightShift, ">>", "__rshift__",   "__rrshift__",   "__irshift__")

#define for_each_compare_op(op)                                               \
    op(LT, "<",  "__lt__", "__gt__")                                          \
    op(LE, "<=", "__le__", "__ge__")                                          \
    op(GT, ">",  "__gt__", "__lt__")                                          \
    op(GE, ">=", "__ge__", "__le__")                                          \
    op(EQ, "==", "__eq__", "__eq__")                                           \
    op(NE, "!=", "__ne__", "__ne__")

enum BinaryOp
{
#define define_enum(name, token, method, rmethod, imethod)                    \
    Binary##name,
    for_each_binary_op(define_enum)
#undef define_enum

    CountBinaryOp
};

enum CompareOp
{
#define define_enum(name, token, method, rmethod)                             \
    Compare##name,
    for_each_compare_op(define_enum)
#undef define_enum

    CountCompareOp
};

extern const char* BinaryOpNames[];
extern const char* CompareOpNames[];

#endif
