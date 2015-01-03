#include "parser.h"

#include "syntax.h"
#include "repr.h"
#include "test.h"

#include <cstdlib>
#include <stdexcept>
#include <ostream>

ParseError::ParseError(string message) :
  runtime_error(message)
{
#ifdef BUILD_TESTS
    maybeAbortTests(*this);
#endif
}

/*
 * Match a comma separated list of expressions up to an end token.
 */
template <typename T>
vector<T> Parser<T>::exprList(const Actions& acts, TokenType separator, TokenType end)
{
    vector<T> exprs;
    if (opt(end))
        return exprs;

    for (;;) {
        exprs.push_back(expression(acts));
        if (opt(end))
            break;
        match(separator);
    }
    return exprs;
}

/*
 * Match a comma separated list of expressions up to an end token, allowing a
 * trailing comma.
 */
template <typename T>
vector<T> Parser<T>::exprListTrailing(const Actions& acts, TokenType separator,
                                      TokenType end)
{
    vector<T> exprs;
    while (!opt(end)) {
        exprs.push_back(expression(acts));
        if (opt(end))
            break;
        match(separator);
    }
    return exprs;
}

SyntaxParser::SyntaxParser() :
  Parser(tokenizer),
  expr(TokenCount)
{
    // Atoms

    expr.createNodeForAtom<SyntaxName>(Token_Identifier);
    expr.createNodeForAtom<SyntaxString>(Token_String);
    expr.createNodeForAtom<SyntaxInteger>(Token_Integer);
    // todo: longinteger floatnumber imagnumber

    // Unary operations

    // todo: check the precedence of these works as expected
    expr.createNodeForUnary<SyntaxPos>(Token_Plus);
    expr.createNodeForUnary<SyntaxNeg>(Token_Minus);
    expr.createNodeForUnary<SyntaxNot>(Token_Not, 0);
    expr.createNodeForUnary<SyntaxInvert>(Token_BitNot);

    // Displays

    expr.addPrefixHandler(Token_Bra, [=] (ParserT& parser, const Actions& acts,
                                         Token token) -> Syntax* {
        if (parser.opt(Token_Ket))
            return new SyntaxExprList(token);
        Syntax* result = parseExprOrExprList();
        parser.match(Token_Ket);
        return result;
    });

    expr.addPrefixHandler(Token_SBra, [] (ParserT& parser, const Actions& acts,
                                          Token token) -> Syntax* {
        return new SyntaxList(token,
                              parser.exprListTrailing(acts, Token_Comma, Token_SKet));
    });

    expr.addPrefixHandler(Token_CBra, [] (ParserT& parser, const Actions& acts,
                                          Token token) -> Syntax* {
        vector<SyntaxDict::Entry> entries;
        while (!parser.opt(Token_CKet)) {
            Syntax* key = parser.expression(acts);
            parser.match(Token_Colon);
            Syntax* value = parser.expression(acts);
            entries.push_back(SyntaxDict::Entry(key, value));
            if (parser.opt(Token_CKet))
                break;
            parser.match(Token_Comma);
        }
        return new SyntaxDict(token, entries);
    });

    // Lambda

    expr.addPrefixHandler(Token_Lambda, [] (ParserT& parser, const Actions& acts,
                                            Token token) {
        vector<string> params;
        if (!parser.opt(Token_Colon)) {
            for (;;) {
                // todo: * and **
                Token t = parser.match(Token_Identifier);
                params.push_back(t.text);
                if (parser.opt(Token_Colon))
                    break;
                parser.match(Token_Comma);
            }
        }
        return new SyntaxLambda(token, params, parser.expression(acts));
    });

    // Conditional expression

    expr.addInfixHandler(Token_If, 90,
                         [] (ParserT& parser, const Actions& acts, Token token,
                             Syntax* cond)
    {
        Syntax* cons = parser.expression(acts);
        parser.match(Token_Else);
        Syntax* alt = parser.expression(acts);
        return new SyntaxCond(token, cond, cons, alt);
    });

    // Boolean operators

    expr.createNodeForBinary<SyntaxOr>(Token_Or, 100, Assoc_Left);
    expr.createNodeForBinary<SyntaxAnd>(Token_And, 100, Assoc_Left);

    // Comparison operators

    expr.createNodeForBinary<SyntaxIn>(Token_In, 120, Assoc_Left);
    expr.createNodeForBinary<SyntaxIs>(Token_Is, 120, Assoc_Left);
    expr.createNodeForBinary<SyntaxLT>(Token_LT, 120, Assoc_Left);
    expr.createNodeForBinary<SyntaxLE>(Token_LE, 120, Assoc_Left);
    expr.createNodeForBinary<SyntaxGT>(Token_GT, 120, Assoc_Left);
    expr.createNodeForBinary<SyntaxGE>(Token_GE, 120, Assoc_Left);
    expr.createNodeForBinary<SyntaxEQ>(Token_EQ, 120, Assoc_Left);
    expr.createNodeForBinary<SyntaxNE>(Token_NE, 120, Assoc_Left);
    expr.addBinaryOp(Token_NotIn, 120, Assoc_Left, [] (Token token, Syntax* l,
                                                       Syntax* r) {
        return new SyntaxNot(token, new SyntaxIn(token, l, r));
    });
    expr.addBinaryOp(Token_IsNot, 120, Assoc_Left, [] (Token token, Syntax* l,
                                                       Syntax* r) {
        return new SyntaxNot(token, new SyntaxIs(token, l, r));
    });

    // Bitwise binary operators

    expr.createNodeForBinary<SyntaxBitOr>(Token_BitOr, 130, Assoc_Left);
    expr.createNodeForBinary<SyntaxBitXor>(Token_BitXor, 140, Assoc_Left);
    expr.createNodeForBinary<SyntaxBitAnd>(Token_BitAnd, 150, Assoc_Left);
    expr.createNodeForBinary<SyntaxBitLeftShift>(Token_BitLeftShift, 160, Assoc_Left);
    expr.createNodeForBinary<SyntaxBitRightShift>(Token_BitRightShift, 160, Assoc_Left);

    // Arithermetic binary operators

    expr.createNodeForBinary<SyntaxPlus>(Token_Plus, 170, Assoc_Left);
    expr.createNodeForBinary<SyntaxMinus>(Token_Minus, 170, Assoc_Left);
    expr.createNodeForBinary<SyntaxMultiply>(Token_Times, 180, Assoc_Left);
    expr.createNodeForBinary<SyntaxDivide>(Token_Divide, 180, Assoc_Left);
    expr.createNodeForBinary<SyntaxIntDivide>(Token_IntDivide, 180, Assoc_Left);
    expr.createNodeForBinary<SyntaxModulo>(Token_Modulo, 180, Assoc_Left);
    expr.createNodeForBinary<SyntaxPower>(Token_Power, 180, Assoc_Left);

    expr.addInfixHandler(Token_SBra, 200, [] (ParserT& parser, const Actions& acts,
                                              Token token, Syntax* l) {
        Syntax* index = parser.expression(acts);
        parser.match(Token_SKet);
        return new SyntaxSubscript(token, l, index);
    });

    expr.addInfixHandler(Token_Bra, 200, [] (ParserT& parser, const Actions& acts,
                                            Token token, Syntax* l) {
        vector<Syntax*> args;
        while (!parser.opt(Token_Ket)) {
            if (!args.empty())
                parser.match(Token_Comma);
            args.push_back(parser.expression(acts));
        }
        return new SyntaxCall(token, l, args);
    });

    expr.addBinaryOp(Token_Period, 200, Assoc_Left, [] (Token token, Syntax* l, Syntax* r) {
        if (!r->is<SyntaxName>())
            throw ParseError("Bad property reference");
        return new SyntaxAttrRef(token, l, r->as<SyntaxName>());
    });

    expr.addPrefixHandler(Token_Return, [] (ParserT& parser, const Actions& acts,
                                            Token token) {
        Syntax *expr = nullptr;
        if (parser.notFollowedBy(Token_EOF) &&
            parser.notFollowedBy(Token_Newline) &&
            parser.notFollowedBy(Token_Dedent))
        {
            expr = parser.expression(acts);
        }
        return new SyntaxReturn(token, expr);
    });

    // Simple statements

    simpleStmt = expr;

    // Assignments

    simpleStmt.addBinaryOp(Token_Assign, 10, Assoc_Right,
                          [] (Token token, Syntax* l, Syntax* r) -> Syntax* {
        if (l->is<SyntaxName>())
            return new SyntaxAssignName(token, l->as<SyntaxName>(), r);
        else if (l->is<SyntaxAttrRef>())
            return new SyntaxAssignAttr(token, l->as<SyntaxAttrRef>(), r);
        else if (l->is<SyntaxSubscript>())
            return new SyntaxAssignSubscript(token, l->as<SyntaxSubscript>(), r);
        else
            throw ParseError("Illegal LHS for assignment");
    });
}

