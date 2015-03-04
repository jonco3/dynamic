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
  : Parser(tokenizer)
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
    createNodeForBinary<SyntaxCompareOp>(Token_LT, 120, Assoc_Left, CompareLT);
    createNodeForBinary<SyntaxCompareOp>(Token_LE, 120, Assoc_Left, CompareLE);
    createNodeForBinary<SyntaxCompareOp>(Token_GT, 120, Assoc_Left, CompareGT);
    createNodeForBinary<SyntaxCompareOp>(Token_GE, 120, Assoc_Left, CompareGE);
    createNodeForBinary<SyntaxCompareOp>(Token_EQ, 120, Assoc_Left, CompareEQ);
    createNodeForBinary<SyntaxCompareOp>(Token_NE, 120, Assoc_Left, CompareNE);
    addBinaryOp(Token_NotIn, 120, Assoc_Left, [] (Token token, Syntax* l, Syntax* r) {
        return new SyntaxNot(token, new SyntaxIn(token, l, r));
    });
    addBinaryOp(Token_IsNot, 120, Assoc_Left, [] (Token token, Syntax* l, Syntax* r) {
        return new SyntaxNot(token, new SyntaxIs(token, l, r));
    });

    // Bitwise binary operators

    createNodeForBinary<SyntaxBinaryOp>(Token_BitOr, 130, Assoc_Left, BinaryOr);
    createNodeForBinary<SyntaxBinaryOp>(Token_BitXor, 140, Assoc_Left, BinaryXor);
    createNodeForBinary<SyntaxBinaryOp>(Token_BitAnd, 150, Assoc_Left, BinaryAnd);
    createNodeForBinary<SyntaxBinaryOp>(Token_BitLeftShift, 160, Assoc_Left,
                                        BinaryLeftShift);
    createNodeForBinary<SyntaxBinaryOp>(Token_BitRightShift, 160, Assoc_Left,
                                        BinaryRightShift);

    // Arithermetic binary operators

    createNodeForBinary<SyntaxBinaryOp>(Token_Plus, 170, Assoc_Left, BinaryPlus);
    createNodeForBinary<SyntaxBinaryOp>(Token_Minus, 170, Assoc_Left, BinaryMinus);
    createNodeForBinary<SyntaxBinaryOp>(Token_Times, 180, Assoc_Left, BinaryMultiply);
    createNodeForBinary<SyntaxBinaryOp>(Token_Divide, 180, Assoc_Left, BinaryDivide);
    createNodeForBinary<SyntaxBinaryOp>(Token_IntDivide, 180, Assoc_Left,
                                        BinaryIntDivide);
    createNodeForBinary<SyntaxBinaryOp>(Token_Modulo, 180, Assoc_Left, BinaryModulo);
    createNodeForBinary<SyntaxBinaryOp>(Token_Power, 180, Assoc_Left, BinaryPower);

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
            throw ParseError("Bad attribute reference");
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

SyntaxTarget* SyntaxParser::makeAssignTarget(Syntax* target)
{
    if (target->is<SyntaxExprList>()) {
        SyntaxExprList* exprs = target->as<SyntaxExprList>();
        const vector<Syntax*>& elems = exprs->elems();
        vector<SyntaxTarget*> targets(elems.size());
        for (unsigned i = 0; i < elems.size(); i++)
            targets[i] = makeAssignTarget(elems[i]);
        SyntaxTargetList* result = new SyntaxTargetList(exprs->token(), targets);
        exprs->release();
        delete exprs;
        return result;
    } else if (target->is<SyntaxList>()) {
        SyntaxList* list = target->as<SyntaxList>();
        const vector<Syntax*>& elems = list->elems();
        vector<SyntaxTarget*> targets(elems.size());
        for (unsigned i = 0; i < elems.size(); i++)
            targets[i] = makeAssignTarget(elems[i]);
        SyntaxTargetList* result = new SyntaxTargetList(list->token(), targets);
        list->release();
        delete list;
        return result;
    } else if (target->is<SyntaxName>()) {
        return target->as<SyntaxName>();
    } else if (target->is<SyntaxAttrRef>()) {
        return target->as<SyntaxAttrRef>();
    } else if (target->is<SyntaxSubscript>()) {
        return target->as<SyntaxSubscript>();
    } else {
        throw ParseError("Illegal target for assignment: " + target->name());
    }
}

