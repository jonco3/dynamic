#include "token.h"

#include "test.h"

#include <cassert>
#include <cstring>

struct Keyword
{
    const char* match;
    TokenType type;
};

static const Keyword keywords[] = {
    { "+", Token_Plus },
    { "-", Token_Minus },
    { nullptr, Token_Error }
};

Tokenizer::Tokenizer(const char* source, const char* file) :
    source(source),
    token(nullptr),
    length(0)
{
    assert(source);
    pos.file = file;
    pos.line = 1;
    pos.column = 0;
}

static bool is_whitespace(char c)
{
    return c == '\t' || c == ' ' || c == '\n';
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

void Tokenizer::advance()
{
    char c = *source++;
    assert(c);
    if (c == '\n') {
        ++pos.line;
        pos.column = 0;
    } else {
        ++pos.column;
    }
}

TokenType Tokenizer::nextToken()
{
    pos.column += length;
    while (is_whitespace(*source))
        advance();

    char c = *source;
    if (!c)
        return Token_EOF;

    for (const Keyword *k = keywords; k->match; ++k) {
        size_t len = strlen(k->match);
        if (strncmp(k->match, source, len) == 0) {
            token = source;
            length = len;
            source += len;
            return k->type;
        }
    }

    if (is_digit(c)) {
        token = source;
        while (is_digit(*source))
            ++source;
        length = source - token;
        return Token_Number;
    }

    token = source;
    length = 1;
    return Token_Error;
}

const char* Tokenizer::copyText()
{
    assert(token);
    char *text = static_cast<char *>(malloc(sizeof(char) * (length + 1)));
    if (!text)
        return nullptr;
    memcpy(text, source, sizeof(char) * length);
    text[length] = 0;
    return text;
}

testcase("tokenizer", {
    Tokenizer empty("", "empty.txt");
    testEqual(empty.getPos().file, "empty.txt");
    testEqual(empty.getPos().line, 1);
    testEqual(empty.getPos().column, 0);
    testEqual(empty.nextToken(), Token_EOF);

    Tokenizer space("   ", "");
    testEqual(space.nextToken(), Token_EOF);

    Tokenizer plus("+", "");
    testEqual(plus.nextToken(), Token_Plus);
    testEqual(space.nextToken(), Token_EOF);

    Tokenizer number("123", "");
    testEqual(number.nextToken(), Token_Number);
    testEqual(space.nextToken(), Token_EOF);

    Tokenizer expr("1+2 - 3", "");
    testEqual(expr.nextToken(), Token_Number);
    testEqual(expr.getPos().column, 0);
    testEqual(expr.nextToken(), Token_Plus);
    testEqual(expr.getPos().column, 1);
    testEqual(expr.nextToken(), Token_Number);
    testEqual(expr.getPos().column, 2);
    testEqual(expr.nextToken(), Token_Minus);
    testEqual(expr.getPos().column, 4);
    testEqual(expr.nextToken(), Token_Number);
    testEqual(expr.getPos().column, 6);
    testEqual(space.nextToken(), Token_EOF);
    testEqual(expr.getPos().line, 1);
});