bool SyntaxParser::maybeExprToken()
{
    // Return whether the next token can be the start of an expression.
    TokenType t = currentToken().type;
    return t == Token_Identifier ||
        t == Token_Integer || t == Token_String ||
        t == Token_Bra || t == Token_SBra || t == Token_CBra ||
        t == Token_Minus || t == Token_Plus || t == Token_BitNot ||
        t == Token_Not || t == Token_Lambda || t == Token_Yield ||
        t == Token_Backtick;
}

Syntax* SyntaxParser::parseExprOrExprList()
{
    Token startToken = currentToken();
    vector<Syntax*> exprs;
    exprs.push_back(parseExpr());
    bool singleExpr = true;
    while (opt(Token_Comma)) {
        singleExpr = false;
        if (!maybeExprToken())
            break;
        exprs.push_back(parseExpr());
    }
    if (singleExpr)
        return exprs[0];
    return new SyntaxExprList(startToken, exprs);
}

Syntax* SyntaxParser::parseSimpleStatement()
{
    Token token = currentToken();
    if (opt(Token_Assert)) {
        Syntax* cond = parseExpr();
        Syntax* message = nullptr;
        if (opt(Token_Comma))
            message = parseExpr();
        return new SyntaxAssert(token, cond, message);
    } else if (opt(Token_Pass)) {
        return new SyntaxPass(token);
    }

    Syntax* expr = parseExprOrExprList();
    if (opt(Token_Assign)) {
        Syntax *l = expr;
        Syntax *r = parseExpr();
        if (l->is<SyntaxName>())
            return new SyntaxAssignName(token, l->as<SyntaxName>(), r);
        else if (l->is<SyntaxAttrRef>())
            return new SyntaxAssignAttr(token, l->as<SyntaxAttrRef>(), r);
        else if (l->is<SyntaxSubscript>())
            return new SyntaxAssignSubscript(token, l->as<SyntaxSubscript>(), r);
        else
            throw ParseError("Illegal LHS for assignment");
    }

    return expr;
}

