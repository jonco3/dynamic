#ifndef __PARSER_H__
#define __PARSER_H__

#include "token.h"
#include "assert.h"
#include "syntax.h"

#include <vector>
#include <stdexcept>
#include <functional>

using namespace std;

enum Assoc
{
    Assoc_Left,
    Assoc_Right
};

struct ParseError : public runtime_error
{
    ParseError(const Token& token, string message);

    TokenPos pos;
};

template <typename T>
struct Parser
{
    Parser(Tokenizer& tokenizer);

    void start(const Input& input);
    bool atEnd() { return token.type == Token_EOF; }
    T parse() { return expression(); }

    typedef Parser<T> ParserT;
    typedef function<T (ParserT& parser, Token token)> PrefixHandler;
    typedef function<T (ParserT& parser, Token token, const T leftValue)> InfixHandler;
    typedef function<T (Token token)> WordHandler;
    typedef function<T (Token token, const T leftValue, const T rightValue)> BinaryOpHandler;
    typedef function<T (Token token, const T rightValue)> UnaryOpHandler;

    // Add a generic handler to call for a token in prefix position
    void addPrefixHandler(TokenType type, PrefixHandler handler);

    // Add a generic handler to call for a token in infix position
    void addInfixHandler(TokenType type, unsigned bindLeft, InfixHandler handler);

    void addWord(TokenType type, WordHandler handler);

    void addBinaryOp(TokenType type, unsigned bindLeft, Assoc assoc,
                     BinaryOpHandler handler);

    void addUnaryOp(TokenType type, UnaryOpHandler hanlder);

    // Add a handler that creates a new node of type N for an atom
    template<typename N> void createNodeForAtom(TokenType type);

    // Add a handler that creates a new node of type N for a token in prefix
    // position
    template<typename N> void createNodeForUnary(TokenType type, unsigned bindRight = 500);  // todo: hardcoded constant

    // Add a handler that creates a new node of type N for a token in infix
    // position
    template<typename N> void createNodeForBinary(TokenType type,
                                                  unsigned bindLeft, Assoc assoc);
    template<typename N, typename X>
    void createNodeForBinary(TokenType type, unsigned bindLeft, Assoc assoc, X x);

    bool notFollowedBy(TokenType type) { return token.type != type; }
    bool opt(TokenType type);
    bool opt(TokenType type, Token& tokenOut);
    Token match(TokenType type);
    Token matchEither(TokenType type1, TokenType type2);
    T expression(unsigned bindRight = 0);
    vector<T> exprList(TokenType separator, TokenType end);
    vector<T> exprListTrailing(TokenType separator, TokenType end);

    const Token& currentToken() { return token; }
    void nextToken();
    T prefix(Token token);
    unsigned getBindLeft(Token token);
    T infix(Token token, const T leftValue);

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

    Tokenizer& tokenizer;
    vector<PrefixAction> prefixActions;
    vector<InfixAction> infixActions;
    Token token;
};

template <typename T>
void Parser<T>::addPrefixHandler(TokenType type, PrefixHandler handler)
{
    assert(type < prefixActions.size());
    assert(!prefixActions[type].present);
    prefixActions[type] = {true, handler};
}

template <typename T>
void Parser<T>::addInfixHandler(TokenType type, unsigned bindLeft,
                                  InfixHandler handler)
{
    assert(type < infixActions.size());
    assert(!infixActions[type].present);
    infixActions[type] = {true, bindLeft, handler};
}

template <typename T>
void Parser<T>::addWord(TokenType type, WordHandler handler)
{
    addPrefixHandler(type,
                     [=] (Parser<T>& parser, Token token) {
                         return handler(token);
                     });
}

template <typename T>
void Parser<T>::addBinaryOp(TokenType type, unsigned bindLeft, Assoc assoc,
                 BinaryOpHandler handler) {
    // calculate right binding power by addng left binding power and
    // associativity, hence why left binding power must be a multiple
    // of two
    unsigned bindRight = bindLeft + assoc;
    addInfixHandler(type,
                    bindLeft,
                    [=] (Parser<T>& parser, Token token,
                         const T leftValue) {
                        T rightValue = parser.expression(bindRight);
                        return handler(token, leftValue, rightValue);
                    });
}

template <typename T>
void Parser<T>::addUnaryOp(TokenType type, UnaryOpHandler handler)
{
    addPrefixHandler(type,
                     [=] (Parser<T>& parser, Token token) {
                         // todo: magic value should be constant
                         T rightValue = parser.expression(500);
                         return handler(token, rightValue);
                     });
}

template<typename T>
template<typename N>
void Parser<T>::createNodeForAtom(TokenType type)
{
    addPrefixHandler(type,
                     [=] (Parser<T>& parser, Token token) {
                         return new N(token);
                     });
}

