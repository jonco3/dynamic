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
vector<T> Parser<T>::exprList(TokenType separator, TokenType end)
{
    vector<T> exprs;
    if (opt(end))
        return exprs;

    for (;;) {
        exprs.push_back(expression());
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
vector<T> Parser<T>::exprListTrailing(TokenType separator,
                                      TokenType end)
{
    vector<T> exprs;
    while (!opt(end)) {
        exprs.push_back(expression());
        if (opt(end))
            break;
        match(separator);
    }
    return exprs;
}

SyntaxParser::SyntaxParser()
  : Parser(tokenizer, TokenCount)
{
    // Atoms

    createNodeForAtom<SyntaxName>(Token_Identifier);
    createNodeForAtom<SyntaxString>(Token_String);
    createNodeForAtom<SyntaxInteger>(Token_Integer);
    // todo: longinteger floatnumber imagnumber

    // Unary operations

    // todo: check the precedence of these works as expected
    createNodeForUnary<SyntaxPos>(Token_Plus);
    createNodeForUnary<SyntaxNeg>(Token_Minus);
    createNodeForUnary<SyntaxNot>(Token_Not, 0);
    createNodeForUnary<SyntaxInvert>(Token_BitNot);

    // Displays

    addPrefixHandler(Token_Bra, [=] (ParserT& parser, Token token) -> Syntax* {
        if (parser.opt(Token_Ket))
            return new SyntaxExprList(token);
        Syntax* result = parseExprOrExprList();
        parser.match(Token_Ket);
        return result;
    });

    addPrefixHandler(Token_SBra, [] (ParserT& parser, Token token) -> Syntax* {
        return new SyntaxList(token,
                              parser.exprListTrailing(Token_Comma, Token_SKet));
    });

    addPrefixHandler(Token_CBra, [] (ParserT& parser, Token token) -> Syntax* {
        vector<SyntaxDict::Entry> entries;
        while (!parser.opt(Token_CKet)) {
            Syntax* key = parser.expression();
            parser.match(Token_Colon);
            Syntax* value = parser.expression();
            entries.push_back(SyntaxDict::Entry(key, value));
            if (parser.opt(Token_CKet))
                break;
            parser.match(Token_Comma);
        }
        return new SyntaxDict(token, entries);
    });

    // Lambda

    addPrefixHandler(Token_Lambda, [] (ParserT& parser, Token token) {
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
        return new SyntaxLambda(token, params, parser.expression());
    });

    // Conditional expression

    addInfixHandler(Token_If, 90, [] (ParserT& parser, Token token, Syntax* cond) {
        Syntax* cons = parser.expression();
        parser.match(Token_Else);
        Syntax* alt = parser.expression();
        return new SyntaxCond(token, cond, cons, alt);
    });

    // Boolean operators

    createNodeForBinary<SyntaxOr>(Token_Or, 100, Assoc_Left);
    createNodeForBinary<SyntaxAnd>(Token_And, 100, Assoc_Left);

    // Comparison operators

    createNodeForBinary<SyntaxIn>(Token_In, 120, Assoc_Left);
    createNodeForBinary<SyntaxIs>(Token_Is, 120, Assoc_Left);
    createNodeForBinary<SyntaxLT>(Token_LT, 120, Assoc_Left);
    createNodeForBinary<SyntaxLE>(Token_LE, 120, Assoc_Left);
    createNodeForBinary<SyntaxGT>(Token_GT, 120, Assoc_Left);
    createNodeForBinary<SyntaxGE>(Token_GE, 120, Assoc_Left);
    createNodeForBinary<SyntaxEQ>(Token_EQ, 120, Assoc_Left);
    createNodeForBinary<SyntaxNE>(Token_NE, 120, Assoc_Left);
    addBinaryOp(Token_NotIn, 120, Assoc_Left, [] (Token token, Syntax* l, Syntax* r) {
        return new SyntaxNot(token, new SyntaxIn(token, l, r));
    });
    addBinaryOp(Token_IsNot, 120, Assoc_Left, [] (Token token, Syntax* l, Syntax* r) {
        return new SyntaxNot(token, new SyntaxIs(token, l, r));
    });

    // Bitwise binary operators

    createNodeForBinary<SyntaxBitOr>(Token_BitOr, 130, Assoc_Left);
    createNodeForBinary<SyntaxBitXor>(Token_BitXor, 140, Assoc_Left);
    createNodeForBinary<SyntaxBitAnd>(Token_BitAnd, 150, Assoc_Left);
    createNodeForBinary<SyntaxBitLeftShift>(Token_BitLeftShift, 160, Assoc_Left);
    createNodeForBinary<SyntaxBitRightShift>(Token_BitRightShift, 160, Assoc_Left);

    // Arithermetic binary operators

    createNodeForBinary<SyntaxPlus>(Token_Plus, 170, Assoc_Left);
    createNodeForBinary<SyntaxMinus>(Token_Minus, 170, Assoc_Left);
    createNodeForBinary<SyntaxMultiply>(Token_Times, 180, Assoc_Left);
    createNodeForBinary<SyntaxDivide>(Token_Divide, 180, Assoc_Left);
    createNodeForBinary<SyntaxIntDivide>(Token_IntDivide, 180, Assoc_Left);
    createNodeForBinary<SyntaxModulo>(Token_Modulo, 180, Assoc_Left);
    createNodeForBinary<SyntaxPower>(Token_Power, 180, Assoc_Left);

    addInfixHandler(Token_SBra, 200, [] (ParserT& parser, Token token, Syntax* l) {
        Syntax* index = parser.expression();
        parser.match(Token_SKet);
        return new SyntaxSubscript(token, l, index);
    });

    addInfixHandler(Token_Bra, 200, [] (ParserT& parser, Token token, Syntax* l) {
        vector<Syntax*> args;
        while (!parser.opt(Token_Ket)) {
            if (!args.empty())
                parser.match(Token_Comma);
            args.push_back(parser.expression());
        }
        return new SyntaxCall(token, l, args);
    });

    addBinaryOp(Token_Period, 200, Assoc_Left, [] (Token token, Syntax* l, Syntax* r) {
        if (!r->is<SyntaxName>())
            throw ParseError("Bad property reference");
        return new SyntaxAttrRef(token, l, r->as<SyntaxName>());
    });

    addPrefixHandler(Token_Return, [] (ParserT& parser,
                                            Token token) {
        Syntax *expr = nullptr;
        if (parser.notFollowedBy(Token_EOF) &&
            parser.notFollowedBy(Token_Newline) &&
            parser.notFollowedBy(Token_Dedent))
        {
            expr = parser.expression();
        }
        return new SyntaxReturn(token, expr);
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
    exprs.push_back(expression());
    bool singleExpr = true;
    while (opt(Token_Comma)) {
        singleExpr = false;
        if (!maybeExprToken())
            break;
        exprs.push_back(expression());
    }
    if (singleExpr)
        return exprs[0];
    return new SyntaxExprList(startToken, exprs);
}

void SyntaxParser::checkAssignTarget(Syntax* target)
{
    if (target->is<SyntaxExprList>()) {
        SyntaxExprList* exprs = target->as<SyntaxExprList>();
        const vector<Syntax*>& targets = exprs->elems();
        for (auto i = targets.begin(); i != targets.end(); i++)
            checkAssignTarget(*i);
    } else if (target->is<SyntaxList>()) {
        SyntaxList* list = target->as<SyntaxList>();
        const vector<Syntax*>& targets = list->elems();
        for (auto i = targets.begin(); i != targets.end(); i++)
            checkAssignTarget(*i);
    } else if (!target->is<SyntaxName>() &&
               !target->is<SyntaxAttrRef>() &&
               !target->is<SyntaxSubscript>()) {
        throw ParseError("Illegal target for assignment");
    }
}

Syntax* SyntaxParser::parseSimpleStatement()
{
    Token token = currentToken();
    if (opt(Token_Assert)) {
        Syntax* cond = expression();
        Syntax* message = nullptr;
        if (opt(Token_Comma))
            message = expression();
        return new SyntaxAssert(token, cond, message);
    } else if (opt(Token_Pass)) {
        return new SyntaxPass(token);
    }

    Syntax* expr = parseExprOrExprList();
    if (opt(Token_Assign)) {
        checkAssignTarget(expr);
        Syntax *target = expr;
        expr = expression();
        if (target->is<SyntaxName>())
            return new SyntaxAssignName(token, target->as<SyntaxName>(), expr);
        else if (target->is<SyntaxAttrRef>())
            return new SyntaxAssignAttr(token, target->as<SyntaxAttrRef>(), expr);
        else if (target->is<SyntaxSubscript>())
            return new SyntaxAssignSubscript(token, target->as<SyntaxSubscript>(), expr);
        else
            throw ParseError("Assignment to target list not implemented");
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
            Syntax* cond = expression();
            SyntaxBlock* suite = parseSuite();
            ifStmt->addBranch(cond, suite);
        } while (opt(Token_Elif));
        if (opt(Token_Else)) {
            ifStmt->setElse(parseSuite());
        }
        stmt = ifStmt;
    } else if (opt(Token_While)) {
        Syntax* cond = expression();
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
            bases = exprListTrailing(Token_Comma, Token_Ket);
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
    Tokenizer tokenizer;
    Parser<int> parser(tokenizer, TokenCount);
    parser.addWord(Token_Integer, [] (Token token) {
        return atoi(token.text.c_str());  // todo: what's the c++ way to do this?
    });
    parser.addBinaryOp(Token_Plus, 10, Assoc_Left, [] (Token _, int l, int r) {
        return l + r;
    });
    parser.addBinaryOp(Token_Minus, 10, Assoc_Left, [] (Token _, int l, int r) {
        return l - r;
    });
    parser.addUnaryOp(Token_Minus, [] (Token _, int r) {
        return -r;
    });

    parser.start("2 + 3 - 1");
    testFalse(parser.atEnd());
    testEqual(parser.parse(), 4);
    testTrue(parser.atEnd());
    testThrows(parser.parse(), ParseError);

    parser.start("1 2");
    testEqual(parser.parse(), 1);
    testEqual(parser.parse(), 2);
    testTrue(parser.atEnd());
    testThrows(parser.parse(), ParseError);

    SyntaxParser sp;
    sp.start("1+2-3");
    unique_ptr<Syntax> expr(sp.expression());
    testEqual(repr(expr.get()), "1 + 2 - 3");

    sp.start("4 ** 5");
    expr.reset(sp.expression());
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
