#ifndef __TOKEN_H__
#define __TOKEN_H__

#include <string>
#include <ostream>

enum TokenType
{
    Token_Error,
    Token_EOF,
    Token_Number,
    Token_Plus,
    Token_Minus,

    TokenCount
};

struct TokenPos
{
    const char* file;
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
    Tokenizer(const char* source, const char* file);
    size_t numTypes();
    std::string typeName(TokenType type);

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
