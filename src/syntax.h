#ifndef __SYNTAX_H__
#define __SYNTAX_H__

#include "name.h"
#include "specials.h"
#include "token.h"

#include <memory>
#include <ostream>
#include <string>

using namespace std;

#define for_each_syntax(syntax)                                               \
    syntax(Pass)                                                              \
    syntax(Block)                                                             \
    syntax(Integer)                                                           \
    syntax(String)                                                            \
    syntax(ExprList)                                                          \
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
    syntax(CompareOp)                                                         \
    syntax(BinaryOp)                                                          \
    syntax(AttrRef)                                                           \
    syntax(Assign)                                                            \
    syntax(TargetList)                                                        \
    syntax(Call)                                                              \
    syntax(Return)                                                            \
    syntax(Cond)                                                              \
    syntax(Lambda)                                                            \
    syntax(Def)                                                               \
    syntax(If)                                                                \
    syntax(While)                                                             \
    syntax(Subscript)                                                         \
    syntax(Assert)                                                            \
    syntax(Class)                                                             \
    syntax(Raise)                                                             \
    syntax(For)                                                               \
    syntax(Global)                                                            \
    syntax(AugAssign)                                                         \
    syntax(Yield)

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

template <typename T>
struct OwningVector : public vector<T*>
{
    OwningVector() {}

    OwningVector(const vector<T*>& other) : vector<T*>(other) {}

    ~OwningVector() {
        for (auto i = this->begin(); i != this->end(); i++)
            delete *i;
    }
};

struct Syntax
{
    Syntax(const Token& token) : token_(token) {}

    template <typename T> bool is() const { return type() == T::Type; }

    template <typename T> T* as() {
        assert(is<T>());
        return static_cast<T*>(this);
    }

    template <typename T> const T* as() const {
        assert(is<T>());
        return static_cast<const T*>(this);
    }

    const Token& token() const { return token_; }

    virtual ~Syntax() {}
    virtual SyntaxType type() const = 0;
    virtual string name() const = 0;
    virtual void print(ostream& s) const = 0;
    virtual void accept(SyntaxVisitor& v) const = 0;

  private:
    Token token_;
};

inline ostream& operator<<(ostream& s, const Syntax& syntax) {
    syntax.print(s);
    return s;
}

#define define_syntax_type(st)                                                \
    static const SyntaxType Type = st;                                        \
    virtual SyntaxType type() const override { return Type; }

#define define_syntax_name(nm)                                                \
    virtual string name() const override { return nm; }

#define define_syntax_accept()                                                \
    virtual void accept(SyntaxVisitor& v) const override { v.visit(*this); }

