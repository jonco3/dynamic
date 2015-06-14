#ifndef __SPECIALS_H__
#define __SPECIALS_H__

#define for_each_binary_op(op)                                                \
    op(Plus,       "+",  "__add__",      "__radd__",      "__iadd__")         \
    op(Minus,      "-",  "__sub__",      "__rsub__",      "__isub__")         \
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

enum BinaryOp
{
#define define_enum(name, token, method, rmethod, imethod)                    \
    Binary##name,
    for_each_binary_op(define_enum)
#undef define_enum

    CountBinaryOp
};

#define for_each_compare_op(op)                                               \
    op(LT, "<",  "__lt__")                                                    \
    op(LE, "<=", "__le__")                                                    \
    op(GT, ">",  "__gt__")                                                    \
    op(GE, ">=", "__ge__")                                                    \
    op(EQ, "==", "__eq__")                                                    \
    op(NE, "!=", "__ne__")

enum CompareOp
{
#define define_enum(name, token, method)                                      \
    Compare##name,
    for_each_compare_op(define_enum)
#undef define_enum

    CountCompareOp
};

extern const char* BinaryOpNames[];
extern const char* CompareOpNames[];

#endif
