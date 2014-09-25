#include "syntax.h"
#include "test.h"

#include <memory>

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
    spec.addUnaryOp(Token_Minus, [] (Token _, Syntax* r) {
            return new SyntaxNegate(r);
        });
    spec.addPrefixHandler(Token_Bra, [] (Derived& parser, Spec& spec, Token _2) {
            Syntax* content = parser.expression(spec);
            parser.match(Token_Ket);
            // todo: parse tuples
            return content;
        });
    spec.addBinaryOp(Token_Assign, 10, Assoc_Right, [] (Token _, Syntax* l, Syntax* r) {
            if (!l->is<SyntaxName>() && !l->is<SyntaxPropRef>())
                throw ParseError("Illegal LHS for assignment");
            return new SyntaxAssign(l, r);
        });
    spec.addBinaryOp(Token_Plus, 10, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
            return new SyntaxPlus(l, r);
        });
    spec.addBinaryOp(Token_Minus, 10, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
            return new SyntaxMinus(l, r);
        });
    spec.addBinaryOp(Token_Period, 90, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
            if (!r->is<SyntaxName>())
                throw ParseError("Bad property reference");
            return new SyntaxPropRef(l, r);
        });
    spec.addInfixHandler(Token_Bra, 80, [] (Derived& parser, Spec& spec, Token _, Syntax* l) {
            SyntaxCall* call = new SyntaxCall(l);
            bool first = true;
            while (!parser.opt(Token_Ket)) {
                if (!first)
                    parser.match(Token_Comma);
                else
                    first = false;
                call->addArg(parser.expression(spec));
            }
            return call;
        });
    spec.addPrefixHandler(Token_Return, [] (Derived& parser, Spec& spec, Token _2) {
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

testcase(syntax)
{
    SyntaxParser sp;
    sp.start("1+2-3");
    unique_ptr<Syntax> expr(sp.parse());
    testEqual(expr->repr(), "1 + 2 - 3");

    sp.start("1\n2");
    expr.reset(sp.parseBlock());
    testEqual(expr->repr(), "1\n2\n");
}
