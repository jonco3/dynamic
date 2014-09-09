#include "parser.h"
#include "test.h"

#include <cstdlib>
#include <stdexcept>
#include <ostream>

using namespace std;

testcase("parser", {
    ExprSpec<int> spec(TokenCount);
    spec.addWord(Token_Number, [] (Token token) {
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

    Tokenizer tokenizer("2 + 3 - 1", "");

    Parser<int> parser(spec, tokenizer);

    testEqual(parser.parse(), 4);
});
