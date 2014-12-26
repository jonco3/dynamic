#ifndef __SYNTAX_H__
#define __SYNTAX_H__

#include "parser.h"
#include "name.h"

#include <memory>
#include <ostream>
#include <string>

using namespace std;

#define for_each_syntax(syntax)                                               \
    syntax(Pass)                                                              \
    syntax(Block)                                                             \
    syntax(Integer)                                                           \
    syntax(String)                                                            \
    syntax(Tuple)                                                             \
    syntax(List)                                                              \
    syntax(Dict)                                                              \
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
    syntax(AttrRef)                                                           \
    syntax(AssignName)                                                        \
    syntax(AssignAttr)                                                        \
    syntax(AssignSubscript)                                                   \
    syntax(Call)                                                              \
    syntax(Return)                                                            \
    syntax(Cond)                                                              \
    syntax(Lambda)                                                            \
    syntax(Def)                                                               \
    syntax(If)                                                                \
    syntax(While)                                                             \
    syntax(Subscript)                                                         \
    syntax(Assert)                                                            \
    syntax(Class)

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

struct SyntaxPass : public Syntax
{
    syntax_type(Syntax_Pass)
    syntax_name("pass")

    virtual void print(ostream& s) const override {
        s << "pass";
    }

    syntax_accept()
};

struct SyntaxBlock : public Syntax
{
    syntax_type(Syntax_Block);
    syntax_name("block");

    ~SyntaxBlock() {
        for (auto i = statements.begin(); i != statements.end(); ++i)
            delete *i;
    }

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
    // todo: this probably doesn't correctly parse all python integers
    SyntaxInteger(const Token& token) : value_(atoi(token.text.c_str())) {}

    syntax_type(Syntax_Integer)
    syntax_name("integer")
    int value() const { return value_; }

    virtual void print(ostream& s) const override {
        s << value_;
    }

    syntax_accept()

  private:
    int value_;
};

struct SyntaxString : public Syntax
{
    SyntaxString(const Token& token) : value_(token.text) {}

    syntax_type(Syntax_String)
    syntax_name("string")
    const string& value() const { return value_; }

    virtual void print(ostream& s) const override {
        s << "'" << value_ << "'";
    }

    syntax_accept()

  private:
    string value_;
};

struct SyntaxName : public Syntax
{

    SyntaxName(const Token& token) : id_(token.text) {}

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

struct SyntaxTuple : public Syntax
{
    syntax_type(Syntax_Tuple);
    syntax_name("tuple");

    SyntaxTuple() {}

    SyntaxTuple(const vector<Syntax*> elems)
        : elements(elems) {}

    ~SyntaxTuple() {
        for (auto i = elements.begin(); i != elements.end(); ++i)
            delete *i;
    }

    const vector<Syntax *>& elems() const { return elements; }

    virtual void print(ostream& s) const override {
        s << "(";
        for (auto i = elements.begin(); i != elements.end(); ++i) {
            if (i != elements.begin())
                s << ", ";
            s << (*i);
        }
        if (elements.size() == 1)
                s << ",";
        s << ")";
    }

    syntax_accept();

  private:
    vector<Syntax *> elements;
};

struct SyntaxList : public Syntax
{
    syntax_type(Syntax_List);
    syntax_name("List");

    SyntaxList(const vector<Syntax*> elems)
        : elements(elems) {}

    ~SyntaxList() {
        for (auto i = elements.begin(); i != elements.end(); ++i)
            delete *i;
    }

    const vector<Syntax *>& elems() const { return elements; }

    virtual void print(ostream& s) const override {
        s << "[";
        for (auto i = elements.begin(); i != elements.end(); ++i) {
            if (i != elements.begin())
                s << ", ";
            s << (*i);
        }
        s << "]";
    }

    syntax_accept();

  private:
    vector<Syntax *> elements;
};

struct SyntaxDict : public Syntax
{
    syntax_type(Syntax_Dict);
    syntax_name("Dict");

    typedef pair<Syntax*, Syntax*> Entry;

    SyntaxDict(const vector<Entry> entries)
        : entries_(entries) {}

