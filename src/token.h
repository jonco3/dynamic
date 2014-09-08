#ifndef __TOKEN_H__
#define __TOKEN_H__

#include <cstdlib>

enum TokenType
{
    Token_Error,
    Token_EOF,
    Token_Number,
    Token_Plus,
    Token_Minus
};

struct TokenPos
{
    const char* file;
    unsigned line;
    unsigned column;
};

struct Tokenizer
{
    Tokenizer(const char* source, const char* file);

    TokenType nextToken();
    const TokenPos& getPos() { return pos; }
    const char* copyText();

private:
    const char* source;
    const char* token;
    size_t length;
    TokenPos pos;

    void advance();
};

#endif