template<typename T>
template<typename N>
void Parser<T>::createNodeForUnary(TokenType type, unsigned bindRight)
{
    addPrefixHandler(type,
                     [=] (Parser<T>& parser, Token token) {
                         return new N(token, parser.expression(bindRight));
                     });
}

template<typename T>
template<typename N>
void Parser<T>::createNodeForBinary(TokenType type, unsigned bindLeft,
                                    Assoc assoc)
{
    // calculate right binding power by addng left binding power and
    // associativity, hence why left binding power must be a multiple
    // of two
    unsigned bindRight = bindLeft + assoc;
    addInfixHandler(type,
                    bindLeft,
                    [=] (Parser<T>& parser, Token token,
                         const T leftValue) {
                        T rightValue = parser.expression(bindRight);
                        return new N(token, leftValue, rightValue);
                    });
}

template<typename T>
template<typename N, typename X> // todo: use variadic templates
void Parser<T>::createNodeForBinary(TokenType type, unsigned bindLeft,
                                    Assoc assoc, X x)
{
    // calculate right binding power by addng left binding power and
    // associativity, hence why left binding power must be a multiple
    // of two
    unsigned bindRight = bindLeft + assoc;
    addInfixHandler(type,
                    bindLeft,
                    [=] (Parser<T>& parser, Token token,
                         const T leftValue) {
                        T rightValue = parser.expression(bindRight);
                        return new N(token, leftValue, rightValue, x);
                    });
}

template <typename T>
Parser<T>::Parser(Tokenizer& tokenizer)
  : tokenizer(tokenizer)
{
    for (unsigned i = 0; i < tokenizer.numTypes(); ++i) {
        prefixActions.push_back({false});
        infixActions.push_back({false});
    }
}

template <typename T>
void Parser<T>::nextToken()
{
    if (atEnd())
        throw ParseError(token, "Unexpected end of input");
    token = tokenizer.nextToken();
}

template <typename T>
void Parser<T>::start(const Input& input)
{
    tokenizer.start(input);
    token = tokenizer.nextToken();
}

template <typename T>
bool Parser<T>::opt(TokenType type)
{
    if (notFollowedBy(type))
        return false;
    nextToken();
    return true;
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
    Token t;
    if (!opt(type, t)) {
        throw ParseError(token,
                         "Expected " + tokenizer.typeName(type) +
                         " but found " + tokenizer.typeName(token.type));
    }
    return t;
}

template <typename T>
Token Parser<T>::matchEither(TokenType type1, TokenType type2)
{
    Token t;
    if (!opt(type1, t) && !opt(type2, t)) {
        throw ParseError(token,
                         "Expected " + tokenizer.typeName(type1) +
                         " or " + tokenizer.typeName(type2) +
                         " but found " + tokenizer.typeName(token.type));
    }
    return t;
}

template <typename T>
T Parser<T>::expression(unsigned bindRight)
{
    Token t = token;
    nextToken();
    T left = prefix(t);
    while (bindRight < getBindLeft(token)) {
        t = token;
        nextToken();
        left = infix(t, left);
    }
    return left;
}

template <typename T>
T Parser<T>::prefix(Token token)
{
    const auto& action = prefixActions[token.type];
    if (!action.present) {
        throw ParseError(token, "Unexpected " + tokenizer.typeName(token.type));
        // todo: + " in " + name + " context"
    }
    return action.handler(*this, token);
}

template <typename T>
unsigned Parser<T>::getBindLeft(Token token)
{
    const auto& action = infixActions[token.type];
    if (!action.present)
        return 0;
    return action.bindLeft;
}

template <typename T>
T Parser<T>::infix(Token token, const T leftValue)
{
    const auto& action = infixActions[token.type];
    if (!action.present) {
        throw ParseError(token, "Unexpected " + tokenizer.typeName(token.type));
        // todo + " in " + name + " context"
    }
    return action.handler(*this, token, leftValue);
}

struct SyntaxParser : public Parser<Syntax *>
{
    SyntaxParser();
    Syntax* parseSimpleStatement();
    Syntax* parseCompoundStatement();
    SyntaxBlock* parseModule();

  private:
    Tokenizer tokenizer;

    bool maybeExprToken();
    Syntax* parseExprOrExprList();
    SyntaxBlock* parseBlock();
    SyntaxBlock* parseSuite();

    SyntaxTarget* makeAssignTarget(Syntax* s);

    SyntaxTarget* parseTarget();
    SyntaxTarget* parseTargetList();

    Syntax* parseAugAssign(Token token, Syntax* target, BinaryOp op);
};

#endif
