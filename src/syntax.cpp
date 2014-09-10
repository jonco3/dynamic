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

struct SyntaxReprVisitor : public SyntaxVisitor
{
    std::string repr(const Syntax& s) {
        s.accept(*this);
        return result;
    }

  private:
    std::string result;

    void visitUnary(const UnarySyntax& s, std::string op) {
        s.right->accept(*this);
        std::string right = result;
        result = op + right;
    }

    void visitBinary(const BinarySyntax& s, std::string op) {
        s.left->accept(*this);
        std::string left = result;
        s.right->accept(*this);
        std::string right = result;
        result = left + op + right;
    }

    virtual void visit(const SyntaxNumber& s) { result = s.repr(); }
    virtual void visit(const SyntaxNegate& s) { visitUnary(s, "-"); }
    virtual void visit(const SyntaxPlus& s) { visitBinary(s, " + "); }
    virtual void visit(const SyntaxMinus& s) { visitBinary(s, " - "); }
};

struct SyntaxEvalVisitor : public SyntaxVisitor
{
    int eval(const Syntax& s) {
        s.accept(*this);
        return result;
    }

  private:
    int result;

    virtual void visit(const SyntaxNumber& s) { result = s.value; }
    virtual void visit(const SyntaxNegate& s) {
        s.right->accept(*this);
        result = -result;
    }
    virtual void visit(const SyntaxPlus& s) {
        s.left->accept(*this);
        int left = result;
        s.right->accept(*this);
        int right = result;
        result = left + right;
    }
    virtual void visit(const SyntaxMinus& s) {
        s.left->accept(*this);
        int left = result;
        s.right->accept(*this);
        int right = result;
        result = left - right;
    }
};

template <typename T>
struct GenericSyntaxVisitor : public SyntaxVisitor
{
    T visit(const Syntax& s) {
        s.accept(*this);
        return result;
    }

  protected:
    T result;

    typedef std::function<T(const T&)> UnaryOp;
    typedef std::function<T(const T&, const T&)> BinaryOp;

    void visitUnary(const UnarySyntax& s, UnaryOp op) {
        s.right->accept(*this);
        T right = result;
        result = op(right);
    }

    void visitBinary(const BinarySyntax& s, BinaryOp op) {
        s.left->accept(*this);
        T left = result;
        s.right->accept(*this);
        T right = result;
        result = op(left, right);
    }
};

testcase("syntax", {
    SyntaxParser sp;
    sp.start("1+2-3");
    Syntax *expr = sp.parse();
    testEqual(expr->repr(), "1 + 2 - 3");

    SyntaxReprVisitor rv;
    testEqual(rv.repr(*expr), "1 + 2 - 3");

    SyntaxEvalVisitor ev;
    testEqual(ev.eval(*expr), 0);

    delete expr;
});