SyntaxTarget* SyntaxParser::parseTarget()
{
    // Unfortunately we have two different paths for parsing assignment targets.
    // This is called when we know we have to match a target, but the main
    // expression patch can also match an expression which is later
    // checked/converted by makeAssignTarget() above.

    Token token = currentToken();
    Syntax* syn = nullptr;
    if (opt(Token_Bra) | opt(Token_SBra)) {
        // Might be an expression before period or it might be a target list
        syn = parseExprOrExprList();
        match(token.type == Token_Bra ? Token_Ket : Token_SKet);
    } else {
        match(Token_Identifier);
        syn = new SyntaxName(token);
    }

    for (;;) {
        if (opt(Token_Period)) {
            Token attr = match(Token_Identifier);
            syn = new SyntaxAttrRef(token, syn, new SyntaxName(attr));
        } else if (opt(Token_SBra)) {
            Syntax* index = expression();
            match(Token_SKet);
            syn = new SyntaxSubscript(token, syn, index);
        } else {
            return makeAssignTarget(syn);
        }
    }
}

SyntaxTarget* SyntaxParser::parseTargetList()
{
    Token startToken = currentToken();
    vector<SyntaxTarget*> targets;
    targets.push_back(parseTarget());
    bool singleTarget = true;
    while (opt(Token_Comma)) {
        singleTarget = false;
        if (!maybeExprToken())
            break;
        targets.push_back(parseTarget());
    }
    if (singleTarget)
        return targets[0];
    return new SyntaxTargetList(startToken, targets);
}

Syntax* SyntaxParser::parseAugAssign(Token token, Syntax* syntax, BinaryOp op)
{
    SyntaxSingleTarget* target = nullptr;
    if (syntax->is<SyntaxName>()) {
        target = syntax->as<SyntaxName>();
    } else if (syntax->is<SyntaxAttrRef>()) {
        target = syntax->as<SyntaxAttrRef>();
    } else if (syntax->is<SyntaxSubscript>()) {
        target = syntax->as<SyntaxSubscript>();
    } else {
        throw ParseError("Illegal target for augmented assignment: " + syntax->name());
    }

    return new SyntaxAugAssign(token, target, expression(), op);
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
    } else if (opt(Token_Raise)) {
        Syntax* expr = expression();
        if (opt(Token_Comma))
            throw ParseError("Multiple exressions for raise not supported"); // todo
        return new SyntaxRaise(token, expr);
    } else if (opt(Token_Global)) {
        vector<Name> names;
        do {
            Token t = match(Token_Identifier);
            names.push_back(t.text);
        } while (opt(Token_Comma));
        return new SyntaxGlobal(token, names);
    }

    Syntax* expr = parseExprOrExprList();
    if (opt(Token_Assign)) {
        SyntaxTarget* target = makeAssignTarget(expr);
        // todo: should admit yield statement too but we haven't implemented that yet
        return new SyntaxAssign(token, target, parseExprOrExprList());
    } else if (opt(Token_AssignPlus)) {
        return parseAugAssign(token, expr, BinaryPlus);
    } else if (opt(Token_AssignMinus)) {
        return parseAugAssign(token, expr, BinaryMinus);
    } else if (opt(Token_AssignTimes)) {
        return parseAugAssign(token, expr, BinaryMultiply);
    } else if (opt(Token_AssignDivide)) {
        return parseAugAssign(token, expr, BinaryDivide);
    } else if (opt(Token_AssignIntDivide)) {
        return parseAugAssign(token, expr, BinaryIntDivide);
    } else if (opt(Token_AssignModulo)) {
        return parseAugAssign(token, expr, BinaryModulo);
    } else if (opt(Token_AssignPower)) {
        return parseAugAssign(token, expr, BinaryPower);
    } else if (opt(Token_AssignBitLeftShift)) {
        return parseAugAssign(token, expr, BinaryLeftShift);
    } else if (opt(Token_AssignBitRightShift)) {
        return parseAugAssign(token, expr, BinaryRightShift);
    } else if (opt(Token_AssignBitAnd)) {
        return parseAugAssign(token, expr, BinaryAnd);
    } else if (opt(Token_AssignBitXor)) {
        return parseAugAssign(token, expr, BinaryXor);
    } else if (opt(Token_AssignBitOr)) {
        return parseAugAssign(token, expr, BinaryOr);
    }

    return expr;
}

