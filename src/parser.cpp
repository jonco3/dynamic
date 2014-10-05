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
  Parser(spec, tokenizer),
  spec(TokenCount)
{
    spec.addWord(Token_Integer, [] (Token token) {
        return new SyntaxInteger(atoi(token.text.c_str()));
    });

    spec.addWord(Token_Identifier, [] (Token token) {
        return new SyntaxName(token.text.c_str());
    });

    // Unary operations

    // todo: check the precedence of these works as expected

    spec.addUnaryOp(Token_Not, [] (Token _, Syntax* r) {
        return new SyntaxNot(r);
    });
    spec.addUnaryOp(Token_Minus, [] (Token _, Syntax* r) {
        return new SyntaxNegate(r);
    });

    // Displays

    spec.addPrefixHandler(Token_Bra, [] (ParserT& parser, const ExprSpec& spec,
                                         Token _) {
        Syntax* content = parser.expression(spec);
        parser.match(Token_Ket);
        // todo: parse tuples
        return content;
    });

    // Assignments

    spec.addBinaryOp(Token_Assign, 10, Assoc_Right,
                     [] (Token _, Syntax* l, Syntax* r) -> Syntax* {
        if (l->is<SyntaxName>())
            return new SyntaxAssignName(l->as<SyntaxName>(), r);
        else if (l->is<SyntaxPropRef>())
            return new SyntaxAssignProp(l->as<SyntaxPropRef>(), r);
        else
            throw ParseError("Illegal LHS for assignment");
    });

    // Boolean operators

    // Comparison operators

    spec.addBinaryOp(Token_In, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxIn(l, r);
    });
    spec.addBinaryOp(Token_NotIn, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxNot(new SyntaxIn(l, r));
    });
    spec.addBinaryOp(Token_Is, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxIs(l, r);
    });
    spec.addBinaryOp(Token_IsNot, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxNot(new SyntaxIs(l, r));
    });
    spec.addBinaryOp(Token_LT, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxLT(l, r);
    });
    spec.addBinaryOp(Token_LE, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxLE(l, r);
    });
    spec.addBinaryOp(Token_GT, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxGT(l, r);
    });
    spec.addBinaryOp(Token_GE, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxGE(l, r);
    });
    spec.addBinaryOp(Token_EQ, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxEQ(l, r);
    });
    spec.addBinaryOp(Token_NE, 120, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxNE(l, r);
    });

    // Bitwise binary operators

    spec.addBinaryOp(Token_BitOr, 130, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxBitOr(l, r);
    });
    spec.addBinaryOp(Token_BitXor, 140, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxBitXor(l, r);
    });
    spec.addBinaryOp(Token_BitAnd, 150, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxBitAnd(l, r);
    });
    spec.addBinaryOp(Token_BitLeftShift, 160, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxBitLeftShift(l, r);
    });
    spec.addBinaryOp(Token_BitRightShift, 160, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxBitRightShift(l, r);
    });

    // Arithermetic binary operators

    spec.addBinaryOp(Token_Plus, 170, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxPlus(l, r);
    });
    spec.addBinaryOp(Token_Minus, 170, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxMinus(l, r);
    });

    spec.addBinaryOp(Token_Times, 180, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxMultiply(l, r);
    });
    spec.addBinaryOp(Token_Divide, 180, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxDivide(l, r);
    });
    spec.addBinaryOp(Token_IntDivide, 180, Assoc_Left, [] (Token _, Syntax* l,
                                                           Syntax* r) {
        return new SyntaxIntDivide(l, r);
    });
    spec.addBinaryOp(Token_Modulo, 180, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxModulo(l, r);
    });

    spec.addBinaryOp(Token_Power, 190, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxPower(l, r);
    });

    spec.addInfixHandler(Token_Bra, 200, [] (ParserT& parser, const ExprSpec& spec,
                                            Token _, Syntax* l) {
        vector<Syntax*> args;
        while (!parser.opt(Token_Ket)) {
            if (!args.empty())
                parser.match(Token_Comma);
            args.push_back(parser.expression(spec));
        }
        return new SyntaxCall(l, args);
    });

    spec.addBinaryOp(Token_Period, 200, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        if (!r->is<SyntaxName>())
            throw ParseError("Bad property reference");
        return new SyntaxPropRef(l, r->as<SyntaxName>());
    });

    spec.addPrefixHandler(Token_Return, [] (ParserT& parser, const ExprSpec& spec,
                                            Token _) {
        Syntax *expr;
        if (parser.notFollowedBy(Token_EOF) &&
            parser.notFollowedBy(Token_Newline) &&
            parser.notFollowedBy(Token_Dedent))
        {
            expr = parser.expression(spec);
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
    Parser<int>::ExprSpec spec(TokenCount);
    spec.addWord(Token_Integer, [] (Token token) {
            return atoi(token.text.c_str());  // todo: what's the c++ way to do this?
        });
    spec.addBinaryOp(Token_Plus, 10, Assoc_Left, [] (Token _, int l, int r) {
            return l + r;
        });
    spec.addBinaryOp(Token_Minus, 10, Assoc_Left, [] (Token _, int l, int r) {
            return l - r;
        });
    spec.addUnaryOp(Token_Minus, [] (Token _, int r) {
            return -r;
        });

    Tokenizer tokenizer;
    Parser<int> parser(spec, tokenizer);

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
