#include "syntax.h"
#include "test.h"

SyntaxParser::SyntaxParser() :
  Parser(spec, tokenizer),
  spec(TokenCount)
{
    spec.addWord(Token_Number, [] (Token token) {
            return new SyntaxNumber(atoi(token.text.c_str()));
        });
    spec.addBinaryOp(Token_Plus, 10, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
            return new SyntaxPlus(l, r);
        });
    spec.addBinaryOp(Token_Minus, 10, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
            return new SyntaxMinus(l, r);
        });
    spec.addUnaryOp(Token_Minus, [] (Token _, Syntax* r) {
            return new SyntaxNegate(r);
        });
}

testcase("syntax", {
    SyntaxParser sp;
    sp.start("1+2-3");
    Syntax *expr = sp.parse();
    testEqual(expr->repr(), "1 + 2 - 3");
    delete expr;
});