    ~SyntaxDict() {
        for (auto i = entries_.begin(); i != entries_.end(); ++i) {
            delete i->first;
            delete i->second;
        }
    }

    const vector<Entry>& entries() const { return entries_; }

    virtual void print(ostream& s) const override {
        s << "{";
        for (auto i = entries_.begin(); i != entries_.end(); ++i) {
            if (i != entries_.begin())
                s << ", ";
            s << i->first << ": " << i->second;
        }
        s << "}";
    }

    syntax_accept();

  private:
    vector<Entry> entries_;
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

struct SyntaxAttrRef : public BinarySyntaxBase<Syntax, SyntaxName>
{
    SyntaxAttrRef(Syntax* l, SyntaxName* r) : BinarySyntaxBase(l, r) {}
    syntax_type(Syntax_AttrRef)
    syntax_name(".")
    syntax_accept()
};

struct SyntaxAssignName : public BinarySyntaxBase<SyntaxName, Syntax>
{
    SyntaxAssignName(SyntaxName* l, Syntax* r) : BinarySyntaxBase(l, r) {}
    syntax_type(Syntax_AssignName)
    syntax_name("n=")
    syntax_accept()
};

struct SyntaxAssignAttr : public BinarySyntaxBase<SyntaxAttrRef, Syntax>
{
    SyntaxAssignAttr(SyntaxAttrRef* l, Syntax* r) : BinarySyntaxBase(l, r) {}
    syntax_type(Syntax_AssignAttr)
    syntax_name("a=")
    syntax_accept()
};

struct SyntaxAssignSubscript : public BinarySyntaxBase<SyntaxSubscript, Syntax>
{
    SyntaxAssignSubscript(SyntaxSubscript* l, Syntax* r) : BinarySyntaxBase(l, r) {}
    syntax_type(Syntax_AssignSubscript)
    syntax_name("s=")
    syntax_accept()
};

struct SyntaxSubscript : public BinarySyntaxBase<Syntax, Syntax>
{
    SyntaxSubscript(Syntax* l, Syntax* r) : BinarySyntaxBase(l, r) {}
    syntax_type(Syntax_Subscript)
    syntax_name("subscript")
    syntax_accept()

    virtual void print(ostream& s) const override {
        s << left() << "[" << right() << "]";
    }
};

struct SyntaxCall : public Syntax
{
    SyntaxCall(Syntax* l, const vector<Syntax *>& r) : left_(l), right_(r) {}
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

struct SyntaxCond : public Syntax
{
    SyntaxCond(Syntax* cons, Syntax* cond, Syntax* alt)
      : cons_(cons), cond_(cond), alt_(alt) {}
    syntax_type(Syntax_Cond)
    syntax_name("cond")

    const Syntax* cons() const { return cons_.get(); }
    const Syntax* cond() const { return cond_.get(); }
    const Syntax* alt() const { return alt_.get(); }

    syntax_accept()

    virtual void print(ostream& s) const override {
        s << cons() << " if " << cond() << " else " << alt();
    }

  private:
    unique_ptr<Syntax> cons_;
    unique_ptr<Syntax> cond_;
    unique_ptr<Syntax> alt_;
};

struct SyntaxLambda : public Syntax
{
    SyntaxLambda(const vector<Name>& params, Syntax* expr)
      : params_(params), expr_(expr) {}

    syntax_type(Syntax_Lambda);
    syntax_name("lambda");
    syntax_accept();

    const vector<Name>& params() const { return params_; }
    Syntax* expr() const { return expr_; }

    virtual void print(ostream& s) const override {
        s << "lambda";
        for (auto i = params_.begin(); i != params_.end(); ++i)
            s << " " << *i;
        s << ": ";
        expr_->print(s);
    }

  private:
    vector<Name> params_;
    Syntax* expr_;
};

struct SyntaxDef : public Syntax
{
    SyntaxDef(Name id, const vector<Name>& params, Syntax* expr)
        : id_(id), params_(params), expr_(expr) {}

