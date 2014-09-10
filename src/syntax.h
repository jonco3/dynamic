#ifndef __SYNTAX_H__
#define __SYNTAX_H__

#include "parser.h"

#include <string>
#include <sstream>

enum SyntaxType
{
    Syntax_Number,
    Syntax_Negate,
    Syntax_Plus,
    Syntax_Minus
};

struct Syntax
{
    virtual ~Syntax() {}
    virtual SyntaxType type() = 0;
    virtual std::string name() = 0;
    virtual std::string repr() = 0;
};

#define syntax_type(st) virtual SyntaxType type() { return st; }
#define syntax_name(nm) virtual std::string name() { return std::string(nm); }

struct UnarySyntax : public Syntax
{
    std::auto_ptr<Syntax> right;

    UnarySyntax(Syntax* r) : right(r) {}

    virtual std::string repr() {
        return name() + " " + right->repr();
    }
};

struct BinarySyntax : public Syntax
{
    std::auto_ptr<Syntax> left;
    std::auto_ptr<Syntax> right;

    BinarySyntax(Syntax* l, Syntax* r) :
      left(l), right(r) {}

    virtual std::string repr() {
        return left->repr() + " " + name() + " " + right->repr();
    }
};

struct SyntaxNumber : public Syntax
{
    int value;

    SyntaxNumber(int value) : value(value) {}

    syntax_type(Syntax_Number);
    syntax_name("number");

    virtual std::string repr() {
        std::ostringstream s;
        s << value;
        return s.str();
    }
};

struct SyntaxNegate : public UnarySyntax
{
    SyntaxNegate(Syntax* right) : UnarySyntax(right) {}
    syntax_type(Syntax_Negate);
    syntax_name("-");
};

struct SyntaxPlus : public BinarySyntax
{
    SyntaxPlus(Syntax* l, Syntax* r) : BinarySyntax(l, r) {}
    syntax_type(Syntax_Plus);
    syntax_name("+");
};

struct SyntaxMinus : public BinarySyntax
{
    SyntaxMinus(Syntax* l, Syntax* r) : BinarySyntax(l, r) {}
    syntax_type(Syntax_Minus);
    syntax_name("-");
};

struct SyntaxParser : public Parser<Syntax *>
{
    SyntaxParser();
  private:
    Tokenizer tokenizer;
    ExprSpec<Syntax *> spec;
};

#endif
