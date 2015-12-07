#include "src/parser.h"

#include "src/repr.h"
#include "src/test.h"

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
    parser.addBinaryOp(Token_Add, 10, Assoc_Left, [] (Token _, int l, int r) {
        return l + r;
    });
    parser.addBinaryOp(Token_Sub, 10, Assoc_Left, [] (Token _, int l, int r) {
        return l - r;
    });
    parser.addUnaryOp(Token_Sub, [] (Token _, int r) {
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
    testTrue(expr.get()->as<SyntaxAssign>()->left->is<SyntaxName>());

    testParseModule("1", "1");
    testParseModule("0x10", "0x10");

    testParseModule("1\n2", "1\n2");
    testParseModule("1; 2; 3", "1\n2\n3");
    testParseModule("1; 2; 3;", "1\n2\n3");
    testParseModule("a[0] = 1", "a[0] = 1");

    // test this parses as "not (a in b)" not "(not a) in b"
    sp.start("not a in b");
    expr = sp.parseModule();
    testEqual(repr(*expr.get()), "not a in b");
    testTrue(expr->is<SyntaxBlock>());
    testTrue(expr->as<SyntaxBlock>()->statements[0]->is<SyntaxNot>());

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

    testParseModule("(1, .3 + 1)", "(1, 0.3 + 1)");

    testParseModule("[x+1 for x in a]", "[ x + 1 for x in a ]");
    testParseModule("[x+y for x in a for y in b]", "[ x + y for x in a for y in b ]");
    testParseModule("[x+1 for x in a if x < 4]", "[ x + 1 for x in a if x < 4 ]");
    testParseModule("[x**2 for x in range(10)]", "[ x ** 2 for x in range(10) ]");
}