    syntax_type(Syntax_Def);
    syntax_name("def");
    syntax_accept();

    Name id() const { return id_; }
    const vector<Name>& params() const { return params_; }
    Syntax* expr() const { return expr_; }

    virtual void print(ostream& s) const override {
        s << "def " << id_ << "(";
        for (auto i = params_.begin(); i != params_.end(); ++i) {
            if (i != params_.begin())
                s << ", ";
            s << *i;
        }
        s << "):" << endl;
        expr_->print(s);
    }

  private:
    Name id_;
    vector<Name> params_;
    Syntax* expr_;
};

struct SyntaxIf : public Syntax
{
    struct Branch
    {
        Syntax* cond;
        SyntaxBlock* block;
    };

    ~SyntaxIf() {
        for (auto i = branches_.begin(); i != branches_.end(); ++i) {
            Branch& branch = *i;
            delete branch.cond;
            delete branch.block;
        }
    }

    syntax_type(Syntax_If);
    syntax_name("if");

    void addBranch(Syntax* cond, SyntaxBlock* block) {
        branches_.push_back({ cond, block });
    }

    void setElse(SyntaxBlock* block) {
        else_.reset(block);
    }

    const vector<Branch>& branches() const { return branches_; }
    const SyntaxBlock* elseBranch() const { return else_.get(); }

    syntax_accept();

    virtual void print(ostream& s) const override {
        // todo: indentation
        for (unsigned i = 0; i < branches_.size(); ++i) {
            s << (i == 0 ? "if" : "elif") << " ";
            s << branches_[i].cond << ":" << endl;
            s << branches_[i].block << endl;
        }
        if (else_) {
            s << "else:" << endl << else_.get() << endl;
        }
    }

  private:
    vector<Branch> branches_;
    unique_ptr<SyntaxBlock> else_;
};

struct SyntaxWhile : public Syntax
{
    SyntaxWhile(Syntax* cond, SyntaxBlock* suite, SyntaxBlock* elseSuite)
      : cond_(cond), suite_(suite), elseSuite_(elseSuite) {}

    syntax_type(Syntax_While);
    syntax_name("while");

    const Syntax* cond() const { return cond_.get(); }
    const Syntax* suite() const { return suite_.get(); }
    const Syntax* elseSuite() const { return elseSuite_.get(); }

    syntax_accept();

    virtual void print(ostream& s) const override {
        // todo: indentation
        s << "while " << cond() << ": " << endl;
        s << suite() << endl;
        if (elseSuite())
            s << elseSuite() << endl;
    }

  private:
    unique_ptr<Syntax> cond_;
    unique_ptr<SyntaxBlock> suite_;
    unique_ptr<SyntaxBlock> elseSuite_;
};

struct SyntaxAssert : public Syntax
{
    SyntaxAssert(Syntax* cond, Syntax* message)
      : cond_(cond), message_(message) {}
    syntax_type(Syntax_Assert)
    syntax_name("assert")

    const Syntax* cond() const { return cond_.get(); }
    const Syntax* message() const { return message_.get(); }

    syntax_accept();

    virtual void print(ostream& s) const override {
        s << "assert " << cond();
        if (message_)
            s << ", " << message();
    }

  private:
    unique_ptr<Syntax> cond_;
    unique_ptr<Syntax> message_;
};

struct SyntaxClass : public Syntax
{
    SyntaxClass(Name id, SyntaxTuple* bases, SyntaxBlock* suite)
      : id_(id), bases_(bases), suite_(suite) {}

    syntax_type(Syntax_Class);
    syntax_name("class");

    Name id() const { return id_; }
    const SyntaxTuple* bases() const { return bases_.get(); }
    const Syntax* suite() const { return suite_.get(); }

    syntax_accept();

    virtual void print(ostream& s) const override {
        // todo: indentation
        s << "class " << id();
        if (!bases_->elems().empty())
            s << bases() << " ";
        s << ":" << endl << suite() << endl;
    }

  private:
    Name id_;
    unique_ptr<SyntaxTuple> bases_;
    unique_ptr<SyntaxBlock> suite_;
};

#endif
