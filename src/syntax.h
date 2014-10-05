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
    syntax(Pos)                                                               \
    syntax(Neg)                                                               \
    syntax(Invert)                                                            \
    syntax(Or)                                                                \
    syntax(And)                                                               \
    syntax(Not)                                                               \
    syntax(In)                                                                \
    syntax(Is)                                                                \
    syntax(LT)                                                                \
    syntax(LE)                                                                \
    syntax(GT)                                                                \
    syntax(GE)                                                                \
    syntax(EQ)                                                                \
    syntax(NE)                                                                \
    syntax(BitOr)                                                             \
    syntax(BitXor)                                                            \
    syntax(BitAnd)                                                            \
    syntax(BitLeftShift)                                                      \
    syntax(BitRightShift)                                                     \
    syntax(Plus)                                                              \
    syntax(Minus)                                                             \
    syntax(Multiply)                                                          \
    syntax(Divide)                                                            \
    syntax(IntDivide)                                                         \
    syntax(Modulo)                                                            \
    syntax(Power)                                                             \
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

    template <typename T> T* as() {
        assert(is<T>());
        return static_cast<T*>(this);
    }

    template <typename T> const T* as() const {
        assert(is<T>());
        return static_cast<const T*>(this);
    }

    virtual ~Syntax() {}
    virtual SyntaxType type() const = 0;
    virtual string name() const = 0;
    virtual void print(ostream& s) const = 0;
    virtual void accept(SyntaxVisitor& v) const = 0;
};

inline ostream& operator<<(ostream& s, const Syntax* syntax) {
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
    UnarySyntax(Syntax* r) : right_(r) {}
    const Syntax* right() const { return right_.get(); }

    virtual void print(ostream& s) const override {
        s << name() << " " << right();
    }

  private:
    unique_ptr<Syntax> right_;
};

template <typename L, typename R>
struct BinarySyntaxBase : public Syntax
{
    BinarySyntaxBase(L* l, R* r) : left_(l), right_(r) {}
    const L* left() const { return left_.get(); }
    const R* right() const { return right_.get(); }

    virtual void print(ostream& s) const override {
        s << left() << " " << name() << " " << right();
    }

  private:
    unique_ptr<L> left_;
    unique_ptr<R> right_;
};

typedef BinarySyntaxBase<Syntax, Syntax> BinarySyntax;

#define define_simple_binary_syntax(name, nameStr)                            \
    struct Syntax##name : public BinarySyntax                                 \
    {                                                                         \
        Syntax##name(Syntax* l, Syntax* r) : BinarySyntax(l, r) {}            \
        syntax_type(Syntax_##name)                                            \
        syntax_name(nameStr)                                                  \
        syntax_accept()                                                       \
    }

#define define_simple_unary_syntax(name, nameStr)                             \
    struct Syntax##name : public UnarySyntax                                  \
    {                                                                         \
        Syntax##name(Syntax* r) : UnarySyntax(r) {}                           \
        syntax_type(Syntax_##name)                                            \
        syntax_name(nameStr)                                                  \
        syntax_accept()                                                       \
    }

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
    SyntaxInteger(int value) : value_(value) {}

    syntax_type(Syntax_Integer)
    syntax_name("number")
    int value() const { return value_; }

    virtual void print(ostream& s) const override {
        s << value_;
    }

    syntax_accept()

  private:
    int value_;
};

struct SyntaxName : public Syntax
{

    SyntaxName(string id) : id_(id) {}

    syntax_type(Syntax_Name)
    syntax_name("name")
    Name id() const { return id_; }

    virtual void print(ostream& s) const override {
        s << id_;
    }

    syntax_accept()

  private:
    Name id_;
};

define_simple_unary_syntax(Neg, "-");
define_simple_unary_syntax(Pos, "+");
define_simple_unary_syntax(Invert, "~");
define_simple_unary_syntax(Not, "not");

define_simple_binary_syntax(Or, "or");
define_simple_binary_syntax(And, "and");
define_simple_binary_syntax(In, "in");
define_simple_binary_syntax(Is, "is");
define_simple_binary_syntax(LT, "<");
define_simple_binary_syntax(LE, "<=");
define_simple_binary_syntax(GT, ">");
define_simple_binary_syntax(GE, ">=");
define_simple_binary_syntax(EQ, "!=");
define_simple_binary_syntax(NE, "==");
define_simple_binary_syntax(BitOr, "|");
define_simple_binary_syntax(BitXor, "^");
define_simple_binary_syntax(BitAnd, "&");
define_simple_binary_syntax(BitLeftShift, "<<");
define_simple_binary_syntax(BitRightShift, ">>");
define_simple_binary_syntax(Plus, "+");
define_simple_binary_syntax(Minus, "-");
define_simple_binary_syntax(Multiply, "*");
define_simple_binary_syntax(Divide, "/");
define_simple_binary_syntax(IntDivide, "//");
define_simple_binary_syntax(Modulo, "%");
define_simple_binary_syntax(Power, "**");

struct SyntaxPropRef : public BinarySyntaxBase<Syntax, SyntaxName>
{
    SyntaxPropRef(Syntax* l, SyntaxName* r) : BinarySyntaxBase(l, r) {}
    syntax_type(Syntax_PropRef)
    syntax_name(".")
    syntax_accept()
};

struct SyntaxAssignName : public BinarySyntaxBase<SyntaxName, Syntax>
{
    SyntaxAssignName(SyntaxName* l, Syntax* r) : BinarySyntaxBase(l, r) {}
    syntax_type(Syntax_AssignName)
    syntax_name("=")
    syntax_accept()
};

struct SyntaxAssignProp : public BinarySyntaxBase<SyntaxPropRef, Syntax>
{
    SyntaxAssignProp(SyntaxPropRef* l, Syntax* r) : BinarySyntaxBase(l, r) {}
    syntax_type(Syntax_AssignProp)
    syntax_name("=")
    syntax_accept()
};

struct SyntaxCall : public Syntax
{
    SyntaxCall(Syntax* l, vector<Syntax *> r) : left_(l), right_(r) {}
    ~SyntaxCall() {
        for (auto i = right_.begin(); i != right_.end(); ++i)
            delete *i;
    }

    const Syntax* left() const { return left_.get(); }
    const vector<Syntax*>& right() const { return right_; }

    syntax_type(Syntax_Call)
    syntax_name("call")
    syntax_accept()

    virtual void print(ostream& s) const override {
        s << left() << "(";
        for (auto i = right_.begin(); i != right_.end(); ++i) {
            if (i != right_.begin())
                s << ", ";
            s << *i;
        }
        s << ")";
    }

  private:
    unique_ptr<Syntax> left_;
    vector<Syntax*> right_;
};

struct SyntaxReturn : public UnarySyntax
{
    SyntaxReturn(Syntax* right) : UnarySyntax(right) {}
    syntax_type(Syntax_Return)
    syntax_name("return")
    syntax_accept()
};

#endif
