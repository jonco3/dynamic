#include "token.h"

#include "test.h"

#include <cassert>
#include <cstring>
#include <sstream>

//#define TRACE_TOKEN

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
    { "nonlocal", Token_NonLocal },
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
//    { "print", Token_Print },  todo
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
    { "{",  Token_CBra },
    { "}",  Token_CKet },
    { "@",  Token_At },
    { ",",  Token_Comma },
    { ":",  Token_Colon },
    { ".",  Token_Period },
    { "`",  Token_Backtick },
    { "=",  Token_Assign },
    { ";",  Token_Semicolon },
    { "+=",  Token_AssignPlus },
    { "-=",  Token_AssignMinus },
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

static string formatPos(const TokenPos& pos)
{
    ostringstream s;
    s << " at " << pos.file << " line " << pos.line << " column " << pos.column;
    return s.str();
}

TokenError::TokenError(string message, const TokenPos& pos) :
  runtime_error(message + formatPos(pos))
{
#ifdef BUILD_TESTS
    maybeAbortTests("TokenError " + message);
#endif
}

Tokenizer::Tokenizer() :
  index(0)
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

string Tokenizer::typeName(TokenType type)
{
    static const char* names[] = {
#define token_name(name) #name,
        for_each_token(token_name)
#undef token_name
    };
#ifdef DEBUG
    const size_t len = sizeof(names) / sizeof(const char*);
    assert(len == TokenCount);
    assert(type < len);
#endif
    return names[type];
}

void Tokenizer::start(const Input& input)
{
    source = input.text;
    index = 0;
    pos.file = input.file;
    pos.line = 1;
    pos.column = 0;
    tokenQueue.clear();
}

bool Tokenizer::atEnd()
{
    assert(index <= source.size());
    return index == source.size();
}

char Tokenizer::peekChar()
{
    if (atEnd())
        return '\0';

    return source[index];
}

char Tokenizer::nextChar()
{
    char c = peekChar();
    index++;
    if (c == '\n') {
        ++pos.line;
        pos.column = 0;
    } else if (c == '\t') {
        pos.column += 8 - (pos.column % 8);
    } else if (c != '\0') {
        ++pos.column;
    }
    return c;
}

void Tokenizer::ungetChar(char c)
{
    assert(index != 0);
    assert(c == source[index - 1]);
    assert(c != '\t' && c != '\n');
    --index;
    --pos.column;
}

bool Tokenizer::isWhitespace(char c)
{
    return c == '\t' || c == ' ';
}

bool Tokenizer::isNewline(char c)
{
    return c == '\n';
}

bool Tokenizer::isDigit(char c)
{
    return c >= '0' && c <= '9';
}