#define define_syntax_members(st, nm)                                         \
    define_syntax_type(Syntax_##st)                                           \
    define_syntax_name(nm)                                                    \
    define_syntax_accept()

struct UnarySyntax : public Syntax
{
    UnarySyntax(const Token& token, Syntax* r) : Syntax(token), right_(r) {}
    const Syntax* right() const { return right_.get(); }

    virtual void print(ostream& s) const override {
        s << name() << " " << *right();
    }

  private:
    unique_ptr<Syntax> right_;
};

template <typename Base, typename L, typename R>
struct BinarySyntax : public Base
{
    BinarySyntax(const Token& token, L* l, R* r)
      : Base(token), left_(l), right_(r)
    {}

    const L* left() const { return left_.get(); }
    const R* right() const { return right_.get(); }

    virtual void print(ostream& s) const override {
        s << *left() << " " << this->name() << " " << *right();
    }

  private:
    unique_ptr<L> left_;
    unique_ptr<R> right_;
};

#define define_binary_syntax(BaseType, LeftType, RightType, name, nameStr)    \
    struct Syntax##name : public BinarySyntax<BaseType, LeftType, RightType>  \
    {                                                                         \
        define_syntax_members(name, nameStr);                                 \
        Syntax##name(const Token& token, LeftType* l, RightType* r)           \
          : BinarySyntax<BaseType, LeftType, RightType>(token, l, r)          \
        {}                                                                    \
    }

#define define_simple_binary_syntax(name, nameStr)                            \
    define_binary_syntax(Syntax, Syntax, Syntax, name, nameStr)

#define define_unary_syntax(name, nameStr)                                    \
    struct Syntax##name : public UnarySyntax                                  \
    {                                                                         \
        define_syntax_members(name, nameStr);                                 \
        Syntax##name(const Token& token, Syntax* r)                           \
          : UnarySyntax(token, r)                                             \
        {}                                                                    \
    }

struct SyntaxTarget : Syntax
{
    SyntaxTarget(const Token& token) : Syntax(token) {}
};

struct SyntaxSingleTarget : SyntaxTarget
{
    SyntaxSingleTarget(const Token& token) : SyntaxTarget(token) {}
};

struct SyntaxPass : public Syntax
{
    define_syntax_members(Pass, "pass");
    SyntaxPass(const Token& token) : Syntax(token) {}

    virtual void print(ostream& s) const override {
        s << "pass";
    }
};

struct SyntaxBlock : public Syntax
{
    define_syntax_members(Block, "block");
    SyntaxBlock(const Token& token) : Syntax(token) {}

    void append(Syntax* s) { statements.push_back(s); }
    const vector<Syntax *>& stmts() const { return statements; }

    virtual void print(ostream& s) const override {
        for (auto i = statements.begin(); i != statements.end(); ++i) {
            if (i != statements.begin())
                s << endl;
            s << **i;
        }
    }

  private:
    OwningVector<Syntax> statements;
};

struct SyntaxInteger : public Syntax
{
    define_syntax_members(Integer, "integer");

    // todo: this probably doesn't correctly parse all python integers
    SyntaxInteger(const Token& token)
      : Syntax(token), value_(atoi(token.text.c_str()))
    {}

    int value() const { return value_; }

    virtual void print(ostream& s) const override {
        s << value_;
    }

  private:
    int value_;
};

struct SyntaxString : public Syntax
{
    define_syntax_members(String, "string");
    SyntaxString(const Token& token) : Syntax(token), value_(token.text) {}

    const string& value() const { return value_; }

    virtual void print(ostream& s) const override {
        s << "'" << value_ << "'";
    }

  private:
    string value_;
};

struct SyntaxName : public SyntaxSingleTarget
{
    define_syntax_members(Name, "name");

    SyntaxName(const Token& token)
      : SyntaxSingleTarget(token), id_(token.text) {}

    Name id() const { return id_; }

    virtual void print(ostream& s) const override {
        s << id_;
    }

  private:
    Name id_;
};

struct SyntaxExprList : public Syntax
{
    define_syntax_members(ExprList, "exprList");

    SyntaxExprList(const Token& token) : Syntax(token) {}

    SyntaxExprList(const Token& token, const vector<Syntax*> elems)
      : Syntax(token), elements(elems)
    {}

    const vector<Syntax *>& elems() const { return elements; }

    void release() { elements.clear(); }

    virtual void print(ostream& s) const override {
        s << "(";
        for (auto i = elements.begin(); i != elements.end(); ++i) {
            if (i != elements.begin())
                s << ", ";
            s << **i;
        }
        if (elements.size() == 1)
                s << ",";
        s << ")";
    }

  private:
    OwningVector<Syntax> elements;
};

struct SyntaxList : public Syntax
{
    define_syntax_members(List, "list");

    SyntaxList(const Token& token, const vector<Syntax*> elems)
      : Syntax(token), elements(elems) {}

    const vector<Syntax *>& elems() const { return elements; }

    void release() { elements.clear(); }

    virtual void print(ostream& s) const override {
        s << "[";
        for (auto i = elements.begin(); i != elements.end(); ++i) {
            if (i != elements.begin())
                s << ", ";
            s << **i;
        }
        s << "]";
    }

  private:
    OwningVector<Syntax> elements;
};

struct SyntaxDict : public Syntax
{
    define_syntax_members(Dict, "dict");

    typedef pair<Syntax*, Syntax*> Entry;

    SyntaxDict(const Token& token, const vector<Entry>& entries)
      : Syntax(token), entries_(entries) {}

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
            s << *i->first << ": " << *i->second;
        }
        s << "}";
    }

  private:
    vector<Entry> entries_;
};

