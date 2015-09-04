#include "src/token.h"

#include "src/test.h"

static void tokenize(const char *source)
{
    Tokenizer tz;
    tz.start(source);
    while (tz.nextToken().type != Token_EOF)
        ;
}

testcase(tokenizer)
{
    Tokenizer tz;
    tz.start(Input("", "empty.txt"));
    Token t = tz.nextToken();
    testEqual(t.type, Token_EOF);
    testEqual(t.text, "");
    testEqual(t.pos.file, "empty.txt");
    testEqual(t.pos.line, 1u);
    testEqual(t.pos.column, 0u);

    tz.start("   ");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("+");
    t = tz.nextToken();
    testEqual(t.type, Token_Add);
    testEqual(t.text, "+");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("!=");
    t = tz.nextToken();
    testEqual(t.type, Token_NE);
    testEqual(t.text, "!=");

    tz.start("123");
    t = tz.nextToken();
    testEqual(t.type, Token_Integer);
    testEqual(t.text, "123");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start(".5"); // fraction
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, ".5");

    tz.start("1.5"); // intpart fraction
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, "1.5");

    tz.start("1."); // intpart .
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, "1.");

    tz.start("1e2"); // intpart exponent
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, "1e2");

    tz.start(".5E2"); // fraction exponent
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, ".5E2");

    tz.start("1.5e-1"); // intpart fraction exponent
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, "1.5e-1");

    tz.start("1.e+3"); // intpart . exponent
    t = tz.nextToken();
    testEqual(t.type, Token_Float);
    testEqual(t.text, "1.e+3");

    tz.start("name_123");
    t = tz.nextToken();
    testEqual(t.type, Token_Identifier);
    testEqual(t.text, "name_123");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("1+2 - 3");
    t = tz.nextToken();
    testEqual(t.type, Token_Integer);
    testEqual(t.text, "1");
    testEqual(t.pos.column, 0u);
    t = tz.nextToken();
    testEqual(t.type, Token_Add);
    testEqual(t.pos.column, 1u);
    t = tz.nextToken();
    testEqual(t.type, Token_Integer);
    testEqual(t.text, "2");
    testEqual(t.pos.column, 2u);
    t = tz.nextToken();
    testEqual(t.type, Token_Sub);
    testEqual(t.pos.column, 4u);
    t = tz.nextToken();
    testEqual(t.type, Token_Integer);
    testEqual(t.text, "3");
    testEqual(t.pos.column, 6u);
    t = tz.nextToken();
    testEqual(t.type, Token_EOF);
    testEqual(t.pos.column, 7u);
    testEqual(t.pos.line, 1u);

    tz.start("foo()");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Bra);
    testEqual(tz.nextToken().type, Token_Ket);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("foo.bar >>= (2 ** baz)");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Period);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_AssignBitRightShift);
    testEqual(tz.nextToken().type, Token_Bra);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Power);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Ket);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("foo\n  bar\nbaz");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Dedent);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("foo\n  bar\n baz");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testThrows(tz.nextToken(), TokenError);

    tz.start("foo\n\n    \nbaz");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("nand and anda");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_And);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("a<<~3");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_BitLeftShift);
    testEqual(tz.nextToken().type, Token_BitNot);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_EOF);

    testThrows(tokenize("$"), TokenError);

    testThrows(tokenize("'string\""), TokenError);
    testThrows(tokenize("\"string'"), TokenError);

    tz.start("'$tring'");
    t = tz.nextToken();
    testEqual(t.type, Token_String);
    testEqual(t.text, "$tring");

    tz.start("\"gnirts\"");
    t = tz.nextToken();
    testEqual(t.type, Token_String);
    testEqual(t.text, "gnirts");

    tz.start("\"\\n\\t\"");
    t = tz.nextToken();
    testEqual(t.type, Token_String);
    testEqual(t.text, "\n\t");

    tz.start("\"\\:\"");
    t = tz.nextToken();
    testEqual(t.type, Token_String);
    testEqual(t.text, "\\:");

    tz.start("is is not not not in in");
    testEqual(tz.nextToken().type, Token_Is);
    testEqual(tz.nextToken().type, Token_IsNot);
    testEqual(tz.nextToken().type, Token_Not);
    testEqual(tz.nextToken().type, Token_NotIn);
    testEqual(tz.nextToken().type, Token_In);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("if 1:\n"
             "  return 2\n");
    testEqual(tz.nextToken().type, Token_If);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Colon);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Return);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Dedent);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("if 0:\n"
             "  return 2\n"
             "else:\n"
             "  return 3\n");
    testEqual(tz.nextToken().type, Token_If);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Colon);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Return);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Dedent);
    testEqual(tz.nextToken().type, Token_Else);
    testEqual(tz.nextToken().type, Token_Colon);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Return);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Dedent);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("white   \nspace");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("white  \n"
             "  space");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Dedent);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("(   \n    )");
    testEqual(tz.nextToken().type, Token_Bra);
    testEqual(tz.nextToken().type, Token_Ket);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("[   # comment\n    ]");
    testEqual(tz.nextToken().type, Token_SBra);
    testEqual(tz.nextToken().type, Token_SKet);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("{\n\n\n}");
    testEqual(tz.nextToken().type, Token_CBra);
    testEqual(tz.nextToken().type, Token_CKet);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("# comment");
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("foo # comment");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("foo # comment\nbar");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("foo # comment\n"
             "    # comment\n"
             "  bar");
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Indent);
    testEqual(tz.nextToken().type, Token_Identifier);
    testEqual(tz.nextToken().type, Token_Newline);
    testEqual(tz.nextToken().type, Token_Dedent);
    testEqual(tz.nextToken().type, Token_EOF);

    tz.start("(1, .3 + 1)");
    testEqual(tz.nextToken().type, Token_Bra);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Comma);
    testEqual(tz.nextToken().type, Token_Float);
    testEqual(tz.nextToken().type, Token_Add);
    testEqual(tz.nextToken().type, Token_Integer);
    testEqual(tz.nextToken().type, Token_Ket);
    testEqual(tz.nextToken().type, Token_EOF);
}