Syntax* SyntaxParser::parseCompoundStatement()
{
    Token token = currentToken();
    Syntax* stmt = nullptr;
    if (opt(Token_If)) {
        SyntaxIf* ifStmt = new SyntaxIf(token);
        do {
            Syntax* cond = parseExpr();
            SyntaxBlock* suite = parseSuite();
            ifStmt->addBranch(cond, suite);
        } while (opt(Token_Elif));
        if (opt(Token_Else)) {
            ifStmt->setElse(parseSuite());
        }
        stmt = ifStmt;
    } else if (opt(Token_While)) {
        Syntax* cond = parseExpr();
        SyntaxBlock* suite = parseSuite();
        SyntaxBlock* elseSuite = nullptr;
        if (opt(Token_Else))
            elseSuite = parseSuite();
        stmt = new SyntaxWhile(token, cond, suite, elseSuite);
    //} else if (opt(Token_For)) {
    //} else if (opt(Token_Try)) {
    //} else if (opt(Token_With)) {
    } else if (opt(Token_Def)) {
        Token name = match(Token_Identifier);
        match(Token_Bra);
        vector<string> params;
        if (!opt(Token_Ket)) {
            for (;;) {
                Token t = match(Token_Identifier);
                // todo: * and **
                params.push_back(t.text);
                if (opt(Token_Ket))
                    break;
                match(Token_Comma);
            }
        }
        SyntaxBlock* suite = parseSuite();
        return new SyntaxDef(token, name.text, params, suite);
    } else if (opt(Token_Class)) {
        Token name = match(Token_Identifier);
        vector<Syntax*> bases;
        if (opt(Token_Bra))
            bases = exprListTrailing(expr, Token_Comma, Token_Ket);
        return new SyntaxClass(token,
                               name.text,
                               new SyntaxExprList(token, bases),
                               parseSuite());
    } else {
        stmt = parseSimpleStatement();
        if (!atEnd())
            matchEither(Token_Newline, Token_Semicolon);
    }

    return stmt;
}

