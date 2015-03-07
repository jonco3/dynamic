#ifndef __SPECIALS_H__
#define __SPECIALS_H__

enum BinaryOp
{
    BinaryPlus,
    BinaryMinus,
    BinaryMultiply,
    BinaryDivide,
    BinaryIntDivide,
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
extern const char* AugAssignMethodNames[];

extern const char* CompareOpNames[];

#endif