define_unary_syntax(Neg, "-");
define_unary_syntax(Pos, "+");
define_unary_syntax(Invert, "~");
define_unary_syntax(Not, "not");

define_simple_binary_syntax(Or, "or");
define_simple_binary_syntax(And, "and");
define_simple_binary_syntax(In, "in");
define_simple_binary_syntax(Is, "is");

struct SyntaxBinaryOp : public BinarySyntax<Syntax, Syntax, Syntax>
{
    define_syntax_type(Syntax_BinaryOp);
    define_syntax_accept();

    SyntaxBinaryOp(const Token& token, Syntax* l, Syntax* r, BinaryOp op)
      : BinarySyntax<Syntax, Syntax, Syntax>(token, l, r), op_(op)
    {}

    virtual string name() const override {
        return BinaryOpNames[op_];
    }

    BinaryOp op() const { return op_; }

  private:
    BinaryOp op_;
};

struct SyntaxAugAssign : public BinarySyntax<Syntax, SyntaxSingleTarget, Syntax>
{
    define_syntax_type(Syntax_BinaryOp);
    define_syntax_accept();

    SyntaxAugAssign(const Token& token, SyntaxSingleTarget* l, Syntax* r, BinaryOp op)
      : BinarySyntax<Syntax, SyntaxSingleTarget, Syntax>(token, l, r), op_(op)
    {}

    virtual string name() const override {
        return AugAssignNames[op_];
    }

    BinaryOp op() const { return op_; }

  private:
    BinaryOp op_;
};

struct SyntaxCompareOp : public BinarySyntax<Syntax, Syntax, Syntax>
{
    define_syntax_type(Syntax_CompareOp);
    define_syntax_accept();

    SyntaxCompareOp(const Token& token, Syntax* l, Syntax* r, CompareOp op)
      : BinarySyntax<Syntax, Syntax, Syntax>(token, l, r), op_(op)
    {}

    virtual string name() const override {
        return CompareOpNames[op()];
    }

    CompareOp op() const { return op_; }

  private:
    CompareOp op_;
};

struct SyntaxAttrRef : public BinarySyntax<SyntaxSingleTarget, Syntax, SyntaxName>
{
    define_syntax_members(AttrRef, ".");

    SyntaxAttrRef(const Token& token, Syntax* l, SyntaxName* r)
      : BinarySyntax<SyntaxSingleTarget, Syntax, SyntaxName>(token, l, r)
    {}

    virtual void print(ostream& s) const override {
        if (left()->is<SyntaxName>() || left()->is<SyntaxExprList>() ||
            left()->is<SyntaxAttrRef>())
        {
            s << *left();
        } else {
            s << "(" << *left() << ")";
        }
        s << "." << *right();
    }
};

struct SyntaxTargetList : SyntaxTarget
{
    define_syntax_members(TargetList, "targetList");

    SyntaxTargetList(const Token& token, const vector<SyntaxTarget*>& targets)
      : SyntaxTarget(token), targets_(targets)
    {}

    const vector<SyntaxTarget*>& targets() const { return targets_; }

    virtual void print(ostream& s) const override {
        s << "(";
        for (auto i = targets_.begin(); i != targets_.end(); i++) {
            if (i != targets_.begin())
                s << ", ";
            s << **i;
        }
        s << ")";
    }

  private:
    OwningVector<SyntaxTarget> targets_;
};

define_binary_syntax(Syntax, SyntaxTarget, Syntax, Assign, "=");

struct SyntaxSubscript : public BinarySyntax<SyntaxSingleTarget, Syntax, Syntax>
{
    define_syntax_members(Subscript, "subscript");

    SyntaxSubscript(const Token& token, Syntax* l, Syntax* r)
      : BinarySyntax<SyntaxSingleTarget, Syntax, Syntax>(token, l, r)
    {}