Syntax* SyntaxParser::parseCompoundStatement()
{
    Token token = currentToken();
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
        return ifStmt;
    } else if (opt(Token_While)) {
        Syntax* cond = expression();
        SyntaxBlock* suite = parseSuite();
        SyntaxBlock* elseSuite = nullptr;
        if (opt(Token_Else))
            elseSuite = parseSuite();
        return new SyntaxWhile(token, cond, suite, elseSuite);
    } else if (opt(Token_For)) {
        SyntaxTarget* targets = parseTargetList();
        match(Token_In);
        Syntax* exprs = parseExprOrExprList();
        SyntaxBlock* suite = parseSuite();
        SyntaxBlock* elseSuite = nullptr;
        if (opt(Token_Else))
            elseSuite = parseSuite();
        return new SyntaxFor(token, targets, exprs, suite, elseSuite);
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
        Syntax* stmt = parseSimpleStatement();
        if (!atEnd())
            matchEither(Token_Newline, Token_Semicolon);
        return stmt;
    }
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

static void testParseExpression(const string& input, const string& expected)
{
    SyntaxParser sp;
    sp.start(input);
    unique_ptr<Syntax> expr(sp.expression());
    testEqual(repr(*expr.get()), expected);
}

static void testParseModule(const string& input, const string& expected)
{
    SyntaxParser sp;
    sp.start(input);
    unique_ptr<Syntax> expr(sp.parseModule());
    testEqual(repr(*expr.get()), expected);
}

static void testParseException(const char* input)
{
    SyntaxParser sp;
    sp.start(input);
    testThrows(sp.parseModule(), ParseError);
}

testcase(parser)
{
    Tokenizer tokenizer;
    Parser<int> parser(tokenizer);
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

    testParseExpression("1+2-3", "1 + 2 - 3");

    testParseExpression("4 ** 5", "4 ** 5");

    SyntaxParser sp;
    sp.start("f = 1 + 2");
    unique_ptr<Syntax> expr(sp.parseCompoundStatement());
    testTrue(expr.get()->is<SyntaxAssign>());
    testTrue(expr.get()->as<SyntaxAssign>()->left()->is<SyntaxName>());

    testParseModule("1\n2", "1\n2\n");
    testParseModule("1; 2; 3", "1\n2\n3\n");
    testParseModule("1; 2; 3;", "1\n2\n3\n");
    testParseModule("a[0] = 1", "a[0] = 1\n");

    // test this parses as "not (a in b)" not "(not a) in b"
    sp.start("not a in b");
    expr.reset(sp.parseModule());
    testEqual(repr(*expr.get()), "not a in b\n");
    testTrue(expr->is<SyntaxBlock>());
    testTrue(expr->as<SyntaxBlock>()->stmts()[0]->is<SyntaxNot>());

    testParseExpression("a.x", "a.x");
    testParseExpression("(a + 1).x", "(a + 1).x");

    testParseModule("(1)", "1\n");
    testParseModule("()", "()\n");
    testParseModule("(1,)", "(1,)\n");
    testParseModule("(1,2)", "(1, 2)\n");
    testParseModule("foo(bar)", "foo(bar)\n");
    testParseModule("foo(bar, baz)", "foo(bar, baz)\n");
    testParseModule("a, b = 1, 2", "(a, b) = (1, 2)\n");

    testParseModule("for x in []:\n  pass", "for x in []:\npass\n\n");
    testParseModule("for a.x in []:\n  pass", "for a.x in []:\npass\n\n");
    testParseModule("for (a + 1).x in []:\n  pass", "for (a + 1).x in []:\npass\n\n");
    testParseModule("for (a, b) in []:\n  pass", "for (a, b) in []:\npass\n\n");
    testParseModule("for (a, b).x in []:\n  pass", "for (a, b).x in []:\npass\n\n");
    testParseModule("for a[0][0] in []:\n  pass", "for a[0][0] in []:\npass\n\n");
    testParseModule("for a.b.c in []:\n  pass", "for a.b.c in []:\npass\n\n");
    testParseException("for 1 in []: pass");
    testParseException("for (a + 1) in []: pass");
}

#endif
