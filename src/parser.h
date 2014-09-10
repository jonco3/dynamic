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
    struct PrefixAction
    {
        bool present;
        PrefixHandler handler;
    };

    struct InfixAction
    {
        bool present;
        unsigned bindLeft;
        InfixHandler handler;
    };

    std::vector<PrefixAction> prefixActions;
    std::vector<InfixAction> infixActions;

    friend struct Parser<T>;
};

template <typename T>
ExprSpec<T>::ExprSpec(unsigned maxTokenTypes)
{
    for (unsigned i = 0; i < maxTokenTypes; ++i) {
        prefixActions.push_back({false});
        infixActions.push_back({false});
    }
}

template <typename T>
void ExprSpec<T>::addPrefixHandler(TokenType type, PrefixHandler handler)
{
    assert(type < prefixActions.size());
    prefixActions[type] = {true, handler};
}

template <typename T>
void ExprSpec<T>::addInfixHandler(TokenType type, unsigned bindLeft,
                                  InfixHandler handler)
{
    assert(type < infixActions.size());
    infixActions[type] = {true, bindLeft, handler};
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

struct ParseError : public std::runtime_error
{
    ParseError(std::string message);
};

template <typename T>
struct Parser
{
    Parser(const ExprSpec<T>& spec, Tokenizer& tokenizer);

    void start(const char* source, const char* file = "");

    bool atEnd() { return token.type == Token_EOF; }
    T parse() { return expression(topSpec); }

    bool notFollowedBy(TokenType type) { return token.type != type; }
    bool opt(TokenType type, Token& tokenOut);
    Token match(TokenType type);
    T expression(const ExprSpec<T>& spec, unsigned bindRight = 0);

    void nextToken();
    T prefix(const ExprSpec<T>& spec, Token token);
    unsigned getBindLeft(const ExprSpec<T>& spec, Token token);
    T infix(const ExprSpec<T>& spec, Token token, const T leftValue);

  private:
    const ExprSpec<T>& topSpec;
    Tokenizer& tokenizer;
    Token token;
};

template <typename T>
Parser<T>::Parser(const ExprSpec<T>& spec, Tokenizer& tokenizer) :
  topSpec(spec),
  tokenizer(tokenizer)
{}

template <typename T>
void Parser<T>::nextToken()
{
    if (atEnd())
        throw ParseError("Unexpected end of input");
    token = tokenizer.nextToken();
}

template <typename T>
void Parser<T>::start(const char* source, const char* file)
{
    tokenizer.start(source, file);
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
    T left = prefix(spec, t);
    while (bindRight < getBindLeft(spec, token)) {
        t = token;
        nextToken();
        left = infix(spec, t, left);
    }
    return left;
}

template <typename T>
T Parser<T>::prefix(const ExprSpec<T>& spec, Token token)
{
    const auto& action = spec.prefixActions[token.type];
    if (!action.present) {
        throw ParseError("Unexpected '" + tokenizer.typeName(token.type));
        // todo: + " in " + spec.name + " context"
    }
    return action.handler(*this, spec, token);
}

template <typename T>
unsigned Parser<T>::getBindLeft(const ExprSpec<T>& spec, Token token)
{
    const auto& action = spec.infixActions[token.type];
    if (!action.present)
        return 0;
    return action.bindLeft;
}

template <typename T>
T Parser<T>::infix(const ExprSpec<T>& spec, Token token, const T leftValue)
{
    const auto& action = spec.infixActions[token.type];
    if (!action.present) {
        throw ParseError("Unexpected '" + tokenizer.typeName(token.type));
        // todo + " in " + spec.name + " context"
    }
    return action.handler(*this, spec, token, leftValue);
}

#endif