SyntaxBlock* SyntaxParser::parseBlock()
{
    unique_ptr<SyntaxBlock> syntax(new SyntaxBlock(currentToken()));
    match(Token_Indent);
    while (!opt(Token_Dedent)) {
        syntax->append(parseCompoundStatement());
    }
    return syntax.release();
}

SyntaxBlock* SyntaxParser::parseSuite()
{
    unique_ptr<SyntaxBlock> suite(new SyntaxBlock(currentToken()));
    match(Token_Colon);
    if (opt(Token_Newline)) {
        match(Token_Indent);
        while (!opt(Token_Dedent)) {
            suite->append(parseCompoundStatement());
        }
    } else {
        do {
            suite->append(parseSimpleStatement());
            if (opt(Token_Newline))
                break;
            match(Token_Semicolon);
        } while (!opt(Token_Newline));
    }
    return suite.release();
}

SyntaxBlock* SyntaxParser::parseModule()
{
    unique_ptr<SyntaxBlock> syntax(new SyntaxBlock(currentToken()));
    while (!atEnd())
        syntax->append(parseCompoundStatement());
    return syntax.release();
}

#ifdef BUILD_TESTS

testcase(parser)
{
    Parser<int>::Actions acts(TokenCount);
    acts.addWord(Token_Integer, [] (Token token) {
        return atoi(token.text.c_str());  // todo: what's the c++ way to do this?
    });
    acts.addBinaryOp(Token_Plus, 10, Assoc_Left, [] (Token _, int l, int r) {
        return l + r;
    });
    acts.addBinaryOp(Token_Minus, 10, Assoc_Left, [] (Token _, int l, int r) {
        return l - r;
    });
    acts.addUnaryOp(Token_Minus, [] (Token _, int r) {
        return -r;
    });

    Tokenizer tokenizer;
    Parser<int> parser(tokenizer);

    parser.start("2 + 3 - 1");
    testFalse(parser.atEnd());
    testEqual(parser.parse(acts), 4);
    testTrue(parser.atEnd());
    testThrows(parser.parse(acts), ParseError);

    parser.start("1 2");
    testEqual(parser.parse(acts), 1);
    testEqual(parser.parse(acts), 2);
    testTrue(parser.atEnd());
    testThrows(parser.parse(acts), ParseError);

    SyntaxParser sp;
    sp.start("1+2-3");
    unique_ptr<Syntax> expr(sp.parseExpr());
    testEqual(repr(expr.get()), "1 + 2 - 3");

    sp.start("4 ** 5");
    expr.reset(sp.parseExpr());
    testEqual(repr(expr.get()), "4 ** 5");

    sp.start("f = 1 + 2");
    expr.reset(sp.parseCompoundStatement());
    testTrue(expr.get()->is<SyntaxAssignName>());

    sp.start("1\n2");
    expr.reset(sp.parseModule());
    testEqual(repr(expr.get()), "1\n2\n");

    sp.start("1; 2; 3");
    expr.reset(sp.parseModule());
    testEqual(repr(expr.get()), "1\n2\n3\n");

    sp.start("1; 2; 3;");
    expr.reset(sp.parseModule());
    testEqual(repr(expr.get()), "1\n2\n3\n");

    sp.start("a[0] = 1");
    expr.reset(sp.parseModule());
    testEqual(repr(expr.get()), "a[0] s= 1\n");

    // test this parses as "not (a in b)" not "(not a) in b"
    sp.start("not a in b");
    expr.reset(sp.parseModule());
    testEqual(repr(expr.get()), "not a in b\n");
    testTrue(expr->is<SyntaxBlock>());
    testTrue(expr->as<SyntaxBlock>()->stmts()[0]->is<SyntaxNot>());

    sp.start("(1)");
    expr.reset(sp.parseModule());
    testEqual(repr(expr.get()), "1\n");

    sp.start("()");
    expr.reset(sp.parseModule());
    testEqual(repr(expr.get()), "()\n");

    sp.start("(1,)");
    expr.reset(sp.parseModule());
    testEqual(repr(expr.get()), "(1,)\n");

    sp.start("(1,2)");
    expr.reset(sp.parseModule());
    testEqual(repr(expr.get()), "(1, 2)\n");

    sp.start("foo(bar)");
    expr.reset(sp.parseModule());
    testEqual(repr(expr.get()), "foo(bar)\n");

    sp.start("foo(bar, baz)");
    expr.reset(sp.parseModule());
    testEqual(repr(expr.get()), "foo(bar, baz)\n");
}

#endif
