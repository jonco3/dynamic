#ifndef __TOKEN_H__
#define __TOKEN_H__

#include "input.h"

#include <ostream>

enum TokenType
{
    Token_Error,
    Token_EOF,
    Token_Number,
    Token_Name,
    Token_Plus,
    Token_Minus,
    Token_Dot,
    Token_Assign,
    Token_Bra,
    Token_Ket,
    Token_Comma,

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

    char peekChar();
    void nextChar(size_t count = 1);
    void skipWhitespace();
};

/*
 * To build a generic tokenizer framwork, we have a starting state, a set of
 * intermediate states and a set of final states corresponding to token types.
 * We can easily configure these at runtime by supplying predicate function for
 * the state transitions.
 */

#endif
