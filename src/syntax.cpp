#include "syntax.h"

#include "repr.h"
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
    spec.addPrefixHandler(Token_Bra, [] (ParserT& parser, const ExprSpec& spec,
                                         Token _) {
        Syntax* content = parser.expression(spec);
        parser.match(Token_Ket);
        // todo: parse tuples
        return content;
    });
    spec.addBinaryOp(Token_Assign, 10, Assoc_Right,
                     [] (Token _, Syntax* l, Syntax* r) -> Syntax* {
        if (l->is<SyntaxName>())
            return new SyntaxAssignName(l->as<SyntaxName>(), r);
        else if (l->is<SyntaxPropRef>())
            return new SyntaxAssignProp(l->as<SyntaxPropRef>(), r);
        else
            throw ParseError("Illegal LHS for assignment");
    });
    spec.addBinaryOp(Token_Plus, 20, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxPlus(l, r);
    });
    spec.addBinaryOp(Token_Minus, 20, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        return new SyntaxMinus(l, r);
    });
    spec.addBinaryOp(Token_Period, 90, Assoc_Left, [] (Token _, Syntax* l, Syntax* r) {
        if (!r->is<SyntaxName>())
            throw ParseError("Bad property reference");
        return new SyntaxPropRef(l, r->as<SyntaxName>());
    });
    spec.addInfixHandler(Token_Bra, 80, [] (ParserT& parser, const ExprSpec& spec,
                                            Token _, Syntax* l) {
        vector<Syntax*> args;
        while (!parser.opt(Token_Ket)) {
            if (!args.empty())
                parser.match(Token_Comma);
            args.push_back(parser.expression(spec));
        }
        return new SyntaxCall(l, args);
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

testcase(syntax)
{
    SyntaxParser sp;
    sp.start("1+2-3");
    unique_ptr<Syntax> expr(sp.parse());
    testEqual(repr(expr.get()), "1 + 2 - 3");

    sp.start("f = 1 + 2");
    expr.reset(sp.parse());
    testTrue(expr.get()->is<SyntaxAssignName>());

    sp.start("1\n2");
    expr.reset(sp.parseBlock());
    testEqual(repr(expr.get()), "1\n2\n");
}
