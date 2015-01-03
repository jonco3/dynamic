#ifndef __PARSER_H__
#define __PARSER_H__

#include "token.h"
#include "assert.h"

#include <vector>
#include <stdexcept>
#include <functional>

using namespace std;

template <typename T>
struct Parser;

enum Assoc
{
    Assoc_Left,
    Assoc_Right
};

struct ParseError : public runtime_error
{
    ParseError(string message);
};

template <typename T>
struct Parser
{
    typedef Parser<T> ParserT;

    struct Actions
    {
        Actions() {}
        Actions(unsigned maxTokenTypes);

        typedef function<T (ParserT& parser, const Actions& acts, Token token)> PrefixHandler;
        typedef function<T (ParserT& parser, const Actions& acts, Token token, const T leftValue)> InfixHandler;
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

        vector<PrefixAction> prefixActions;
        vector<InfixAction> infixActions;

        friend struct Parser<T>;
    };

    Parser(Tokenizer& tokenizer);

    void start(const Input& input);

    bool atEnd() { return token.type == Token_EOF; }
    T parse(const Actions& acts) { return expression(acts); }

    bool notFollowedBy(TokenType type) { return token.type != type; }
    bool opt(TokenType type);
    bool opt(TokenType type, Token& tokenOut);
    Token match(TokenType type);
    Token matchEither(TokenType type1, TokenType type2);
    T expression(const Actions& acts, unsigned bindRight = 0);
    vector<T> exprList(const Actions& acts, TokenType separator, TokenType end);
    vector<T> exprListTrailing(const Actions& acts, TokenType separator, TokenType end);

    const Token& currentToken() { return token; }
    void nextToken();
    T prefix(const Actions& acts, Token token);
    unsigned getBindLeft(const Actions& acts, Token token);
    T infix(const Actions& acts, Token token, const T leftValue);

  private:
    Tokenizer& tokenizer;
    Token token;
};

template <typename T>
Parser<T>::Actions::Actions(unsigned maxTokenTypes)
{
    for (unsigned i = 0; i < maxTokenTypes; ++i) {
        prefixActions.push_back({false});
        infixActions.push_back({false});
    }
}

template <typename T>
void Parser<T>::Actions::addPrefixHandler(TokenType type, PrefixHandler handler)
{
    assert(type < prefixActions.size());
    assert(!prefixActions[type].present);
    prefixActions[type] = {true, handler};
}

template <typename T>
void Parser<T>::Actions::addInfixHandler(TokenType type, unsigned bindLeft,
                                  InfixHandler handler)
{
    assert(type < infixActions.size());
    assert(!infixActions[type].present);
    infixActions[type] = {true, bindLeft, handler};
}

template <typename T>
void Parser<T>::Actions::addWord(TokenType type, WordHandler handler)
{
    addPrefixHandler(type,
                     [=] (Parser<T>& parser, Actions acts, Token token) {
                         return handler(token);
                     });
}

template <typename T>
void Parser<T>::Actions::addBinaryOp(TokenType type, unsigned bindLeft, Assoc assoc,
                 BinaryOpHandler handler) {
    // calculate right binding power by addng left binding power and
    // associativity, hence why left binding power must be a multiple
    // of two
    unsigned bindRight = bindLeft + assoc;
    addInfixHandler(type,
                    bindLeft,
                    [=] (Parser<T>& parser, Actions acts, Token token,
                         const T leftValue) {
                        T rightValue = parser.expression(acts, bindRight);
                        return handler(token, leftValue, rightValue);
                    });
}

template <typename T>
void Parser<T>::Actions::addUnaryOp(TokenType type, UnaryOpHandler handler)
{
    addPrefixHandler(type,
                     [=] (Parser<T>& parser, Actions acts, Token token) {
                         // todo: magic value should be constant
                         T rightValue = parser.expression(acts, 500);
                         return handler(token, rightValue);
                     });
}

template<typename T>
template<typename N>
void Parser<T>::Actions::createNodeForAtom(TokenType type)
{
    addPrefixHandler(type,
                     [=] (Parser<T>& parser, Actions acts, Token token) {
                         return new N(token);
                     });
}

template<typename T>
template<typename N>
void Parser<T>::Actions::createNodeForUnary(TokenType type, unsigned bindRight)
{
    addPrefixHandler(type,
                     [=] (Parser<T>& parser, Actions acts, Token token) {
                         return new N(token, parser.expression(acts, bindRight));
                     });
}

template<typename T>
template<typename N>
void Parser<T>::Actions::createNodeForBinary(TokenType type, unsigned bindLeft,
                                             Assoc assoc)
{
    // calculate right binding power by addng left binding power and
    // associativity, hence why left binding power must be a multiple
    // of two
    unsigned bindRight = bindLeft + assoc;
    addInfixHandler(type,
                    bindLeft,
                    [=] (Parser<T>& parser, Actions acts, Token token,
                         const T leftValue) {
                        T rightValue = parser.expression(acts, bindRight);
                        return new N(token, leftValue, rightValue);
                    });
}


template <typename T>
Parser<T>::Parser(Tokenizer& tokenizer) : tokenizer(tokenizer) {}

template <typename T>
void Parser<T>::nextToken()
{
    if (atEnd())
        throw ParseError("Unexpected end of input");
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
        throw ParseError("Expected " + tokenizer.typeName(type) +
                         " but found " + tokenizer.typeName(token.type));
    }
    return t;
}

template <typename T>
Token Parser<T>::matchEither(TokenType type1, TokenType type2)
{
    Token t;
    if (!opt(type1, t) && !opt(type2, t)) {
        throw ParseError("Expected " + tokenizer.typeName(type1) +
                         " or " + tokenizer.typeName(type2) +
                         " but found " + tokenizer.typeName(token.type));
    }
    return t;
}

template <typename T>
T Parser<T>::expression(const Actions& acts, unsigned bindRight)
{
    Token t = token;
    nextToken();
    T left = prefix(acts, t);
    while (bindRight < getBindLeft(acts, token)) {
        t = token;
        nextToken();
        left = infix(acts, t, left);
    }
    return left;
}

template <typename T>
T Parser<T>::prefix(const Actions& acts, Token token)
{
    const auto& action = acts.prefixActions[token.type];
    if (!action.present) {
        throw ParseError("Unexpected " + tokenizer.typeName(token.type));
        // todo: + " in " + acts.name + " context"
    }
    return action.handler(*this, acts, token);
}

template <typename T>
unsigned Parser<T>::getBindLeft(const Actions& acts, Token token)
{
    const auto& action = acts.infixActions[token.type];
    if (!action.present)
        return 0;
    return action.bindLeft;
}

template <typename T>
T Parser<T>::infix(const Actions& acts, Token token, const T leftValue)
{
    const auto& action = acts.infixActions[token.type];
    if (!action.present) {
        throw ParseError("Unexpected " + tokenizer.typeName(token.type));
        // todo + " in " + acts.name + " context"
    }
    return action.handler(*this, acts, token, leftValue);
}

struct Syntax;
struct SyntaxBlock;
struct SyntaxParser : public Parser<Syntax *>
{
    SyntaxParser();
    Syntax* parseExpr() { return Parser::parse(expr); }
    Syntax* parseSimpleStatement();
    Syntax* parseCompoundStatement();
    SyntaxBlock* parseModule();

  private:
    Tokenizer tokenizer;
    Actions expr;

    bool maybeExprToken();
    Syntax* parseExprOrExprList();
    SyntaxBlock* parseBlock();
    SyntaxBlock* parseSuite();

    void checkAssignTarget(Syntax* s);
};

#endif
