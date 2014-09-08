#ifndef __PARSER_H__
#define __PARSER_H__

#include "token.h"
#include "assert.h"

#include <vector>
#include <stdexcept>
#include <functional>

template <typename T>
struct Parser;

enum Assoc
{
    Assoc_Left,
    Assoc_Right
};

template <typename T>
struct ExprSpec
{
    ExprSpec(unsigned maxTokenTypes);

    typedef std::function<T (Parser<T>& parser, ExprSpec<T> spec, Token token)>
                PrefixHandler;
    void addPrefixHandler(TokenType type, PrefixHandler handler);

    typedef std::function<T (Parser<T>& parser, ExprSpec<T> spec, Token token,
                             const T leftValue)> InfixHandler;
    void addInfixHandler(TokenType type, unsigned bindLeft, InfixHandler handler);

    typedef std::function<T (Token token)> WordHandler;
    void addWord(TokenType type, WordHandler handler);

    typedef std::function<T (Token token, const T leftValue, const T rightValue)>
                BinaryOpHandler;
    void addBinaryOp(TokenType type, unsigned bindLeft, Assoc assoc,
                     BinaryOpHandler handler);

    typedef std::function<T (Token token, const T rightValue)> UnaryOpHandler;
    void addUnaryOp(TokenType type, UnaryOpHandler hanlder);

  private:
    std::vector<PrefixHandler> prefixActions;
    std::vector<std::pair<unsigned, InfixHandler> > infixActions;

    friend struct Parser<T>;
};

struct ParseError : public std::runtime_error
{
    ParseError(std::string message);
};

template <typename T>
struct Parser
{
    Parser(const ExprSpec<T>& spec, Tokenizer& tokenizer);

    bool atEnd() { return token.type == Token_EOF; }
    T parse() { return expression(topSpec); }

    bool notFollowedBy(TokenType type) { return token.type != type; }
    bool opt(TokenType type, Token& tokenOut);
    Token match(TokenType type);
    T expression(const ExprSpec<T>& spec, unsigned bindRight = 0);

    void nextToken();
    T prefix(const ExprSpec<T>& spec, TokenType type);
    unsigned getBindLeft(const ExprSpec<T>& spec, TokenType type);
    T infix(const ExprSpec<T>& spec, TokenType type, const T leftValue);

  private:
    const ExprSpec<T>& topSpec;
    Tokenizer& tokenizer;
    Token token;
};

template <typename T>
ExprSpec<T>::ExprSpec(unsigned maxTokenTypes)
{
    prefixActions.resize(maxTokenTypes);
    infixActions.resize(maxTokenTypes);
}

template <typename T>
void ExprSpec<T>::addPrefixHandler(TokenType type, PrefixHandler handler)
{
    assert(type < prefixActions.size());
    prefixActions[type] = handler;
}

template <typename T>
void ExprSpec<T>::addInfixHandler(TokenType type, unsigned bindLeft,
                                  InfixHandler handler)
{
    assert(type < infixActions.size());
    infixActions[type] = std::pair<unsigned, InfixHandler>(bindLeft, handler);
}

template <typename T>
void ExprSpec<T>::addWord(TokenType type, WordHandler handler)
{
    addPrefixHandler(type,
                     [=] (Parser<T>& parser, ExprSpec<T> spec, Token token) {
                         return handler(token);
                     });
}

template <typename T>
void ExprSpec<T>::addBinaryOp(TokenType type, unsigned bindLeft, Assoc assoc,
                 BinaryOpHandler handler) {
    // calculate right binding power by addng left binding power and
    // associativity, hence why left binding power must be a multiple
    // of two
    unsigned bindRight = bindLeft + assoc;
    addInfixHandler(type,
                    bindLeft,
                    [=] (Parser<T>& parser, ExprSpec<T> spec, Token token,
                         const T leftValue) {
                        T rightValue = parser.expression(spec, bindRight);
                        return handler(token, leftValue, rightValue);
                    });
}

template <typename T>
void ExprSpec<T>::addUnaryOp(TokenType type, UnaryOpHandler handler)
{
    addPrefixHandler(type,
                     [=] (Parser<T>& parser, ExprSpec<T> spec, Token token) {
                         T rightValue = parser.expression(spec, 100);
                         return handler(token, rightValue);
                     });
}

ParseError::ParseError(std::string message) :
  runtime_error(message)
{}

template <typename T>
Parser<T>::Parser(const ExprSpec<T>& spec, Tokenizer& tokenizer) :
  topSpec(spec),
  tokenizer(tokenizer),
  token(tokenizer.nextToken())
{}

template <typename T>
void Parser<T>::nextToken()
{
    if (atEnd())
        throw ParseError("Unexpected end of input");
    token = tokenizer.nextToken();
}

template <typename T>
bool Parser<T>::opt(TokenType type, Token& tokenOut)
{
    if (notFollowedBy(type))
        return false;
    tokenOut = token;
    nextToken();
    return true;
}

template <typename T>
Token Parser<T>::match(TokenType type)
{
    Token token;
    if (!opt(type, token)) {
        throw ParseError("Expected " + tokenizer.typeName(type) +
                         " but found " + tokenizer.typeName(token.type));
    }
    return token;
}

template <typename T>
T Parser<T>::expression(const ExprSpec<T>& spec, unsigned bindRight)
{
    Token t = token;
    nextToken();
    T left = prefix(spec, t.type);
    while (bindRight < getBindLeft(spec, token.type)) {
        t = token;
        nextToken();
        left = infix(spec, t.type, left);
    }
    return left;
}

template <typename T>
T Parser<T>::prefix(const ExprSpec<T>& spec, TokenType type)
{
    if (!spec.prefixActions[type]) {
        throw ParseError("Unexpected '" + tokenizer.typeName(type));
        // todo: + " in " + spec.name + " context"
    }
    return spec.prefixActions[token.type](*this, spec, token);
}

template <typename T>
unsigned Parser<T>::getBindLeft(const ExprSpec<T>& spec, TokenType type)
{
    if (!spec.infixActions[type].second)
        return 0;
    return spec.infixActions[type].first;
}

template <typename T>
T Parser<T>::infix(const ExprSpec<T>& spec, TokenType type, const T leftValue)
{
    if (!spec.infixActions[type].second) {
        throw ParseError("Unexpected '" + tokenizer.typeName(type));
        // todo + " in " + spec.name + " context"
    }
    return spec.infixActions[token.type].second(*this, spec, token, leftValue);
}

#endif
