#ifndef __TOKEN_H__
#define __TOKEN_H__

#include "input.h"

#include <list>
#include <ostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

using namespace std;

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
    token(NotIn)                                                              \
    token(Raise)                                                              \
    token(Continue)                                                           \
    token(Finally)                                                            \
    token(Is)                                                                 \
    token(IsNot)                                                              \
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
    token(Integer)                                                            \
    token(String)                                                             \
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
    string file;
    unsigned line;
    unsigned column;
};

struct Token
{
    TokenType type;
    string text;
    TokenPos pos;
};

struct TokenError : public runtime_error
{
    TokenError(string message, const TokenPos& pos);
};

struct Tokenizer
{
    Tokenizer();
    size_t numTypes();
    string typeName(TokenType type);

    void start(const Input& input);
    Token nextToken();

private:
    const char* source;
    TokenPos pos;
    vector<unsigned> indentStack;
    list<Token> tokenQueue;
    unordered_map<string, TokenType> keywords;
    unordered_map<string, TokenType> operators;

    char peekChar();
    char nextChar();
    void ungetChar(char c);

    void skipWhitespace();
    unsigned skipIndentation();

    bool isWhitespace(char c);
    bool isNewline(char c);
    bool isDigit(char c);
    bool isIdentifierStart(char c);
    bool isIdentifierRest(char c);
    bool isOrdinary(char c);
};

/*
 * To build a generic tokenizer framwork, we have a starting state, a set of
 * intermediate states and a set of final states corresponding to token types.
 * We can easily configure these at runtime by supplying predicate function for
 * the state transitions.
 */

#endif
