#ifndef __SYNTAX_H__
#define __SYNTAX_H__

#include "parser.h"
#include "name.h"

#include <string>
#include <sstream>

#define for_each_syntax(syntax)                                               \
    syntax(Number)                                                            \
    syntax(Name)                                                              \
    syntax(Negate)                                                            \
    syntax(Plus)                                                              \
    syntax(Minus)                                                             \
    syntax(PropRef)                                                           \
    syntax(Assign)                                                            \
    syntax(Call)

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
    template <typename T> bool is() const { return type() == T::Type; }
    template <typename T> T* as() { assert(is<T>()); return static_cast<T*>(this); }

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

struct SyntaxName : public Syntax
{
    Name id;

    SyntaxName(std::string id) : id(id) {}

    syntax_type(Syntax_Name)
    syntax_name("name")

    virtual std::string repr() const {
        return id;
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

struct SyntaxPropRef : public BinarySyntax
{
    SyntaxPropRef(Syntax* l, Syntax* r) : BinarySyntax(l, r) {
        assert(r->is<SyntaxName>());
    }

    Name name() { return right->as<SyntaxName>()->id; }

    syntax_type(Syntax_PropRef)
    syntax_name(".")
    syntax_accept()
};

struct SyntaxAssign : public BinarySyntax
{
    SyntaxAssign(Syntax* l, Syntax* r) : BinarySyntax(l, r) {}
    syntax_type(Syntax_Assign)
    syntax_name("=")
    syntax_accept()
};

struct SyntaxCall : public Syntax
{
    std::auto_ptr<Syntax> left;
    std::vector<Syntax*> right;

    SyntaxCall(Syntax* l) : left(l) {}
    void addArg(Syntax* arg) { right.push_back(arg); }
    ~SyntaxCall() {
        // todo: use auto_ptr
        for (auto i = right.begin(); i != right.end(); ++i)
            delete *i;
    }

    syntax_type(Syntax_Call)
    syntax_name("call")
    syntax_accept()

    virtual std::string repr() const {
        std::ostringstream s;
        s << left->repr() << "(";
        for (auto i = right.begin(); i != right.end(); ++i) {
            if (i != right.begin())
                s << ", ";
            s << *i;
        }
        s << "(";
        return s.str();
    }
};

struct SyntaxParser : public Parser<Syntax *>
{
    SyntaxParser();
  private:
    Tokenizer tokenizer;
    ExprSpec<Syntax *> spec;
};

#endif
