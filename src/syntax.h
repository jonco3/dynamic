#ifndef __SYNTAX_H__
#define __SYNTAX_H__

#include "parser.h"
#include "name.h"

#include <memory>
#include <ostream>
#include <string>

using namespace std;

#define for_each_syntax(syntax)                                               \
    syntax(Block)                                                             \
    syntax(Integer)                                                           \
    syntax(Name)                                                              \
    syntax(Negate)                                                            \
    syntax(Plus)                                                              \
    syntax(Minus)                                                             \
    syntax(PropRef)                                                           \
    syntax(AssignName)                                                        \
    syntax(AssignProp)                                                        \
    syntax(Call)                                                              \
    syntax(Return)

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

struct DefaultSyntaxVisitor : public SyntaxVisitor
{
#define syntax_visitor(name) virtual void visit(const Syntax##name&) override {}
    for_each_syntax(syntax_visitor)
#undef syntax_visitor
};

struct Syntax
{
    template <typename T> bool is() const { return type() == T::Type; }
    template <typename T> T* as() { assert(is<T>()); return static_cast<T*>(this); }

    virtual ~Syntax() {}
    virtual SyntaxType type() const = 0;
    virtual string name() const = 0;
    virtual void print(ostream& s) const = 0;
    virtual void accept(SyntaxVisitor& v) const = 0;
};

inline ostream& operator<<(ostream& s, Syntax* syntax) {
    syntax->print(s);
    return s;
}

#define syntax_type(st)                                                       \
    static const SyntaxType Type = st;                                        \
    virtual SyntaxType type() const override { return Type; }

#define syntax_name(nm)                                                       \
    virtual string name() const override { return string(nm); }

#define syntax_accept()                                                      \
    virtual void accept(SyntaxVisitor& v) const override { v.visit(*this); }

struct UnarySyntax : public Syntax
{
    unique_ptr<Syntax> right;

    UnarySyntax(Syntax* r) : right(r) {}

    virtual void print(ostream& s) const override {
        s << name() << " " << right.get();
    }
};

struct BinarySyntax : public Syntax
{
    unique_ptr<Syntax> left;
    unique_ptr<Syntax> right;

    BinarySyntax(Syntax* l, Syntax* r) :
      left(l), right(r) {}

    virtual void print(ostream& s) const override {
        s << left.get() << " " << name() << " " << right.get();
    }
};

struct SyntaxBlock : public Syntax
{
    syntax_type(Syntax_Block)
    syntax_name("block")

    void append(Syntax* s) { statements.push_back(s); }
    const vector<Syntax *>& stmts() const { return statements; }

    virtual void print(ostream& s) const override {
        for (auto i = statements.begin(); i != statements.end(); ++i)
            s << (*i) << endl;
    }

    syntax_accept();

  private:
    vector<Syntax *> statements;
};

struct SyntaxInteger : public Syntax
{
    int value;

    SyntaxInteger(int value) : value(value) {}

    syntax_type(Syntax_Integer)
    syntax_name("number")

    virtual void print(ostream& s) const override {
        s << value;
    }

    syntax_accept()
};

struct SyntaxName : public Syntax
{
    Name id;

    SyntaxName(string id) : id(id) {}

    syntax_type(Syntax_Name)
    syntax_name("name")

    virtual void print(ostream& s) const override {
        s << id;
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

struct SyntaxAssignName : public Syntax
{
    unique_ptr<SyntaxName> left;
    unique_ptr<Syntax> right;

    SyntaxAssignName(SyntaxName* l, Syntax* r) : left(l), right(r) {}
    syntax_type(Syntax_AssignName)
    syntax_name("=")
    syntax_accept()

    virtual void print(ostream& s) const override {
        s << left.get() << " = " << right.get();
    }
};

struct SyntaxAssignProp : public Syntax
{
    unique_ptr<SyntaxPropRef> left;
    unique_ptr<Syntax> right;

    SyntaxAssignProp(SyntaxPropRef* l, Syntax* r) : left(l), right(r) {}
    syntax_type(Syntax_AssignProp)
    syntax_name("=")
    syntax_accept()

    virtual void print(ostream& s) const override {
        s << left.get() << " = " << right.get();
    }
};

struct SyntaxCall : public Syntax
{
    unique_ptr<Syntax> left;
    vector<Syntax*> right;

    SyntaxCall(Syntax* l) : left(l) {}
    void addArg(Syntax* arg) { right.push_back(arg); }
    ~SyntaxCall() {
        // todo: use unique_ptr
        for (auto i = right.begin(); i != right.end(); ++i)
            delete *i;
    }

    syntax_type(Syntax_Call)
    syntax_name("call")
    syntax_accept()

    virtual void print(ostream& s) const override {
        s << left.get() << "(";
        for (auto i = right.begin(); i != right.end(); ++i) {
            if (i != right.begin())
                s << ", ";
            s << *i;
        }
        s << ")";
    }
};

struct SyntaxReturn : public UnarySyntax
{
    SyntaxReturn(Syntax* right) : UnarySyntax(right) {}
    syntax_type(Syntax_Return)
    syntax_name("return")
    syntax_accept()
};

struct SyntaxParser : public Parser<Syntax *>
{
    SyntaxParser();
    SyntaxBlock *parseBlock();
  private:
    Tokenizer tokenizer;
    Parser<Syntax*>::ExprSpec spec;
};

#endif
