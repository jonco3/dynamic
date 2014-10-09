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
        Actions(unsigned maxTokenTypes);

        typedef function<T (ParserT& parser, const Actions& acts, Token token)>
        PrefixHandler;
        void addPrefixHandler(TokenType type, PrefixHandler handler);

        typedef function<T (ParserT& parser, const Actions& acts, Token token,
                            const T leftValue)> InfixHandler;
        void addInfixHandler(TokenType type, unsigned bindLeft, InfixHandler handler);

        typedef function<T (Token token)> WordHandler;
        void addWord(TokenType type, WordHandler handler);

        typedef function<T (Token token, const T leftValue, const T rightValue)>
        BinaryOpHandler;
        void addBinaryOp(TokenType type, unsigned bindLeft, Assoc assoc,
                         BinaryOpHandler handler);

        typedef function<T (Token token, const T rightValue)> UnaryOpHandler;
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

        vector<PrefixAction> prefixActions;
        vector<InfixAction> infixActions;

        friend struct Parser<T>;
    };

    Parser(const Actions& acts, Tokenizer& tokenizer);

    void start(const Input& input);

    bool atEnd() { return token.type == Token_EOF; }
    T parse() { return expression(actions); }

    bool notFollowedBy(TokenType type) { return token.type != type; }
    bool opt(TokenType type);
    bool opt(TokenType type, Token& tokenOut);
    Token match(TokenType type);
    T expression(const Actions& acts, unsigned bindRight = 0);

    void nextToken();
    T prefix(const Actions& acts, Token token);
    unsigned getBindLeft(const Actions& acts, Token token);
    T infix(const Actions& acts, Token token, const T leftValue);

  private:
    const Actions& actions;
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
    prefixActions[type] = {true, handler};
}

template <typename T>
void Parser<T>::Actions::addInfixHandler(TokenType type, unsigned bindLeft,
                                  InfixHandler handler)
{
    assert(type < infixActions.size());
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
                         T rightValue = parser.expression(acts, 500);
                         return handler(token, rightValue);
                     });
}

template <typename T>
Parser<T>::Parser(const Actions& acts, Tokenizer& tokenizer) :
  actions(acts),
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
    Token token;
    if (!opt(type, token)) {
        throw ParseError("Expected " + tokenizer.typeName(type) +
                         " but found " + tokenizer.typeName(token.type));
    }
    return token;
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
    SyntaxBlock *parseBlock();
  private:
    Tokenizer tokenizer;
    Parser<Syntax*>::Actions acts;
};

#endif
