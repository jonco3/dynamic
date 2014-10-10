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
  Parser(expr, tokenizer),
  expr(TokenCount)
{
    expr.addWord(Token_Integer, [] (Token token) {
        return new SyntaxInteger(atoi(token.text.c_str()));
    });

    expr.addWord(Token_Identifier, [] (Token token) {
        return new SyntaxName(token.text.c_str());
    });

    // Unary operations

    // todo: check the precedence of these works as expected

    expr.addUnaryOp(Token_Plus, [] (Token _, Syntax* r) {
        return new SyntaxPos(r);
    });
    expr.addUnaryOp(Token_Minus, [] (Token _, Syntax* r) {
        return new SyntaxNeg(r);
    });
    expr.addUnaryOp(Token_Not, [] (Token _, Syntax* r) {
        return new SyntaxInvert(r);
    });

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

    expr.addBinaryOp(Token_Or, 100, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxOr(l, r);
    });

    expr.addBinaryOp(Token_And, 110, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxAnd(l, r);
    });

    // Comparison operators

    expr.addBinaryOp(Token_In, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxIn(l, r);
    });
    expr.addBinaryOp(Token_NotIn, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxNot(new SyntaxIn(l, r));
    });
    expr.addBinaryOp(Token_Is, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxIs(l, r);
    });
    expr.addBinaryOp(Token_IsNot, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxNot(new SyntaxIs(l, r));
    });
    expr.addBinaryOp(Token_LT, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxLT(l, r);
    });
    expr.addBinaryOp(Token_LE, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxLE(l, r);
    });
    expr.addBinaryOp(Token_GT, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxGT(l, r);
    });
    expr.addBinaryOp(Token_GE, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxGE(l, r);
    });
    expr.addBinaryOp(Token_EQ, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxEQ(l, r);
    });
    expr.addBinaryOp(Token_NE, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxNE(l, r);
    });

    // Bitwise binary operators

    expr.addBinaryOp(Token_BitOr, 130, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxBitOr(l, r);
    });
    expr.addBinaryOp(Token_BitXor, 140, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxBitXor(l, r);
    });
    expr.addBinaryOp(Token_BitAnd, 150, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxBitAnd(l, r);
    });
    expr.addBinaryOp(Token_BitLeftShift, 160, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxBitLeftShift(l, r);
    });
    expr.addBinaryOp(Token_BitRightShift, 160, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxBitRightShift(l, r);
    });

    // Arithermetic binary operators

    expr.addBinaryOp(Token_Plus, 170, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxPlus(l, r);
    });
    expr.addBinaryOp(Token_Minus, 170, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxMinus(l, r);
    });

    expr.addBinaryOp(Token_Times, 180, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxMultiply(l, r);
    });
    expr.addBinaryOp(Token_Divide, 180, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxDivide(l, r);
    });
    expr.addBinaryOp(Token_IntDivide, 180, Assoc_Left, [] (Token _, Syntax* l,
                                                           Syntax* r) {
        return new SyntaxIntDivide(l, r);
    });
    expr.addBinaryOp(Token_Modulo, 180, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxModulo(l, r);
    });

    expr.addBinaryOp(Token_Power, 190, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxPower(l, r);
    });

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
}

SyntaxBlock* SyntaxParser::parseBlock()
{
    unique_ptr<SyntaxBlock> syntax(new SyntaxBlock);
    while (!atEnd()) {
        syntax->append(parse());
        if (atEnd())
            break;
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
    Parser<int> parser(acts, tokenizer);

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
    unique_ptr<Syntax> expr(sp.parse());
    testEqual(repr(expr.get()), "1 + 2 - 3");

    sp.start("4 ** 5");
    expr.reset(sp.parse());
    testEqual(repr(expr.get()), "4 ** 5");

    sp.start("f = 1 + 2");
    expr.reset(sp.parse());
    testTrue(expr.get()->is<SyntaxAssignName>());

    sp.start("1\n2");
    expr.reset(sp.parseBlock());
    testEqual(repr(expr.get()), "1\n2\n");
}
