#include "token.h"

#include "test.h"

#include <cassert>
#include <cstring>
#include <sstream>

struct Keyword
{
    const char* match;
    TokenType type;
};

static const Keyword keywordList[] = {
    { "and", Token_And },
    { "del", Token_Del },
    { "from", Token_From },
    { "not", Token_Not },
    { "while", Token_While },
    { "as", Token_As },
    { "elif", Token_Elif },
    { "global", Token_Global },
    { "or", Token_Or },
    { "with", Token_With },
    { "assert", Token_Assert },
    { "else", Token_Else },
    { "if", Token_If },
    { "pass", Token_Pass },
    { "yield", Token_Yield },
    { "break", Token_Break },
    { "except", Token_Except },
    { "import", Token_Import },
    { "print", Token_Print },
    { "class", Token_Class },
    { "exec", Token_Exec },
    { "in", Token_In },
    { "raise", Token_Raise },
    { "continue", Token_Continue },
    { "finally", Token_Finally },
    { "is", Token_Is },
    { "return", Token_Return },
    { "def", Token_Def },
    { "for", Token_For },
    { "lambda", Token_Lambda },
    { "try", Token_Try },
    { nullptr, Token_EOF }
};

static const Keyword operatorList[] = {
    { "+",  Token_Plus },
    { "-",  Token_Minus },
    { "*",  Token_Times },
    { "**", Token_Power },
    { "/",  Token_Divide },
    { "//", Token_IntDivide },
    { "%",  Token_Modulo },
    { "<<",  Token_BitLeftShift },
    { ">>",  Token_BitRightShift },
    { "&",  Token_BitAnd },
    { "|",  Token_BitOr },
    { "^",  Token_BitXor },
    { "~",  Token_BitNot },
    { "<",  Token_LT },
    { ">",  Token_GT },
    { "<=",  Token_LE },
    { ">=",  Token_GE },
    { "==",  Token_EQ },
    { "!=",  Token_NE },
    { "<>",  Token_NE },
    { "(",  Token_Bra },
    { ")",  Token_Ket },
    { "[",  Token_SBra },
    { "]",  Token_SKet },
    { "{",  Token_OpenBrace },
    { "}",  Token_CloseBrace },
    { "@",  Token_At },
    { ",",  Token_Comma },
    { ":",  Token_Colon },
    { ".",  Token_Period },
    { "`",  Token_Backtick },
    { "=",  Token_Assign },
    { ";",  Token_Semicolon },
    { "+=",  Token_AssignPlus },
    { "-+",  Token_AssignMinus },
    { "*=",  Token_AssignTimes },
    { "/=",  Token_AssignDivide },
    { "//=",  Token_AssignIntDivide },
    { "%=",  Token_AssignModulo },
    { "&=",  Token_AssignBitAnd },
    { "|=",  Token_AssignBitOr },
    { "^=",  Token_AssignBitXor },
    { ">>=",  Token_AssignBitRightShift },
    { "<<=",  Token_AssignBitLeftShift },
    { "**=",  Token_AssignPower },
    { nullptr, Token_EOF }
};

static std::string formatPos(const TokenPos& pos)
{
    std::ostringstream s;
    s << " at " << pos.file << " line " << pos.line << " column " << pos.column;
    return s.str();
}

TokenError::TokenError(std::string message, const TokenPos& pos) :
  runtime_error(message + formatPos(pos))
{
    maybeAbortTests(*this);
}

Tokenizer::Tokenizer() :
  source(nullptr),
  dedentCount(0)
{
    indentStack.push_back(0);

    for (const Keyword *k = keywordList; k->match; ++k)
        keywords.emplace(k->match, k->type);
    for (const Keyword *k = operatorList; k->match; ++k)
        operators.emplace(k->match, k->type);
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
    dedentCount = 0;
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
        } else if (c == '\t') {
            pos.column += 8 - (pos.column % 8);
        } else if (c != '\0') {
            ++pos.column;
        }
    }
}

