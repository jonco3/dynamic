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
    { ".", Token_Dot },
    { "=", Token_Assign },
    { "(", Token_Bra },
    { ")", Token_Ket },
    { ",", Token_Comma },
    { nullptr, Token_Error }
};

Tokenizer::Tokenizer() :
    source(nullptr)
{
}

size_t Tokenizer::numTypes()
{
    return TokenCount;
}

std::string Tokenizer::typeName(TokenType type)
{
    static const char* names[] = {
#define token_name(name) #name,
        for_each_token(token_name)
#undef token_name
    };
    const size_t len = sizeof(names) / sizeof(const char*);
    assert(len == TokenCount);
    assert(type < len);
    return names[type];
}

void Tokenizer::start(const Input& input)
{
    source = input.text.c_str();
    pos.file = input.file;
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

static bool is_name_start(char c)
{
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_name_rest(char c)
{
    return is_name_start(c) || (c >= '0' && c <= '9');
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
        return {Token_EOF, std::string(), startPos};

    for (const Keyword *k = keywords; k->match; ++k) {
        size_t len = strlen(k->match);
        if (strncmp(k->match, source, len) == 0) {
            nextChar(len);
            return {k->type, std::string(k->match), startPos};
        }
    }

    if (is_digit(peekChar())) {
        do {
            nextChar();
        } while (is_digit(peekChar()));
        size_t length = source - startText;
        return {Token_Number, std::string(startText, length), startPos};
    }

    if (is_name_start(peekChar())) {
        do {
            nextChar();
        } while (is_name_rest(peekChar()));
        size_t length = source - startText;
        return {Token_Name, std::string(startText, length), startPos};
    }

    return {Token_Error, std::string(), startPos};
}

testcase("tokenizer", {
    Tokenizer tz;
    tz.start(Input("", "empty.txt"));
    Token t = tz.nextToken();
    testEqual(t.type, Token_EOF);
    testEqual(t.text, "");
    testEqual(t.pos.file, "empty.txt");
    testEqual(t.pos.line, 1);
    testEqual(t.pos.column, 0);

    tz.start("   ");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("+");
    t = tz.nextToken();
    testEqual(t.type, Token_Plus);
    testEqual(t.text, "+");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("123");
    t = tz.nextToken();
    testEqual(t.type, Token_Number);
    testEqual(t.text, "123");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("name_123");
    t = tz.nextToken();
    testEqual(t.type, Token_Name);
    testEqual(t.text, "name_123");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("1+2 - 3");
    t = tz.nextToken();
    testEqual(t.type, Token_Number);
    testEqual(t.text, "1");
    testEqual(t.pos.column, 0);
    t = tz.nextToken();
    testEqual(t.type, Token_Plus);
    testEqual(t.pos.column, 1);
    t = tz.nextToken();
    testEqual(t.type, Token_Number);
    testEqual(t.text, "2");
    testEqual(t.pos.column, 2);
    t = tz.nextToken();
    testEqual(t.type, Token_Minus);
    testEqual(t.pos.column, 4);
    t = tz.nextToken();
    testEqual(t.type, Token_Number);
    testEqual(t.text, "3");
    testEqual(t.pos.column, 6);
    t = tz.nextToken();
    testEqual(t.type, Token_EOF);
    testEqual(t.pos.column, 7);
    testEqual(t.pos.line, 1);
});