    virtual void print(ostream& s) const override {
        s << *left() << "[" << *right() << "]";
    }
};

struct SyntaxCall : public Syntax
{
    define_syntax_members(Call, "call");

    SyntaxCall(const Token& token, Syntax* l, const vector<Syntax *>& r)
      : Syntax(token), left_(l), right_(r)
    {}

    const Syntax* left() const { return left_.get(); }
    const vector<Syntax*>& right() const { return right_; }

    virtual void print(ostream& s) const override {
        s << *left() << "(";
        for (auto i = right_.begin(); i != right_.end(); ++i) {
            if (i != right_.begin())
                s << ", ";
            s << **i;
        }
        s << ")";
    }

  private:
    unique_ptr<Syntax> left_;
    OwningVector<Syntax> right_;
};

struct SyntaxReturn : public UnarySyntax
{
    define_syntax_members(Return, "return");

    SyntaxReturn(const Token& token, Syntax* right)
      : UnarySyntax(token, right) {}
};

struct SyntaxCond : public Syntax
{
    define_syntax_members(Cond, "cond");

    SyntaxCond(const Token& token, Syntax* cons, Syntax* cond, Syntax* alt)
      : Syntax(token), cons_(cons), cond_(cond), alt_(alt) {}

    const Syntax* cons() const { return cons_.get(); }
    const Syntax* cond() const { return cond_.get(); }
    const Syntax* alt() const { return alt_.get(); }

    virtual void print(ostream& s) const override {
        s << *cons() << " if " << *cond() << " else " << *alt();
    }

  private:
    unique_ptr<Syntax> cons_;
    unique_ptr<Syntax> cond_;
    unique_ptr<Syntax> alt_;
};

struct Parameter
{
    Name name;
    Syntax* maybeDefault;
};

struct SyntaxLambda : public Syntax
{
    define_syntax_members(Lambda, "lambda");

    SyntaxLambda(const Token& token, const vector<Parameter>& params,
                 Syntax* expr)
        : Syntax(token), params_(params), expr_(expr)
    {}

    ~SyntaxLambda() {
        for (auto i = params_.begin(); i != params_.end(); ++i)
            delete i->maybeDefault;
    }

    const vector<Parameter>& params() const { return params_; }
    Syntax* expr() const { return expr_.get(); }

    virtual void print(ostream& s) const override {
        s << "lambda";
        for (auto i = params_.begin(); i != params_.end(); ++i)
            s << " " << i->name;
        s << ": ";
        expr_->print(s);
    }

  private:
    vector<Parameter> params_;
    unique_ptr<Syntax> expr_;
};

struct SyntaxDef : public Syntax
{
    define_syntax_members(Def, "def");

    SyntaxDef(const Token& token,
              Name id,
              const vector<Parameter>& params,
              Syntax* expr,
              bool isGenerator)
      : Syntax(token),
        id_(id),
        params_(params),
        expr_(expr),
        isGenerator_(isGenerator)
    {}

    Name id() const { return id_; }
    const vector<Parameter>& params() const { return params_; }
    Syntax* expr() const { return expr_; }
    bool isGenerator() const { return isGenerator_; }

    virtual void print(ostream& s) const override {
        s << "def " << id_ << "(";
        for (auto i = params_.begin(); i != params_.end(); ++i) {
            if (i != params_.begin())
                s << ", ";
            s << i->name;
        }
        s << "):" << endl;
        expr_->print(s);
    }

  private:
    Name id_;
    vector<Parameter> params_;
    Syntax* expr_;
    bool isGenerator_;
};

struct SyntaxIf : public Syntax
{
    define_syntax_members(If, "if");

    struct Branch
    {
        Syntax* cond;
        SyntaxBlock* block;
    };

    SyntaxIf(const Token& token) : Syntax(token) {}

    ~SyntaxIf() {
        for (auto i = branches_.begin(); i != branches_.end(); ++i) {
            Branch& branch = *i;
            delete branch.cond;
            delete branch.block;
        }
    }

