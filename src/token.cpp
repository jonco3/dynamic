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
    source(source)
{
    assert(source);
    pos.file = file;
    pos.line = 1;
    pos.column = 0;
}

size_t Tokenizer::numTypes()
{
    return TokenCount;
}

std::string Tokenizer::typeName(TokenType type)
{
    static const char* names[] = {
        "Error",
        "EOF",
        "Number",
        "Plus",
        "Minus"
    };
    const size_t len = sizeof(names) / sizeof(const char*);
    assert(type < len);
    return names[type];
}

static bool is_whitespace(char c)
{
    return c == '\t' || c == ' ' || c == '\n';
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

char Tokenizer::peekChar()
{
    return *source;
}

void Tokenizer::nextChar(size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        char c = *source++;
        if (c == '\n') {
            ++pos.line;
            pos.column = 0;
        } else {
            ++pos.column;
        }
    }
}

void Tokenizer::skipWhitespace()
{
    while (is_whitespace(*source))
        nextChar();
}

Token Tokenizer::nextToken()
{
    skipWhitespace();

    TokenPos startPos = pos;
    const char *startText = source;

    if (!peekChar())
        return Token({Token_EOF, std::string(), startPos});

    for (const Keyword *k = keywords; k->match; ++k) {
        size_t len = strlen(k->match);
        if (strncmp(k->match, source, len) == 0) {
            nextChar(len);
            return Token({k->type, std::string(k->match), startPos});
        }
    }

    if (is_digit(peekChar())) {
        do {
            nextChar();
        } while (is_digit(peekChar()));
        size_t length = source - startText;
        return Token({Token_Number, std::string(startText, length), startPos});
    }

    return Token({Token_Error, std::string(), startPos});
}

testcase("tokenizer", {
    Tokenizer empty("", "empty.txt");
    Token t = empty.nextToken();
    testEqual(t.type, Token_EOF);
    testEqual(t.text, "");
    testEqual(t.pos.file, "empty.txt");
    testEqual(t.pos.line, 1);
    testEqual(t.pos.column, 0);

    Tokenizer space("   ", "");
    testEqual(space.nextToken().type, Token_EOF);

    Tokenizer plus("+", "");
    t = plus.nextToken();
    testEqual(t.type, Token_Plus);
    testEqual(t.text, "+");
    testEqual(plus.nextToken().type, Token_EOF);

    Tokenizer number("123", "");
    t = number.nextToken();
    testEqual(t.type, Token_Number);
    testEqual(t.text, "123");
    testEqual(number.nextToken().type, Token_EOF);

    Tokenizer expr("1+2 - 3", "");
    t = expr.nextToken();
    testEqual(t.type, Token_Number);
    testEqual(t.pos.column, 0);
    t = expr.nextToken();
    testEqual(t.type, Token_Plus);
    testEqual(t.pos.column, 1);
    t = expr.nextToken();
    testEqual(t.type, Token_Number);
    testEqual(t.pos.column, 2);
    t = expr.nextToken();
    testEqual(t.type, Token_Minus);
    testEqual(t.pos.column, 4);
    t = expr.nextToken();
    testEqual(t.type, Token_Number);
    testEqual(t.pos.column, 6);
    t = expr.nextToken();
    testEqual(t.type, Token_EOF);
    testEqual(t.pos.column, 7);
    testEqual(t.pos.line, 1);
});
