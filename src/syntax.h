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
    syntax(LT)                                                                \
    syntax(LE)                                                                \
    syntax(GT)                                                                \
    syntax(GE)                                                                \
    syntax(EQ)                                                                \
    syntax(NE)                                                                \
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
    syntax(Global)

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

#define syntax_type(st)                                                       \
    static const SyntaxType Type = st;                                        \
    virtual SyntaxType type() const override { return Type; }

#define syntax_name(nm)                                                       \
    virtual string name() const override { return string(nm); }

#define syntax_accept()                                                      \
    virtual void accept(SyntaxVisitor& v) const override { v.visit(*this); }

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
        Syntax##name(const Token& token, LeftType* l, RightType* r)           \
          : BinarySyntax<BaseType, LeftType, RightType>(token, l, r)          \
        {}                                                                    \
        syntax_type(Syntax_##name)                                            \
        syntax_name(nameStr)                                                  \
        syntax_accept()                                                       \
    }

#define define_simple_binary_syntax(name, nameStr)                            \
    define_binary_syntax(Syntax, Syntax, Syntax, name, nameStr)

#define define_unary_syntax(name, nameStr)                                    \
    struct Syntax##name : public UnarySyntax                                  \
    {                                                                         \
        Syntax##name(const Token& token, Syntax* r)                           \
          : UnarySyntax(token, r)                                             \
        {}                                                                    \
        syntax_type(Syntax_##name)                                            \
        syntax_name(nameStr)                                                  \
        syntax_accept()                                                       \
    }

struct SyntaxTarget : Syntax
{
    SyntaxTarget(const Token& token) : Syntax(token) {}
};

struct SyntaxPass : public Syntax
{
    SyntaxPass(const Token& token) : Syntax(token) {}

    syntax_type(Syntax_Pass)
    syntax_name("pass")

    virtual void print(ostream& s) const override {
        s << "pass";
    }

    syntax_accept()
};

struct SyntaxBlock : public Syntax
{
    SyntaxBlock(const Token& token) : Syntax(token) {}

    ~SyntaxBlock() {
        for (auto i = statements.begin(); i != statements.end(); ++i)
            delete *i;
    }

    syntax_type(Syntax_Block);
    syntax_name("block");

    void append(Syntax* s) { statements.push_back(s); }
    const vector<Syntax *>& stmts() const { return statements; }

    virtual void print(ostream& s) const override {
        for (auto i = statements.begin(); i != statements.end(); ++i)
            s << **i << endl;
    }

    syntax_accept();

  private:
    vector<Syntax *> statements;
};

struct SyntaxInteger : public Syntax
{
    // todo: this probably doesn't correctly parse all python integers
    SyntaxInteger(const Token& token)
      : Syntax(token), value_(atoi(token.text.c_str()))
    {}

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
    SyntaxString(const Token& token) : Syntax(token), value_(token.text) {}

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

struct SyntaxName : public SyntaxTarget
{
    SyntaxName(const Token& token) : SyntaxTarget(token), id_(token.text) {}

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

struct SyntaxExprList : public Syntax
{
    syntax_type(Syntax_ExprList);
    syntax_name("exprList");

    SyntaxExprList(const Token& token) : Syntax(token) {}

    SyntaxExprList(const Token& token, const vector<Syntax*> elems)
      : Syntax(token), elements(elems)
    {}

    ~SyntaxExprList() {
        for (auto i = elements.begin(); i != elements.end(); ++i)
            delete *i;
    }

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

    syntax_accept();

  private:
    vector<Syntax *> elements;
};

struct SyntaxList : public Syntax
{
    SyntaxList(const Token& token, const vector<Syntax*> elems)
      : Syntax(token), elements(elems) {}

    ~SyntaxList() {
        for (auto i = elements.begin(); i != elements.end(); ++i)
            delete *i;
    }

    syntax_type(Syntax_List);
    syntax_name("List");

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

    syntax_accept();

  private:
    vector<Syntax *> elements;
};

struct SyntaxDict : public Syntax
{
    typedef pair<Syntax*, Syntax*> Entry;

    SyntaxDict(const Token& token, const vector<Entry>& entries)
      : Syntax(token), entries_(entries) {}

    ~SyntaxDict() {
        for (auto i = entries_.begin(); i != entries_.end(); ++i) {
            delete i->first;
            delete i->second;
        }
    }

    syntax_type(Syntax_Dict);
    syntax_name("Dict");

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

    syntax_accept();

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

enum BinaryOp
{
    BinaryPlus,
    BinaryMinus,
    BinaryMultiply,
    BinaryDivide,
    BinaryIntDivide,
    BinaryModulo,
    BinaryPower,
    BinaryOr,
    BinaryXor,
    BinaryAnd,
    BinaryLeftShift,
    BinaryRightShift,

    CountBinaryOp
};

struct SyntaxBinaryOp : public BinarySyntax<Syntax, Syntax, Syntax>
{
    SyntaxBinaryOp(const Token& token, Syntax* l, Syntax* r, BinaryOp op)
      : BinarySyntax<Syntax, Syntax, Syntax>(token, l, r), op_(op)
    {}

    syntax_type(Syntax_BinaryOp);
    syntax_accept();

    virtual string name() const override {
        static const char* names[] = {
            "+", "-", "*", "/", "//", "%", "**", "|", "^", "&", "<<", ">>"
        };
        static_assert(sizeof(names)/sizeof(*names) == CountBinaryOp,
            "Number of names must match number of binary operations");
        return names[op_];
    }

    BinaryOp op() const { return op_; }

  private:
    BinaryOp op_;
};

define_simple_binary_syntax(LT, "<");
define_simple_binary_syntax(LE, "<=");
define_simple_binary_syntax(GT, ">");
define_simple_binary_syntax(GE, ">=");
define_simple_binary_syntax(EQ, "!=");
define_simple_binary_syntax(NE, "==");

struct SyntaxAttrRef : public BinarySyntax<SyntaxTarget, Syntax, SyntaxName>
{
    SyntaxAttrRef(const Token& token, Syntax* l, SyntaxName* r)
      : BinarySyntax<SyntaxTarget, Syntax, SyntaxName>(token, l, r)
    {}

    syntax_type(Syntax_AttrRef);
    syntax_name(".");
    syntax_accept();

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
    SyntaxTargetList(const Token& token, const vector<SyntaxTarget*>& targets)
      : SyntaxTarget(token), targets_(targets)
    {}

    syntax_type(Syntax_TargetList);
    syntax_name("targetList");
    syntax_accept();

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
    vector<SyntaxTarget*> targets_;
};

define_binary_syntax(Syntax, SyntaxTarget, Syntax, Assign, "=");

struct SyntaxSubscript : public BinarySyntax<SyntaxTarget, Syntax, Syntax>
{
    SyntaxSubscript(const Token& token, Syntax* l, Syntax* r)
      : BinarySyntax<SyntaxTarget, Syntax, Syntax>(token, l, r)
    {}
    syntax_type(Syntax_Subscript)
    syntax_name("subscript")
    syntax_accept()

    virtual void print(ostream& s) const override {
        s << *left() << "[" << *right() << "]";
    }
};

struct SyntaxCall : public Syntax
{
    SyntaxCall(const Token& token, Syntax* l, const vector<Syntax *>& r)
      : Syntax(token), left_(l), right_(r)
    {}

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
    vector<Syntax*> right_;
};

struct SyntaxReturn : public UnarySyntax
{
    SyntaxReturn(const Token& token, Syntax* right) : UnarySyntax(token, right) {}
    syntax_type(Syntax_Return)
    syntax_name("return")
    syntax_accept()
};

struct SyntaxCond : public Syntax
{
    SyntaxCond(const Token& token, Syntax* cons, Syntax* cond, Syntax* alt)
      : Syntax(token), cons_(cons), cond_(cond), alt_(alt) {}
    syntax_type(Syntax_Cond)
    syntax_name("cond")

    const Syntax* cons() const { return cons_.get(); }
    const Syntax* cond() const { return cond_.get(); }
    const Syntax* alt() const { return alt_.get(); }

    syntax_accept()

    virtual void print(ostream& s) const override {
        s << *cons() << " if " << *cond() << " else " << *alt();
    }

  private:
    unique_ptr<Syntax> cons_;
    unique_ptr<Syntax> cond_;
    unique_ptr<Syntax> alt_;
};

struct SyntaxLambda : public Syntax
{
    SyntaxLambda(const Token& token, const vector<Name>& params, Syntax* expr)
      : Syntax(token), params_(params), expr_(expr)
    {}

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
    SyntaxDef(const Token& token, Name id, const vector<Name>& params, Syntax* expr)
        : Syntax(token), id_(id), params_(params), expr_(expr)
    {}

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

    SyntaxIf(const Token& token) : Syntax(token) {}

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
    SyntaxWhile(const Token& token, Syntax* cond, SyntaxBlock* suite,
              SyntaxBlock* elseSuite)
      : Syntax(token), cond_(cond), suite_(suite), elseSuite_(elseSuite)
    {}

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
    SyntaxAssert(const Token& token, Syntax* cond, Syntax* message)
      : Syntax(token), cond_(cond), message_(message)
    {}

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
    SyntaxClass(const Token& token, Name id, SyntaxExprList* bases, SyntaxBlock* suite)
      : Syntax(token), id_(id), bases_(bases), suite_(suite)
    {}

    syntax_type(Syntax_Class);
    syntax_name("class");

    Name id() const { return id_; }
    const SyntaxExprList* bases() const { return bases_.get(); }
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
    unique_ptr<SyntaxExprList> bases_;
    unique_ptr<SyntaxBlock> suite_;
};

struct SyntaxRaise : public UnarySyntax
{
    SyntaxRaise(const Token& token, Syntax* right) : UnarySyntax(token, right) {}
    syntax_type(Syntax_Raise)
    syntax_name("raise")
    syntax_accept()
};

struct SyntaxFor : public Syntax
{
    SyntaxFor(const Token& token, SyntaxTarget* targets, Syntax* exprs,
              SyntaxBlock* suite, SyntaxBlock* elseSuite)
      : Syntax(token), targets_(targets), exprs_(exprs), suite_(suite),
        elseSuite_(elseSuite)
    {}

    syntax_type(Syntax_For);
    syntax_name("for");

    const SyntaxTarget* targets() const { return targets_.get(); }
    const Syntax* exprs() const { return exprs_.get(); }
    const SyntaxBlock* suite() const { return suite_.get(); }
    const SyntaxBlock* elseSuite() const { return elseSuite_.get(); }

    syntax_accept();

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
    SyntaxGlobal(const Token& token, vector<Name> names)
        : Syntax(token), names_(names) {}

    syntax_type(Syntax_Global);
    syntax_name("global");
    syntax_accept();

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

#endif