void Tokenizer::ungetChar(char c)
{
    assert(pos.column > 0);
    assert(c == source[-1]);
    assert(c != '\t' && c != '\n');
    --source;
    --pos.column;
}

static bool is_whitespace(char c)
{
    return c == '\t' || c == ' ';
}

static bool is_newline(char c)
{
    return c == '\n';
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_identifier_start(char c)
{
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_identifier_rest(char c)
{
    return is_identifier_start(c) || (c >= '0' && c <= '9');
}

static bool is_ordinary(char c)
{
    return
        c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '<' ||
        c == '>' || c == '&' || c == '|' || c == '^' || c == '~' || c == '=' ||
        c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
        c == '@' || c == ',' || c == ':' || c == '.' || c == '`' || c == ';';
}

void Tokenizer::skipWhitespace()
{
    while (is_whitespace(peekChar()))
        nextChar();
}

unsigned Tokenizer::skipIndentation()
{
    skipWhitespace();
    while (is_newline(peekChar()))
        skipWhitespace();
    if (!peekChar())
        return 0;
    return pos.column;
}

Token Tokenizer::nextToken()
{
    // Return any queued dedent token
    if (dedentCount) {
        --dedentCount;
        return {Token_Dedent, std::string(), pos};
    }

    TokenPos startPos = pos;
    if (pos.column == 0) {
        // Handle indentation at the start of a line
        unsigned indent = skipIndentation();
        if (indent > indentStack.back()) {
            indentStack.push_back(indent);
            return {Token_Indent, std::string(), pos};
        } else if (indent < indentStack.back()) {
            while (indent < indentStack.back()) {
                indentStack.pop_back();
                ++dedentCount;
            }
            if (indent != indentStack.back())
                throw TokenError("Bad indentation", startPos);
            --dedentCount;
            return {Token_Dedent, std::string(), pos};
        }
    } else {
        skipWhitespace();
    }

    startPos = pos;
    const char *startText = source;

    if (!peekChar())
        return {Token_EOF, std::string(), startPos};

    if (peekChar() == '\n') {
        nextChar();
        return {Token_Newline, std::string(), startPos};
    }

    if (is_digit(peekChar())) {
        do {
            nextChar();
        } while (is_digit(peekChar()));
        size_t length = source - startText;
        return {Token_Number, std::string(startText, length), startPos};
    }

    if (is_identifier_start(peekChar())) {
        do {
            nextChar();
        } while (is_identifier_rest(peekChar()));
        size_t length = source - startText;

        std::string s(startText, length);
        auto k = keywords.find(s);
        if (k != keywords.end())
            return {(*k).second, s, startPos};

        return {Token_Identifier, s, startPos};
    }

    if (is_ordinary(peekChar())) {
        do {
            nextChar();
        } while (is_ordinary(peekChar()));
        size_t length = source - startText;

        // Return the longest valid token we can make out of what we found.
        for (; length >= 1; --length) {
            std::string s(startText, length);
            auto k = operators.find(s);
            if (k != operators.end())
                return {(*k).second, s, startPos};
            ungetChar(s.back());
        }

        throw TokenError("Unexpected " + std::string(startText, 1), startPos);
    }

    throw TokenError("Unexpected " + std::string(startText, 1), startPos);
}

testcase(tokenizer)
{
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
    testEqual(t.type, Token_Identifier);
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

    tz.start("foo()");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Bra);
    testEqual(tz.nextToken().type, Token_Ket);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("foo.bar >>= (2 ** baz)");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Period);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_AssignBitRightShift);
    testEqual(tz.nextToken().type, Token_Bra);
    testEqual(tz.nextToken().type, Token_Number);
    testEqual(tz.nextToken().type, Token_Power);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Ket);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("foo\n  bar\nbaz");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Dedent);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("foo\n  bar\n baz");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testThrows(tz.nextToken(), TokenError);

    tz.start("nand and anda");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_And);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("a<<~3");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_BitLeftShift);
    testEqual(tz.nextToken().type, Token_BitNot);
    testEqual(tz.nextToken().type, Token_Number);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("$");
    testThrows(tz.nextToken(), TokenError);
}