    void addBranch(Syntax* cond, SyntaxBlock* block) {
        branches_.push_back({ cond, block });
    }

    void setElse(SyntaxBlock* block) {
        else_.reset(block);
    }

    const vector<Branch>& branches() const { return branches_; }
    const SyntaxBlock* elseBranch() const { return else_.get(); }

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
    define_syntax_members(While, "while");

    SyntaxWhile(const Token& token, Syntax* cond, SyntaxBlock* suite,
              SyntaxBlock* elseSuite)
      : Syntax(token), cond_(cond), suite_(suite), elseSuite_(elseSuite)
    {}

    const Syntax* cond() const { return cond_.get(); }
    const Syntax* suite() const { return suite_.get(); }
    const Syntax* elseSuite() const { return elseSuite_.get(); }

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
    define_syntax_members(Assert, "assert");

    SyntaxAssert(const Token& token, Syntax* cond, Syntax* message)
      : Syntax(token), cond_(cond), message_(message)
    {}

    const Syntax* cond() const { return cond_.get(); }
    const Syntax* message() const { return message_.get(); }

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
    define_syntax_members(Class, "class");

    SyntaxClass(const Token& token, Name id, SyntaxExprList* bases, SyntaxBlock* suite)
      : Syntax(token), id_(id), bases_(bases), suite_(suite)
    {}

    Name id() const { return id_; }
    const SyntaxExprList* bases() const { return bases_.get(); }
    const Syntax* suite() const { return suite_.get(); }

    virtual void print(ostream& s) const override {
        // todo: indentation
        s << "class " << id();
        if (!bases_->elems().empty())
            s << bases() << " ";
        s << ":" << endl << suite() << endl;
    }

  private:
    Name id_;
    unique_ptr<SyntaxExprList> bases_;
    unique_ptr<SyntaxBlock> suite_;
};

struct SyntaxRaise : public UnarySyntax
{
    SyntaxRaise(const Token& token, Syntax* right) : UnarySyntax(token, right) {}
    define_syntax_type(Syntax_Raise)
    define_syntax_name("raise")
    define_syntax_accept()
};

struct SyntaxFor : public Syntax
{
    define_syntax_members(For, "for");

    SyntaxFor(const Token& token, SyntaxTarget* targets, Syntax* exprs,
              SyntaxBlock* suite, SyntaxBlock* elseSuite)
      : Syntax(token), targets_(targets), exprs_(exprs), suite_(suite),
        elseSuite_(elseSuite)
    {}

    const SyntaxTarget* targets() const { return targets_.get(); }
    const Syntax* exprs() const { return exprs_.get(); }
    const SyntaxBlock* suite() const { return suite_.get(); }
    const SyntaxBlock* elseSuite() const { return elseSuite_.get(); }

    virtual void print(ostream& s) const override {
        // todo: indentation
        s << "for " << *targets() << " in " << *exprs() << ":" << endl;
        s << *suite();
        if (elseSuite()) {
            s << endl << "else: " << endl;
            s << *elseSuite();
        }
    }

  private:
    unique_ptr<SyntaxTarget> targets_;
    unique_ptr<Syntax> exprs_;
    unique_ptr<SyntaxBlock> suite_;
    unique_ptr<SyntaxBlock> elseSuite_;
};

struct SyntaxGlobal : public Syntax
{
    define_syntax_members(Global, "global");

    SyntaxGlobal(const Token& token, vector<Name> names)
        : Syntax(token), names_(names) {}

    const vector<Name>& names() const { return names_; }

    virtual void print(ostream& s) const override {
        s << "global ";
        for (auto i = names_.begin(); i != names_.end(); i++) {
            if (i != names_.begin())
                s << ", ";
            s << *i;
        }
    }

  private:
    vector<Name> names_;
};

define_unary_syntax(Yield, "yield");

#undef define_syntax_type
#undef define_syntax_name
#undef define_syntax_accept
#undef define_syntax_members
#undef define_binary_syntax
#undef define_simple_binary_syntax
#undef define_unary_syntax

#endif
