#ifndef __SYNTAX_H__
#define __SYNTAX_H__

#include "parser.h"

#include <string>
#include <sstream>

#define for_each_syntax(syntax)                                               \
    syntax(Number)                                                            \
    syntax(Negate)                                                            \
    syntax(Plus)                                                              \
    syntax(Minus)

enum SyntaxType
{
#define syntax_enum(name) Syntax_##name,
    for_each_syntax(syntax_enum)
#undef syntax_enum
    SyntaxTypeCount
};

#define forward_declare_syntax(name) struct Syntax##name;
for_each_syntax(forward_declare_syntax)
#undef forward_declare_syntax

struct SyntaxVisitor
{
    virtual ~SyntaxVisitor() {}
#define syntax_visitor(name) virtual void visit(const Syntax##name&) = 0;
    for_each_syntax(syntax_visitor)
#undef syntax_visitor
};

struct Syntax
{
    virtual ~Syntax() {}
    virtual SyntaxType type() const = 0;
    virtual std::string name() const = 0;
    virtual std::string repr() const = 0;
    virtual void accept(SyntaxVisitor& v) const = 0;
};

#define syntax_type(st)                                                       \
    static const SyntaxType Type = st;                                        \
    virtual SyntaxType type() const { return Type; }

#define syntax_name(nm)                                                       \
    virtual std::string name() const { return std::string(nm); }

#define syntax_accept()                                                      \
    virtual void accept(SyntaxVisitor& v) const { v.visit(*this); }

struct UnarySyntax : public Syntax
{
    std::auto_ptr<Syntax> right;

    UnarySyntax(Syntax* r) : right(r) {}

    virtual std::string repr() const {
        return name() + " " + right->repr();
    }
};

struct BinarySyntax : public Syntax
{
    std::auto_ptr<Syntax> left;
    std::auto_ptr<Syntax> right;

    BinarySyntax(Syntax* l, Syntax* r) :
      left(l), right(r) {}

    virtual std::string repr() const {
        return left->repr() + " " + name() + " " + right->repr();
    }
};

struct SyntaxNumber : public Syntax
{
    int value;

    SyntaxNumber(int value) : value(value) {}

    syntax_type(Syntax_Number)
    syntax_name("number")

    virtual std::string repr() const {
        std::ostringstream s;
        s << value;
        return s.str();
    }

    syntax_accept()
};

struct SyntaxNegate : public UnarySyntax
{
    SyntaxNegate(Syntax* right) : UnarySyntax(right) {}
    syntax_type(Syntax_Negate)
    syntax_name("-")
    syntax_accept()
};

struct SyntaxPlus : public BinarySyntax
{
    SyntaxPlus(Syntax* l, Syntax* r) : BinarySyntax(l, r) {}
    syntax_type(Syntax_Plus)
    syntax_name("+")
    syntax_accept()
};

struct SyntaxMinus : public BinarySyntax
{
    SyntaxMinus(Syntax* l, Syntax* r) : BinarySyntax(l, r) {}
    syntax_type(Syntax_Minus)
    syntax_name("-")
    syntax_accept()
};

struct SyntaxParser : public Parser<Syntax *>
{
    SyntaxParser();
  private:
    Tokenizer tokenizer;
    ExprSpec<Syntax *> spec;
};

#endif
