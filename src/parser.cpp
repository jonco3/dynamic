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
    maybeAbortTests(*this);
}

SyntaxParser::SyntaxParser() :
  Parser(tokenizer),
  expr(TokenCount),
  statement(TokenCount)
{
    expr.addWord(Token_Integer, [] (Token token) {
        return new SyntaxInteger(atoi(token.text.c_str()));
    });

    expr.addWord(Token_Identifier, [] (Token token) {
        return new SyntaxName(token.text.c_str());
    });

    // Unary operations

    // todo: check the precedence of these works as expected
    expr.createNodeForUnary<SyntaxPos>(Token_Plus);
    expr.createNodeForUnary<SyntaxNeg>(Token_Minus);
    expr.createNodeForUnary<SyntaxInvert>(Token_Not);

    // Displays

    expr.addPrefixHandler(Token_Bra, [] (ParserT& parser, const Actions& acts,
                                         Token _) {
        Syntax* content = parser.expression(acts);
        parser.match(Token_Ket);
        // todo: parse tuples
        return content;
    });

    // Assignments

    expr.addBinaryOp(Token_Assign, 10, Assoc_Right,
                     [] (Token _, Syntax* l, Syntax* r) -> Syntax* {
        if (l->is<SyntaxName>())
            return new SyntaxAssignName(l->as<SyntaxName>(), r);
        else if (l->is<SyntaxPropRef>())
            return new SyntaxAssignProp(l->as<SyntaxPropRef>(), r);
        else
            throw ParseError("Illegal LHS for assignment");
    });

    // Lambda

    expr.addPrefixHandler(Token_Lambda, [] (ParserT& parser, const Actions& acts,
                                            Token _) {
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
        return new SyntaxLambda(params, parser.expression(acts));
    });

    // Conditional expression

    expr.addInfixHandler(Token_If, 90,
                         [] (ParserT& parser, const Actions& acts, Token _, Syntax* cond)
    {
        Syntax* cons = parser.expression(acts);
        parser.match(Token_Else);
        Syntax* alt = parser.expression(acts);
        return new SyntaxCond(cond, cons, alt);
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
    expr.addBinaryOp(Token_NotIn, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxNot(new SyntaxIn(l, r));
    });
    expr.addBinaryOp(Token_IsNot, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxNot(new SyntaxIs(l, r));
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

    expr.addInfixHandler(Token_Bra, 200, [] (ParserT& parser, const Actions& acts,
                                            Token _, Syntax* l) {
        vector<Syntax*> args;
        while (!parser.opt(Token_Ket)) {
            if (!args.empty())
                parser.match(Token_Comma);
            args.push_back(parser.expression(acts));
        }
        return new SyntaxCall(l, args);
    });

    expr.addBinaryOp(Token_Period, 200, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        if (!r->is<SyntaxName>())
            throw ParseError("Bad property reference");
        return new SyntaxPropRef(l, r->as<SyntaxName>());
    });

    expr.addPrefixHandler(Token_Return, [] (ParserT& parser, const Actions& acts,
                                            Token _) {
        Syntax *expr;
        if (parser.notFollowedBy(Token_EOF) &&
            parser.notFollowedBy(Token_Newline) &&
            parser.notFollowedBy(Token_Dedent))
        {
            expr = parser.expression(acts);
        } else {
            expr = new SyntaxInteger(0); // todo: none
        }
        return new SyntaxReturn(expr);
    });

    // Statements

    statement = expr;

    // todo: assignment is really a statement

    statement.addPrefixHandler(Token_If, [&] (ParserT& parser, const Actions& acts,
                                             Token _) {
        SyntaxIf* ifStmt = new SyntaxIf;
        do {
            Syntax* cond = parser.expression(expr);
            parser.match(Token_Colon);
            parser.match(Token_Newline);  // todo: this is optional
            SyntaxBlock* block = parseBlock();
            ifStmt->addBranch(cond, block);
        } while (parser.opt(Token_Elif));
        if (parser.opt(Token_Else)) {
            parser.match(Token_Colon);
            parser.match(Token_Newline);  // todo: this is optional
            ifStmt->setElse(parseBlock());
        }
        return ifStmt;
    });
}

SyntaxBlock* SyntaxParser::parseTopLevel()
{
    unique_ptr<SyntaxBlock> syntax(new SyntaxBlock);
    while (!atEnd()) {
        syntax->append(parseStatement());
        if (atEnd())
            break;
        match(Token_Newline);
    }
    return syntax.release();
}

SyntaxBlock* SyntaxParser::parseBlock()
{
    unique_ptr<SyntaxBlock> syntax(new SyntaxBlock);
    match(Token_Indent);
    while (!opt(Token_Dedent)) {
        syntax->append(parseStatement());
        match(Token_Newline);
    }
    return syntax.release();
}

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
    expr.reset(sp.parseExpr());
    testTrue(expr.get()->is<SyntaxAssignName>());

    sp.start("1\n2");
    expr.reset(sp.parseTopLevel());
    testEqual(repr(expr.get()), "1\n2\n");
}
