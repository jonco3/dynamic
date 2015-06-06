#ifndef __SPECIALS_H__
#define __SPECIALS_H__

enum BinaryOp
{
    BinaryPlus,
    BinaryMinus,
    BinaryMultiply,
    BinaryTrueDiv,
    BinaryFloorDiv,
    BinaryModulo,
    BinaryPower,
    BinaryOr,
    BinaryXor,
    BinaryAnd,
    BinaryLeftShift,
    BinaryRightShift,

    CountBinaryOp
};

enum CompareOp
{
    CompareLT,
    CompareLE,
    CompareGT,
    CompareGE,
    CompareEQ,
    CompareNE,

    CountCompareOp
};

extern const char* BinaryOpNames[];
extern const char* BinaryOpMethodNames[];
extern const char* BinaryOpReflectedMethodNames[];
extern const char* AugAssignNames[];
extern const char* AugAssignMethodNames[];

extern const char* CompareOpNames[];
extern const char* CompareOpMethodNames[];

#endif
