#include "parser.h"

#include "generator.h"
#include "syntax.h"
#include "repr.h"
#include "test.h"
#include "utils.h"

#include <cstdlib>
#include <stdexcept>
#include <ostream>

ParseError::ParseError(const Token& token, string message) :
  runtime_error(message), pos(token.pos)
{
#ifdef BUILD_TESTS
    maybeAbortTests("ParseError: " + message);
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
  : Parser(tokenizer), inFunction(false), isGenerator(false)
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

    addPrefixHandler(Token_Bra, [=] (ParserT& parser, Token token) -> unique_ptr<Syntax> {
        if (parser.opt(Token_Ket))
            return make_unique<SyntaxExprList>(token);
        unique_ptr<Syntax> result = parseExprOrExprList();
        parser.match(Token_Ket);
        return result;
    });

    addPrefixHandler(Token_SBra, [] (ParserT& parser, Token token) -> unique_ptr<Syntax> {
        vector<unique_ptr<Syntax>> elems =
            parser.exprListTrailing(Token_Comma, Token_SKet);
        return make_unique<SyntaxList>(token, move(elems));
    });

    addPrefixHandler(Token_CBra, [] (ParserT& parser, Token token) -> unique_ptr<Syntax> {
        vector<SyntaxDict::Entry> entries;
        while (!parser.opt(Token_CKet)) {
            unique_ptr<Syntax> key = parser.expression();
            parser.match(Token_Colon);
            unique_ptr<Syntax> value = parser.expression();
            entries.emplace_back(move(key), move(value));
            if (parser.opt(Token_CKet))
                break;
            parser.match(Token_Comma);
        }
        return make_unique<SyntaxDict>(token, move(entries));
    });

    // Lambda

    addPrefixHandler(Token_Lambda, [] (ParserT& parser, Token token) {
        vector<Parameter> params;
        if (!parser.opt(Token_Colon)) {
            // todo: may need to allow bracketed parameters here
            SyntaxParser& sp = static_cast<SyntaxParser&>(parser);
            params = sp.parseParameterList(Token_Colon);
        }
        unique_ptr<Syntax> expr(parser.expression());
        return make_unique<SyntaxLambda>(token, move(params), move(expr));
    });

    // Conditional expression

    addInfixHandler(Token_If, 90, [] (ParserT& parser, Token token, unique_ptr<Syntax> cond) {
        unique_ptr<Syntax> cons(parser.expression());
        parser.match(Token_Else);
        unique_ptr<Syntax> alt(parser.expression());
        return make_unique<SyntaxCond>(token, move(cond), move(cons), move(alt));
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
    addBinaryOp(Token_NotIn, 120, Assoc_Left, [] (Token token, unique_ptr<Syntax> l, unique_ptr<Syntax> r) {
        return make_unique<SyntaxNot>(token, make_unique<SyntaxIn>(token, move(l), move(r)));
    });
    addBinaryOp(Token_IsNot, 120, Assoc_Left, [] (Token token, unique_ptr<Syntax> l, unique_ptr<Syntax> r) {
        return make_unique<SyntaxNot>(token, make_unique<SyntaxIs>(token, move(l), move(r)));
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

    addInfixHandler(Token_SBra, 200, [] (ParserT& parser, Token token, unique_ptr<Syntax> l) {
        unique_ptr<Syntax> index(parser.expression());
        parser.match(Token_SKet);
        return make_unique<SyntaxSubscript>(token, move(l), move(index));
    });

    addInfixHandler(Token_Bra, 200, [] (ParserT& parser, Token token, unique_ptr<Syntax> l) {
        vector<unique_ptr<Syntax>> args;
        while (!parser.opt(Token_Ket)) {
            if (!args.empty())
                parser.match(Token_Comma);
            args.emplace_back(parser.expression());
        }
        return make_unique<SyntaxCall>(token, move(l), move(args));
    });

    addBinaryOp(Token_Period, 200, Assoc_Left, [] (Token token, unique_ptr<Syntax> l, unique_ptr<Syntax> r) {
        if (!r->is<SyntaxName>())
            throw ParseError(token, "Bad attribute reference");
        unique_ptr<SyntaxName> name(r.release()->as<SyntaxName>());
        return make_unique<SyntaxAttrRef>(token, move(l), move(name));
    });

    addPrefixHandler(Token_Return, [] (ParserT& parser,
                                            Token token) {
        unique_ptr<Syntax> expr;
        if (parser.notFollowedBy(Token_EOF) &&
            parser.notFollowedBy(Token_Newline) &&
            parser.notFollowedBy(Token_Dedent))
        {
            expr = parser.expression();
        }
        return make_unique<SyntaxReturn>(token, move(expr));
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

unique_ptr<Syntax> SyntaxParser::parseExprOrExprList()
{
    Token startToken = currentToken();
    vector<unique_ptr<Syntax>> exprs;
    exprs.emplace_back(expression());
    bool singleExpr = true;
    while (opt(Token_Comma)) {
        singleExpr = false;
        if (!maybeExprToken())
            break;
        exprs.emplace_back(expression());
    }
    if (singleExpr)
        return move(exprs[0]);
    return make_unique<SyntaxExprList>(startToken, move(exprs));
}

unique_ptr<SyntaxTarget> SyntaxParser::makeAssignTarget(unique_ptr<Syntax> target)
{
    if (target->is<SyntaxExprList>()) {
        unique_ptr<SyntaxExprList> exprs(target.release()->as<SyntaxExprList>());
        vector<unique_ptr<Syntax>>& elems = exprs->elems();
        vector<unique_ptr<SyntaxTarget>> targets;
        for (unsigned i = 0; i < elems.size(); i++)
            targets.push_back(makeAssignTarget(move(elems[i])));
        return make_unique<SyntaxTargetList>(exprs->token(), move(targets));
    } else if (target->is<SyntaxList>()) {
        unique_ptr<SyntaxList> list(unique_ptr_as<SyntaxList>(target));
        vector<unique_ptr<Syntax>>& elems = list->elems();
        vector<unique_ptr<SyntaxTarget>> targets;
        for (unsigned i = 0; i < elems.size(); i++)
            targets.push_back(makeAssignTarget(move(elems[i])));
        return make_unique<SyntaxTargetList>(list->token(), move(targets));
    } else if (target->is<SyntaxName>()) {
        return unique_ptr<SyntaxTarget>(unique_ptr_as<SyntaxName>(target));
    } else if (target->is<SyntaxAttrRef>()) {
        return unique_ptr_as<SyntaxAttrRef>(target);
    } else if (target->is<SyntaxSubscript>()) {
        return unique_ptr_as<SyntaxSubscript>(target);
    } else {
        throw ParseError(target->token(),
                         "Illegal target for assignment: " + target->name());
    }
}

unique_ptr<SyntaxTarget> SyntaxParser::parseTarget()
{
    // Unfortunately we have two different paths for parsing assignment targets.
    // This is called when we know we have to match a target, but the main
    // expression patch can also match an expression which is later
    // checked/converted by makeAssignTarget() above.

    Token token = currentToken();
    unique_ptr<Syntax> syn;
    if (opt(Token_Bra) | opt(Token_SBra)) {
        // Might be an expression before period or it might be a target list
        syn = parseExprOrExprList();
        match(token.type == Token_Bra ? Token_Ket : Token_SKet);
    } else {
        match(Token_Identifier);
        syn = make_unique<SyntaxName>(token);
    }

    for (;;) {
        if (opt(Token_Period)) {
            Token attr = match(Token_Identifier);
            unique_ptr<SyntaxName> name(make_unique<SyntaxName>(move(attr)));
            syn = make_unique<SyntaxAttrRef>(token, move(syn), move(name));
        } else if (opt(Token_SBra)) {
            unique_ptr<Syntax> index(expression());
            match(Token_SKet);
            syn = make_unique<SyntaxSubscript>(token, move(syn), move(index));
        } else {
            return makeAssignTarget(move(syn));
        }
    }
}

unique_ptr<SyntaxTarget> SyntaxParser::parseTargetList()
{
    Token startToken = currentToken();
    vector<unique_ptr<SyntaxTarget>> targets;
    targets.emplace_back(parseTarget());
    bool singleTarget = true;
    while (opt(Token_Comma)) {
        singleTarget = false;
        if (!maybeExprToken())
            break;
        targets.emplace_back(parseTarget());
    }
    if (singleTarget)
        return move(targets[0]);
    return make_unique<SyntaxTargetList>(startToken, move(targets));
}

unique_ptr<Syntax> SyntaxParser::parseAugAssign(Token token, unique_ptr<Syntax> syntax, BinaryOp op)
{
    unique_ptr<SyntaxSingleTarget> target;
    // todo: special case is/as for SyntaxSingleTarget
    if (syntax->is<SyntaxName>()) {
        target = unique_ptr_as<SyntaxName>(syntax);
    } else if (syntax->is<SyntaxAttrRef>()) {
        target = unique_ptr_as<SyntaxAttrRef>(syntax);
    } else if (syntax->is<SyntaxSubscript>()) {
        target = unique_ptr_as<SyntaxSubscript>(syntax);
    } else {
        throw ParseError(token,
                         "Illegal target for augmented assignment: " +
                         syntax->name());
    }

    unique_ptr<Syntax> expr(expression());
    return make_unique<SyntaxAugAssign>(token, move(target), move(expr), op);
}

unique_ptr<Syntax> SyntaxParser::parseAssignSource()
{
    unique_ptr<Syntax> expr = parseExprOrExprList();
    if (!opt(Token_Assign))
        return expr;

    Token token = currentToken();
    unique_ptr<SyntaxTarget> target = makeAssignTarget(move(expr));
    expr = parseAssignSource();
    return make_unique<SyntaxAssign>(token, move(target), move(expr));
}

unique_ptr<Syntax> SyntaxParser::parseSimpleStatement()
{
    Token token = currentToken();
    if (opt(Token_Assert)) {
        unique_ptr<Syntax> cond(expression());
        unique_ptr<Syntax> message;
        if (opt(Token_Comma))
            message = expression();
        return make_unique<SyntaxAssert>(token, move(cond), move(message));
    } else if (opt(Token_Pass)) {
        return make_unique<SyntaxPass>(token);
    } else if (opt(Token_Raise)) {
        unique_ptr<Syntax> expr(expression());
        if (opt(Token_Comma))
            throw ParseError(token,
                             "Multiple exressions for raise not supported"); // todo
        return make_unique<SyntaxRaise>(token, move(expr));
    } else if (opt(Token_Global)) {
        vector<Name> names;
        do {
            Token t = match(Token_Identifier);
            names.push_back(t.text);
        } while (opt(Token_Comma));
        return make_unique<SyntaxGlobal>(token, move(names));
    } else if (opt(Token_Yield)) {
        isGenerator = true;
        return make_unique<SyntaxYield>(token, expression());
    } else if (opt(Token_Break)) {
        return make_unique<SyntaxBreak>(token);
    } else if (opt(Token_Continue)) {
        return make_unique<SyntaxContinue>(token);
    }

    unique_ptr<Syntax> expr = parseExprOrExprList();
    if (opt(Token_Assign)) {
        unique_ptr<SyntaxTarget> target = makeAssignTarget(move(expr));
        return make_unique<SyntaxAssign>(token, move(target), parseAssignSource());
    } else if (opt(Token_AssignPlus)) {
        return parseAugAssign(token, move(expr), BinaryPlus);
    } else if (opt(Token_AssignMinus)) {
        return parseAugAssign(token, move(expr), BinaryMinus);
    } else if (opt(Token_AssignTimes)) {
        return parseAugAssign(token, move(expr), BinaryMultiply);
    } else if (opt(Token_AssignDivide)) {
        return parseAugAssign(token, move(expr), BinaryDivide);
    } else if (opt(Token_AssignIntDivide)) {
        return parseAugAssign(token, move(expr), BinaryIntDivide);
    } else if (opt(Token_AssignModulo)) {
        return parseAugAssign(token, move(expr), BinaryModulo);
    } else if (opt(Token_AssignPower)) {
        return parseAugAssign(token, move(expr), BinaryPower);
    } else if (opt(Token_AssignBitLeftShift)) {
        return parseAugAssign(token, move(expr), BinaryLeftShift);
    } else if (opt(Token_AssignBitRightShift)) {
        return parseAugAssign(token, move(expr), BinaryRightShift);
    } else if (opt(Token_AssignBitAnd)) {
        return parseAugAssign(token, move(expr), BinaryAnd);
    } else if (opt(Token_AssignBitXor)) {
        return parseAugAssign(token, move(expr), BinaryXor);
    } else if (opt(Token_AssignBitOr)) {
        return parseAugAssign(token, move(expr), BinaryOr);
    }

    return expr;
}

vector<Parameter> SyntaxParser::parseParameterList(TokenType endToken)
{
    vector<Parameter> params;
    bool hadDefault = false;
    bool hadRest = false;
    while (!opt(endToken)) {
        // todo: kwargs
        bool takesRest = false;
        if (opt(Token_Times))
            takesRest = true;
        Token t = match(Token_Identifier);
        if (hadRest)
            throw ParseError(t, "Parameter following rest parameter");
        Name name = t.text;
        unique_ptr<Syntax> defaultExpr;
        if (opt(Token_Assign)) {
            if (takesRest)
                throw ParseError(t, "Rest parameter can't take default");
            defaultExpr = expression();
            hadDefault = true;
        } else if (hadDefault && !takesRest) {
            throw ParseError(t, "Non-default parameter following default parameter");
        }
        for (auto i = params.begin(); i != params.end(); i++)
            if (name == i->name)
                throw ParseError(t, "Duplicate parameter name: " + name);
        params.emplace_back(name, move(defaultExpr), takesRest);
        if (opt(endToken))
            break;
        match(Token_Comma);
        hadRest = takesRest;
    }
    return params;
}

unique_ptr<Syntax> SyntaxParser::parseCompoundStatement()
{
    Token token = currentToken();
    if (opt(Token_If)) {
        unique_ptr<SyntaxIf> ifStmt = make_unique<SyntaxIf>(token);
        do {
            unique_ptr<Syntax> cond(expression());
            ifStmt->addBranch(move(cond), parseSuite());
        } while (opt(Token_Elif));
        if (opt(Token_Else))
            ifStmt->setElse(parseSuite());
        return move(ifStmt);
    } else if (opt(Token_While)) {
        unique_ptr<Syntax> cond(expression());
        unique_ptr<SyntaxBlock> suite(parseSuite());
        unique_ptr<SyntaxBlock> elseSuite;
        if (opt(Token_Else))
            elseSuite = parseSuite();
        return make_unique<SyntaxWhile>(token, move(cond), move(suite),
                               move(elseSuite));
    } else if (opt(Token_For)) {
        unique_ptr<SyntaxTarget> targets(parseTargetList());
        match(Token_In);
        unique_ptr<Syntax> exprs(parseExprOrExprList());
        unique_ptr<SyntaxBlock> suite(parseSuite());
        unique_ptr<SyntaxBlock> elseSuite;
        if (opt(Token_Else))
            elseSuite = parseSuite();
        return make_unique<SyntaxFor>(token, move(targets), move(exprs), move(suite),
                             move(elseSuite));
    } else if (opt(Token_Try)) {
        Token token = currentToken();
        unique_ptr<SyntaxBlock> suite(parseSuite());
        unique_ptr<SyntaxBlock> elseSuite;
        vector<unique_ptr<ExceptInfo>> excepts;
        unique_ptr<SyntaxBlock> finallySuite;
        bool hadExpr = true;
        while (opt(Token_Except)) {
            if (!hadExpr) {
                throw ParseError(token,
                                 "Expresion-less except clause must be last");
            }
            Token token = currentToken();
            unique_ptr<Syntax> expr;
            unique_ptr<SyntaxTarget> as;
            if (token.type != Token_Colon) {
                expr = parseExprOrExprList();
                if (opt(Token_As))
                    as = parseTarget();
            } else {
                hadExpr = false;
            }
            unique_ptr<SyntaxBlock> suite(parseSuite());
            excepts.emplace_back(new ExceptInfo(token, move(expr), move(as), move(suite)));
        }
        if (!excepts.empty() && opt(Token_Else))
            elseSuite = parseSuite();
        if (opt(Token_Finally))
            finallySuite = parseSuite();
        return make_unique<SyntaxTry>(token, move(suite), move(excepts), move(elseSuite), move(finallySuite));
    //} else if (opt(Token_With)) {
    } else if (opt(Token_Def)) {
        Token name = match(Token_Identifier);
        match(Token_Bra);
        vector<Parameter> params = parseParameterList(Token_Ket);
        AutoSetAndRestore asar1(inFunction, true);
        AutoSetAndRestore asar2(isGenerator, false);
        unique_ptr<Syntax> suite(parseSuite());
        return make_unique<SyntaxDef>(token, name.text, move(params), move(suite), move(isGenerator));
    } else if (opt(Token_Class)) {
        Token name = match(Token_Identifier);
        vector<unique_ptr<Syntax>> bases;
        if (opt(Token_Bra))
            bases = exprListTrailing(Token_Comma, Token_Ket);
        unique_ptr<SyntaxExprList> baseList(make_unique<SyntaxExprList>(token, move(bases)));
        unique_ptr<SyntaxBlock> suite(parseSuite());
        return make_unique<SyntaxClass>(token, name.text, move(baseList), move(suite));
    } else {
        unique_ptr<Syntax> stmt = parseSimpleStatement();
        if (!atEnd())
            matchEither(Token_Newline, Token_Semicolon);
        return stmt;
    }
}

unique_ptr<SyntaxBlock> SyntaxParser::parseBlock()
{
    Token token = currentToken();
    vector<unique_ptr<Syntax>> stmts;
    match(Token_Indent);
    while (!opt(Token_Dedent)) {
        stmts.emplace_back(parseCompoundStatement());
    }
    return make_unique<SyntaxBlock>(token, move(stmts));
}

unique_ptr<SyntaxBlock> SyntaxParser::parseSuite()
{
    Token token = currentToken();
    vector<unique_ptr<Syntax>> stmts;
    match(Token_Colon);
    if (opt(Token_Newline)) {
        match(Token_Indent);
        while (!opt(Token_Dedent)) {
            stmts.emplace_back(parseCompoundStatement());
        }
    } else {
        do {
            stmts.emplace_back(parseSimpleStatement());
            if (atEnd() || opt(Token_Newline))
                break;
            match(Token_Semicolon);
        } while (!atEnd() && !opt(Token_Newline));
    }
    return make_unique<SyntaxBlock>(token, move(stmts));
}

unique_ptr<SyntaxBlock> SyntaxParser::parseModule()
{
    Token token = currentToken();
    vector<unique_ptr<Syntax>> stmts;
    while (!atEnd())
        stmts.emplace_back(parseCompoundStatement());
    return make_unique<SyntaxBlock>(token, move(stmts));
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

    testParseModule("1\n2", "1\n2");
    testParseModule("1; 2; 3", "1\n2\n3");
    testParseModule("1; 2; 3;", "1\n2\n3");
    testParseModule("a[0] = 1", "a[0] = 1");

    // test this parses as "not (a in b)" not "(not a) in b"
    sp.start("not a in b");
    expr = sp.parseModule();
    testEqual(repr(*expr.get()), "not a in b");
    testTrue(expr->is<SyntaxBlock>());
    testTrue(expr->as<SyntaxBlock>()->stmts()[0]->is<SyntaxNot>());

    testParseExpression("a.x", "a.x");
    testParseExpression("(a + 1).x", "(a + 1).x");

    testParseModule("(1)", "1");
    testParseModule("()", "()");
    testParseModule("(1,)", "(1,)");
    testParseModule("(1,2)", "(1, 2)");
    testParseModule("foo(bar)", "foo(bar)");
    testParseModule("foo(bar, baz)", "foo(bar, baz)");
    testParseModule("a, b = 1, 2", "(a, b) = (1, 2)");
    testParseModule("a = b = 1", "a = b = 1");

    testParseModule("for x in []:\n  pass", "for x in []:\npass");
    testParseModule("for a.x in []:\n  pass", "for a.x in []:\npass");
    testParseModule("for (a + 1).x in []:\n  pass", "for (a + 1).x in []:\npass");
    testParseModule("for (a, b) in []:\n  pass", "for (a, b) in []:\npass");
    testParseModule("for (a, b).x in []:\n  pass", "for (a, b).x in []:\npass");
    testParseModule("for a[0][0] in []:\n  pass", "for a[0][0] in []:\npass");
    testParseModule("for a.b.c in []:\n  pass", "for a.b.c in []:\npass");
    testParseException("for 1 in []: pass");
    testParseException("for (a + 1) in []: pass");

    testParseModule("def f(): pass", "def f():\npass");
    testParseModule("def f(x): pass", "def f(x):\npass");
    testParseModule("def f(x,): pass", "def f(x):\npass");
    testParseModule("def f(x = 0): pass", "def f(x = 0):\npass");
    testParseModule("def f(x, y): pass", "def f(x, y):\npass");
    testParseModule("def f(x, y = 1): pass", "def f(x, y = 1):\npass");
    testParseModule("def f(x = 0, y = 1): pass", "def f(x = 0, y = 1):\npass");
    testParseModule("def f(*x): pass", "def f(*x):\npass");
    testParseModule("def f(x, *y): pass", "def f(x, *y):\npass");
    testParseModule("def f(x = 0, *y): pass", "def f(x = 0, *y):\npass");
    testParseException("def f(x, x): pass");
    testParseException("def f(x = 0, y): pass");
    testParseException("def f(*x, y): pass");
    testParseException("def f(*x = ()): pass");
}

#endif