bool Tokenizer::isIdentifierStart(char c)
{
    return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool Tokenizer::isIdentifierRest(char c)
{
    return isIdentifierStart(c) || isDigit(c);
}

bool Tokenizer::isOperatorOrDelimiter(char c)
{
    return
        c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '<' ||
        c == '>' || c == '&' || c == '|' || c == '^' || c == '~' || c == '=' ||
        c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
        c == '@' || c == ',' || c == ':' || c == '.' || c == '`' || c == ';' ||
        c == '!';
}

TokenType Tokenizer::isOpeningBracket(char c)
{
    switch (c) {
      case '(': return Token_Bra;
      case '[': return Token_SBra;
      case '{': return Token_CBra;
      default:  return Token_EOF;
    }
}

TokenType Tokenizer::isClosingBracket(char c)
{
    switch (c) {
      case ')': return Token_Ket;
      case ']': return Token_SKet;
      case '}': return Token_CKet;
      default:  return Token_EOF;
    }
}

void Tokenizer::skipWhitespace()
{
    while (isWhitespace(peekChar()))
        nextChar();
    if (peekChar() == '#') {
        do
            nextChar();
        while (!atEnd() && !isNewline(peekChar()));
    }
}

unsigned Tokenizer::skipIndentation()
{
    skipWhitespace();
    while (isNewline(peekChar())) {
        nextChar();
        skipWhitespace();
    }
    if (index >= source.size())
        return 0;
    return pos.column;
}

template <typename T>
T popFront(list<T>& list)
{
    assert(!list.empty());
    T result = list.front();
    list.pop_front();
    return result;
}

Token Tokenizer::nextToken()
{
    Token result = findNextToken();
#ifdef TRACE_TOKEN
    cerr << "nextToken " << typeName(t) << endl;
#endif
    return result;
}

void Tokenizer::queueDedentsToColumn(unsigned column)
{
    while (column < indentStack.back()) {
        indentStack.pop_back();
        tokenQueue.push_back({Token_Dedent, string(), pos});
    }
}

bool Tokenizer::matchExponent()
{
    char c = peekChar();
    if (c != 'e' && c != 'E')
        return false;

    nextChar();
    c = peekChar();
    if (c == '+' || c == '-')
        nextChar();
    while (isDigit(peekChar()))
        nextChar();
    return true;
}

Token Tokenizer::findNextToken()
{
    // Return any queued tokens
    if (!tokenQueue.empty()) {
        return popFront(tokenQueue);
    }

    // Whitespace
    TokenPos startPos = pos;
    if (pos.column == 0) {
        // Handle indentation at the start of a line
        unsigned indent = skipIndentation();
        if (indent > indentStack.back()) {
            indentStack.push_back(indent);
            return {Token_Indent, string(), pos};
        } else if (indent < indentStack.back()) {
            queueDedentsToColumn(indent);
            if (indent != indentStack.back())
                throw TokenError("Bad indentation", startPos);
            return popFront(tokenQueue);
        }
    } else {
        skipWhitespace();
        if (!bracketStack.empty())
            skipIndentation();
    }

    startPos = pos;
    unsigned startIndex = index;

    // EOF
    if (index >= source.size()) {
        if (indentStack.back() != 0) {
            // No newline after indented block - fake one and follow with dedent
            // tokens
            assert(pos.column > 0);
            queueDedentsToColumn(0);
            return {Token_Newline, string(), pos};
        }
        return {Token_EOF, string(), startPos};
    }

    // Newline
    if (peekChar() == '\n') {
        nextChar();
        return {Token_Newline, string(), startPos};
    }

    // Numeric literals
    bool hasIntPart = false;
    if (isDigit(peekChar())) {
        // todo: floating pointer, prefixes, etc.
        do {
            nextChar();
        } while (isDigit(peekChar()));
        if (peekChar() != '.') {
            if (matchExponent()) {
                string text(source, startIndex, index - startIndex);
                return {Token_Float, text, startPos};
            }
            string text(source, startIndex, index - startIndex);
            return {Token_Integer, text, startPos};
        }
        hasIntPart = true;
    }
    if (peekChar() == '.') {
        nextChar();
        if (!hasIntPart && !isDigit(peekChar())) {
            ungetChar('.');
        } else {
            while (isDigit(peekChar()))
                nextChar();
            matchExponent();
            string text(source, startIndex, index - startIndex);
            return {Token_Float, text, startPos};
        }
    }

    // String literals
    if (peekChar() == '\'' || peekChar() == '"') {
        // todo: r u and b prefixes, long strings
        char delimiter = nextChar();
        ostringstream s;
        char c;
        while (c = nextChar(), c && c != delimiter) {
            if (c != '\\') {
                s << c;
            } else {
                char escape = nextChar();
                if (!escape)
                    break;
                if (escape == 'n')
                    s << "\n";
                else if (escape == '\t')
                    s << "\t";
                else
                    throw TokenError("Unimplimented string escape", pos);
            }
        }
        if (c != delimiter)
            throw TokenError("Unterminated string", pos);
        return {Token_String, s.str(), startPos};
    }

    // Identifiers and keywords
    if (isIdentifierStart(peekChar())) {
        do {
            nextChar();
        } while (isIdentifierRest(peekChar()));
        size_t length = index - startIndex;

        string s(source, startIndex, length);
        auto k = keywords.find(s);
        if (k != keywords.end()) {
            TokenType type = (*k).second;

            // Special cases to make "not in" and "is not" single tokens
            if (type == Token_Not || type == Token_Is) {
                Token next = nextToken();
                if (type == Token_Not and next.type == Token_In)
                    type = Token_NotIn;
                else if (type == Token_Is and next.type == Token_Not)
                    type = Token_IsNot;
                else
                    tokenQueue.push_front(next);
            }

            return {type, s, startPos};
        }

        return {Token_Identifier, s, startPos};
    }

    // Brackets
    assert(!Token_EOF);
    if (TokenType type = isOpeningBracket(peekChar())) {
        nextChar();
        bracketStack.push_back(type);
        return {type, string(source, startIndex, 1), startPos};
    } else if (TokenType type = isClosingBracket(peekChar())) {
        nextChar();
        assert(Token_Ket == Token_Bra + 1);
        assert(Token_SKet == Token_SBra + 1);
        assert(Token_CKet == Token_CBra + 1);
        TokenType opening = TokenType(type - 1);
        if (bracketStack.empty() || bracketStack.back() != opening)
            throw TokenError("Mismatched bracket", startPos);
        bracketStack.pop_back();
        return {type, string(source, startIndex, 1), startPos};
    }

    // Operators and delimiters
    if (isOperatorOrDelimiter(peekChar())) {
        do {
            nextChar();
        } while (isOperatorOrDelimiter(peekChar()));
        size_t length = index - startIndex;

        // Return the longest valid token we can make out of what we found.
        for (; length >= 1; --length) {
            string s(source, startIndex, length);
            auto k = operators.find(s);
            if (k != operators.end())
                return {(*k).second, s, startPos};
            ungetChar(s.back());
        }

        throw TokenError("Unexpected " + string(source, startIndex, 1), startPos);
    }

    throw TokenError("Unexpected " + string(source, startIndex, 1), startPos);
}

#ifdef BUILD_TESTS

static void tokenize(const char *source)
{
    Tokenizer tz;
    tz.start(source);
    while (tz.nextToken().type != Token_EOF)
        ;
}

testcase(tokenizer)
{
    Tokenizer tz;
    tz.start(Input("", "empty.txt"));
    Token t = tz.nextToken();
    testEqual(t.type, Token_EOF);
    testEqual(t.text, "");
    testEqual(t.pos.file, "empty.txt");
    testEqual(t.pos.line, 1u);
    testEqual(t.pos.column, 0u);

    tz.start("   ");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("+");
    t = tz.nextToken();
    testEqual(t.type, Token_Plus);
    testEqual(t.text, "+");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("!=");
    t = tz.nextToken();
    testEqual(t.type, Token_NE);
    testEqual(t.text, "!=");

    tz.start("123");
    t = tz.nextToken();
    testEqual(t.type, Token_Integer);
    testEqual(t.text, "123");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start(".5"); // fraction
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, ".5");

    tz.start("1.5"); // intpart fraction
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, "1.5");

    tz.start("1."); // intpart .
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, "1.");

    tz.start("1e2"); // intpart exponent
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, "1e2");

    tz.start(".5E2"); // fraction exponent
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, ".5E2");

    tz.start("1.5e-1"); // intpart fraction exponent
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, "1.5e-1");

    tz.start("1.e+3"); // intpart . exponent
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, "1.e+3");

    tz.start("name_123");
    t = tz.nextToken();
    testEqual(t.type, Token_Identifier);
    testEqual(t.text, "name_123");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("1+2 - 3");
    t = tz.nextToken();
    testEqual(t.type, Token_Integer);
    testEqual(t.text, "1");
    testEqual(t.pos.column, 0u);
    t = tz.nextToken();
    testEqual(t.type, Token_Plus);
    testEqual(t.pos.column, 1u);
    t = tz.nextToken();
    testEqual(t.type, Token_Integer);
    testEqual(t.text, "2");
    testEqual(t.pos.column, 2u);
    t = tz.nextToken();
    testEqual(t.type, Token_Minus);
    testEqual(t.pos.column, 4u);
    t = tz.nextToken();
    testEqual(t.type, Token_Integer);
    testEqual(t.text, "3");
    testEqual(t.pos.column, 6u);
    t = tz.nextToken();
    testEqual(t.type, Token_EOF);
    testEqual(t.pos.column, 7u);
    testEqual(t.pos.line, 1u);

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
    testEqual(tz.nextToken().type, Token_Integer);
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

    tz.start("foo\n\n    \nbaz");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("nand and anda");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_And);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("a<<~3");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_BitLeftShift);
    testEqual(tz.nextToken().type, Token_BitNot);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_EOF);

    testThrows(tokenize("$"), TokenError);

    testThrows(tokenize("'string\""), TokenError);
    testThrows(tokenize("\"string'"), TokenError);

    tz.start("'$tring'");
    testEqual(tz.nextToken().type, Token_String);

    tz.start("\"gnirts\"");
    testEqual(tz.nextToken().type, Token_String);

    tz.start("is is not not not in in");
    testEqual(tz.nextToken().type, Token_Is);
    testEqual(tz.nextToken().type, Token_IsNot);
    testEqual(tz.nextToken().type, Token_Not);
    testEqual(tz.nextToken().type, Token_NotIn);
    testEqual(tz.nextToken().type, Token_In);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("if 1:\n"
             "  return 2\n");
    testEqual(tz.nextToken().type, Token_If);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Colon);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Return);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Dedent);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("if 0:\n"
             "  return 2\n"
             "else:\n"
             "  return 3\n");
    testEqual(tz.nextToken().type, Token_If);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Colon);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Return);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Dedent);
    testEqual(tz.nextToken().type, Token_Else);
    testEqual(tz.nextToken().type, Token_Colon);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Return);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Dedent);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("white   \nspace");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("white  \n"
             "  space");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Dedent);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("(   \n    )");
    testEqual(tz.nextToken().type, Token_Bra);
    testEqual(tz.nextToken().type, Token_Ket);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("[   # comment\n    ]");
    testEqual(tz.nextToken().type, Token_SBra);
    testEqual(tz.nextToken().type, Token_SKet);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("{\n\n\n}");
    testEqual(tz.nextToken().type, Token_CBra);
    testEqual(tz.nextToken().type, Token_CKet);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("# comment");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("foo # comment");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("foo # comment\nbar");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("foo # comment\n"
             "    # comment\n"
             "  bar");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Dedent);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("(1, .3 + 1)");
    testEqual(tz.nextToken().type, Token_Bra);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Comma);
    testEqual(tz.nextToken().type, Token_Float);
    testEqual(tz.nextToken().type, Token_Plus);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Ket);
    testEqual(tz.nextToken().type, Token_EOF);
}

#endif
