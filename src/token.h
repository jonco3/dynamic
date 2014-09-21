#ifndef __TOKEN_H__
#define __TOKEN_H__

#include "input.h"

#include <ostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#define for_each_token(token)                                                 \
    token(EOF)                                                                \
                                                                              \
    token(Newline)                                                            \
    token(Indent)                                                             \
    token(Dedent)                                                             \
                                                                              \
    token(And)                                                                \
    token(Del)                                                                \
    token(From)                                                               \
    token(Not)                                                                \
    token(While)                                                              \
    token(As)                                                                 \
    token(Elif)                                                               \
    token(Global)                                                             \
    token(Or)                                                                 \
    token(With)                                                               \
    token(Assert)                                                             \
    token(Else)                                                               \
    token(If)                                                                 \
    token(Pass)                                                               \
    token(Yield)                                                              \
    token(Break)                                                              \
    token(Except)                                                             \
    token(Import)                                                             \
    token(Print)                                                              \
    token(Class)                                                              \
    token(Exec)                                                               \
    token(In)                                                                 \
    token(Raise)                                                              \
    token(Continue)                                                           \
    token(Finally)                                                            \
    token(Is)                                                                 \
    token(Return)                                                             \
    token(Def)                                                                \
    token(For)                                                                \
    token(Lambda)                                                             \
    token(Try)                                                                \
                                                                              \
    token(Plus)                                                               \
    token(Minus)                                                              \
    token(Times)                                                              \
    token(Power)                                                              \
    token(Divide)                                                             \
    token(IntDivide)                                                          \
    token(Modulo)                                                             \
    token(BitLeftShift)                                                       \
    token(BitRightShift)                                                      \
    token(BitAnd)                                                             \
    token(BitOr)                                                              \
    token(BitXor)                                                             \
    token(BitNot)                                                             \
    token(LT)                                                                 \
    token(GT)                                                                 \
    token(LE)                                                                 \
    token(GE)                                                                 \
    token(EQ)                                                                 \
    token(NE)                                                                 \
    token(Bra)                                                                \
    token(Ket)                                                                \
    token(SBra)                                                               \
    token(SKet)                                                               \
    token(OpenBrace)                                                          \
    token(CloseBrace)                                                         \
    token(At)                                                                 \
    token(Comma)                                                              \
    token(Colon)                                                              \
    token(Period)                                                             \
    token(Backtick)                                                           \
    token(Assign)                                                             \
    token(Semicolon)                                                          \
    token(AssignPlus)                                                         \
    token(AssignMinus)                                                        \
    token(AssignTimes)                                                        \
    token(AssignDivide)                                                       \
    token(AssignIntDivide)                                                    \
    token(AssignModulo)                                                       \
    token(AssignBitAnd)                                                       \
    token(AssignBitOr)                                                        \
    token(AssignBitXor)                                                       \
    token(AssignBitRightShift)                                                \
    token(AssignBitLeftShift)                                                 \
    token(AssignPower)                                                        \
                                                                              \
    token(Number)                                                             \
    token(Identifier)

enum TokenType
{
#define token_enum(name) Token_##name,
    for_each_token(token_enum)
#undef token_enum

    TokenCount
};

struct TokenPos
{
    std::string file;
    unsigned line;
    unsigned column;
};

struct Token
{
    TokenType type;
    std::string text;
    TokenPos pos;
};

struct TokenError : public std::runtime_error
{
    TokenError(std::string message, const TokenPos& pos);
};

struct Tokenizer
{
    Tokenizer();
    size_t numTypes();
    std::string typeName(TokenType type);

    void start(const Input& input);
    Token nextToken();

private:
    const char* source;
    TokenPos pos;
    std::vector<unsigned> indentStack;
    unsigned dedentCount;
    std::unordered_map<std::string, TokenType> keywords;
    std::unordered_map<std::string, TokenType> operators;

    char peekChar();
    void nextChar(size_t count = 1);
    void ungetChar(char c);

    void skipWhitespace();
    unsigned skipIndentation();
};

/*
 * To build a generic tokenizer framwork, we have a starting state, a set of
 * intermediate states and a set of final states corresponding to token types.
 * We can easily configure these at runtime by supplying predicate function for
 * the state transitions.
 */

#endif
