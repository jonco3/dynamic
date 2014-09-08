#include "parser.h"
#include "test.h"

#include <cstdlib>
#include <stdexcept>
#include <ostream>

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

    Tokenizer tokenizer("1 + 2 - 3", "");

    Parser<int> parser(spec, tokenizer);

    try {
        testEqual(parser.parse(), -1);
    } catch (const std::runtime_error& err) {
        std::cerr << "Runtime error" << err.what() << std::endl;
    }
});
