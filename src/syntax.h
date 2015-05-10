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
    syntax(Yield)                                                             \
    syntax(Try)                                                               \
    syntax(Break)                                                             \
    syntax(Continue)

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
    virtual void setPos(const TokenPos& pos) = 0;
#define syntax_visitor(name)                                                  \
    virtual void visit(const Syntax##name&) = 0;
    for_each_syntax(syntax_visitor)
#undef syntax_visitor
};

struct DefaultSyntaxVisitor : public SyntaxVisitor
{
    virtual void setPos(const TokenPos& pos) {}
#define syntax_visitor(name)                                                  \
    virtual void visit(const Syntax##name&) override {}
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
    virtual void accept(SyntaxVisitor& v) const = 0;

  protected:
    const Token token_;
};

template <typename T, typename S>
static unique_ptr<T> unique_ptr_as(S&& p)
{
    return unique_ptr<T>(p.release()->template as<T>());
}

ostream& operator<<(ostream& s, const Syntax& syntax);

#define define_syntax_type(st)                                                \
    static const SyntaxType Type = st;                                        \
    virtual SyntaxType type() const override { return Type; }

#define define_syntax_name(nm)                                                \
    virtual string name() const override { return nm; }

#define define_syntax_accept()                                                \
    virtual void accept(SyntaxVisitor& v) const override                      \
    {                                                                         \
        v.setPos(token_.pos);                                                 \
        v.visit(*this);                                                       \
    }

#define define_syntax_members(st, nm)                                         \
    define_syntax_type(Syntax_##st)                                           \
    define_syntax_name(nm)                                                    \
    define_syntax_accept()

struct UnarySyntax : public Syntax
{
    UnarySyntax(const Token& token, unique_ptr<Syntax> r)
      : Syntax(token), right_(move(r))
    {}

    const Syntax* right() const { return right_.get(); }

  private:
    unique_ptr<Syntax> right_;
};

template <typename Base, typename L, typename R>
struct BinarySyntax : public Base
{
    BinarySyntax(const Token& token, unique_ptr<L> l, unique_ptr<R> r)
      : Base(token), left_(move(l)), right_(move(r))
    {}

    const L* left() const { return left_.get(); }
    const R* right() const { return right_.get(); }

  private:
    unique_ptr<L> left_;
    unique_ptr<R> right_;
};

#define define_nullary_syntax(name, nameStr)                                  \
    struct Syntax##name : public Syntax                                       \
    {                                                                         \
        define_syntax_members(name, nameStr);                                 \
        Syntax##name(const Token& token)                                      \
          : Syntax(token)                                                     \
        {}                                                                    \
    }

#define define_unary_syntax(name, nameStr)                                    \
    struct Syntax##name : public UnarySyntax                                  \
    {                                                                         \
        define_syntax_members(name, nameStr);                                 \
        Syntax##name(const Token& token, unique_ptr<Syntax> r)                \
          : UnarySyntax(token, move(r))                                       \
        {}                                                                    \
    }

#define define_binary_syntax(BaseType, LeftType, RightType, name, nameStr)    \
    struct Syntax##name : public BinarySyntax<BaseType, LeftType, RightType>  \
    {                                                                         \
        define_syntax_members(name, nameStr);                                 \
        Syntax##name(const Token& token,                                      \
                     unique_ptr<LeftType> l,                                  \
                     unique_ptr<RightType> r)                                 \
          : BinarySyntax<BaseType, LeftType, RightType>(token,                \
                                                        move(l),              \
                                                        move(r))              \
        {}                                                                    \
    }

#define define_simple_binary_syntax(name, nameStr)                            \
    define_binary_syntax(Syntax, Syntax, Syntax, name, nameStr)

struct SyntaxTarget : Syntax
{
    SyntaxTarget(const Token& token) : Syntax(token) {}
};

struct SyntaxSingleTarget : SyntaxTarget
{
    SyntaxSingleTarget(const Token& token) : SyntaxTarget(token) {}
};

define_nullary_syntax(Pass, "pass");
define_nullary_syntax(Break, "break");
define_nullary_syntax(Continue, "continue");

struct SyntaxBlock : public Syntax
{
    define_syntax_members(Block, "block");

    SyntaxBlock(const Token& token, vector<unique_ptr<Syntax>> stmts)
      : Syntax(token), statements(move(stmts))
    {}

    const vector<unique_ptr<Syntax>>& stmts() const { return statements; }

  private:
    vector<unique_ptr<Syntax>> statements;
};

struct SyntaxInteger : public Syntax
{
    define_syntax_members(Integer, "integer");

    // todo: this probably doesn't correctly parse all python integers
    SyntaxInteger(const Token& token)
      : Syntax(token), value_(atoi(token.text.c_str()))
    {}

    int value() const { return value_; }

  private:
    int value_;
};

struct SyntaxString : public Syntax
{
    define_syntax_members(String, "string");
    SyntaxString(const Token& token) : Syntax(token), value_(token.text) {}

    const string& value() const { return value_; }

  private:
    string value_;
};

struct SyntaxName : public SyntaxSingleTarget
{
    define_syntax_members(Name, "name");

    SyntaxName(const Token& token)
      : SyntaxSingleTarget(token), id_(token.text) {}

    Name id() const { return id_; }

  private:
    Name id_;
};

struct SyntaxExprList : public Syntax
{
    define_syntax_members(ExprList, "exprList");

    SyntaxExprList(const Token& token) : Syntax(token) {}

    SyntaxExprList(const Token& token, vector<unique_ptr<Syntax>> elems)
      : Syntax(token), elements(move(elems))
    {}

    const vector<unique_ptr<Syntax>>& elems() const { return elements; }
    vector<unique_ptr<Syntax>>& elems() { return elements; }

  private:
    vector<unique_ptr<Syntax>> elements;
};

struct SyntaxList : public Syntax
{
    define_syntax_members(List, "list");

    SyntaxList(const Token& token, vector<unique_ptr<Syntax>> elems)
      : Syntax(token), elements(move(elems)) {}

    const vector<unique_ptr<Syntax>>& elems() const { return elements; }
    vector<unique_ptr<Syntax>>& elems() { return elements; }

    void release() { elements.clear(); }

  private:
    vector<unique_ptr<Syntax>> elements;
};

struct SyntaxDict : public Syntax
{
    define_syntax_members(Dict, "dict");

    typedef pair<unique_ptr<Syntax>, unique_ptr<Syntax>> Entry;

    SyntaxDict(const Token& token, vector<Entry> entries)
      : Syntax(token), entries_(move(entries)) {}

    const vector<Entry>& entries() const { return entries_; }

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

    SyntaxBinaryOp(const Token& token,
                   unique_ptr<Syntax> l,
                   unique_ptr<Syntax> r,
                   BinaryOp op)
      : BinarySyntax<Syntax, Syntax, Syntax>(token, move(l), move(r)), op_(op)
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

    typedef BinarySyntax<Syntax, SyntaxSingleTarget, Syntax> Base;
    SyntaxAugAssign(const Token& token,
                    unique_ptr<SyntaxSingleTarget> l,
                    unique_ptr<Syntax> r,
                    BinaryOp op)
      : Base(token, move(l), move(r)), op_(op)
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

    SyntaxCompareOp(const Token& token,
                    unique_ptr<Syntax> l,
                    unique_ptr<Syntax> r, CompareOp op)
      : BinarySyntax<Syntax, Syntax, Syntax>(token, move(l), move(r)), op_(op)
    {}

    virtual string name() const override {
        return CompareOpNames[op()];
    }

    CompareOp op() const { return op_; }

  private:
    CompareOp op_;
};

struct SyntaxAttrRef : public BinarySyntax<SyntaxSingleTarget,
                                           Syntax, SyntaxName>
{
    define_syntax_members(AttrRef, ".");

    typedef BinarySyntax<SyntaxSingleTarget, Syntax, SyntaxName> Base;
    SyntaxAttrRef(const Token& token,
                  unique_ptr<Syntax> l,
                  unique_ptr<SyntaxName> r)
      : Base(token, move(l), move(r))
    {}
};

struct SyntaxTargetList : SyntaxTarget
{
    define_syntax_members(TargetList, "targetList");

    SyntaxTargetList(const Token& token,
                     vector<unique_ptr<SyntaxTarget>> targets)
      : SyntaxTarget(token), targets_(move(targets))
    {}

    const vector<unique_ptr<SyntaxTarget>>& targets() const { return targets_; }

  private:
    vector<unique_ptr<SyntaxTarget>> targets_;
};

define_binary_syntax(Syntax, SyntaxTarget, Syntax, Assign, "=");

struct SyntaxSubscript : public BinarySyntax<SyntaxSingleTarget, Syntax, Syntax>
{
    define_syntax_members(Subscript, "subscript");

    SyntaxSubscript(const Token& token,
                    unique_ptr<Syntax> l,
                    unique_ptr<Syntax> r)
      : BinarySyntax<SyntaxSingleTarget, Syntax, Syntax>(token,
                                                         move(l),
                                                         move(r))
    {}
};

struct SyntaxCall : public Syntax
{
    define_syntax_members(Call, "call");

    SyntaxCall(const Token& token,
               unique_ptr<Syntax> l,
               vector<unique_ptr<Syntax>> r)
      : Syntax(token), left_(move(l)), right_(move(r))
    {}

    const Syntax* left() const { return left_.get(); }
    const vector<unique_ptr<Syntax>>& right() const { return right_; }

  private:
    unique_ptr<Syntax> left_;
    vector<unique_ptr<Syntax>> right_;
};

struct SyntaxReturn : public UnarySyntax
{
    define_syntax_members(Return, "return");

    SyntaxReturn(const Token& token, unique_ptr<Syntax> right)
      : UnarySyntax(token, move(right)) {}
};

struct SyntaxCond : public Syntax
{
    define_syntax_members(Cond, "cond");

    SyntaxCond(const Token& token,
               unique_ptr<Syntax> cons,
               unique_ptr<Syntax> cond,
               unique_ptr<Syntax> alt)
      : Syntax(token),
        cons_(move(cons)),
        cond_(move(cond)),
        alt_(move(alt))
    {}

    const Syntax* cons() const { return cons_.get(); }
    const Syntax* cond() const { return cond_.get(); }
    const Syntax* alt() const { return alt_.get(); }

  private:
    unique_ptr<Syntax> cons_;
    unique_ptr<Syntax> cond_;
    unique_ptr<Syntax> alt_;
};

struct Parameter
{
    Parameter(Name name, unique_ptr<Syntax> maybeDefault, bool takesRest)
      : name(name), maybeDefault(move(maybeDefault)), takesRest(takesRest)
    {}

    Name name;
    unique_ptr<Syntax> maybeDefault;
    bool takesRest;
};

struct SyntaxLambda : public Syntax
{
    define_syntax_members(Lambda, "lambda");

    SyntaxLambda(const Token& token, vector<Parameter> params,
                 unique_ptr<Syntax> expr)
        : Syntax(token), params_(move(params)), expr_(move(expr))
    {}

    const vector<Parameter>& params() const { return params_; }
    Syntax* expr() const { return expr_.get(); }

  private:
    vector<Parameter> params_;
    unique_ptr<Syntax> expr_;
};

struct SyntaxDef : public Syntax
{
    define_syntax_members(Def, "def");

    SyntaxDef(const Token& token,
              Name id,
              vector<Parameter> params,
              unique_ptr<Syntax> expr,  // todo: should be block?
              bool isGenerator)
      : Syntax(token),
        id_(id),
        params_(move(params)),
        expr_(move(expr)),
        isGenerator_(isGenerator)
    {}

    Name id() const { return id_; }
    const vector<Parameter>& params() const { return params_; }
    Syntax* expr() const { return expr_.get(); }
    bool isGenerator() const { return isGenerator_; }

  private:
    Name id_;
    vector<Parameter> params_;
    unique_ptr<Syntax> expr_;
    bool isGenerator_;
};

struct SyntaxIf : public Syntax
{
    define_syntax_members(If, "if");

    struct Branch
    {
        Branch(unique_ptr<Syntax> cond, unique_ptr<SyntaxBlock> block)
          : cond(move(cond)), block(move(block))
        {}

        unique_ptr<Syntax> cond;
        unique_ptr<SyntaxBlock> block;
    };

    SyntaxIf(const Token& token) : Syntax(token) {}

    void addBranch(unique_ptr<Syntax> cond, unique_ptr<SyntaxBlock> block) {
        branches_.emplace_back(move(cond), move(block));
    }

    void setElse(unique_ptr<SyntaxBlock> block) {
        else_ = move(block);
    }

    const vector<Branch>& branches() const { return branches_; }
    const SyntaxBlock* elseBranch() const { return else_.get(); }

  private:
    vector<Branch> branches_;
    unique_ptr<SyntaxBlock> else_;
};

struct SyntaxWhile : public Syntax
{
    define_syntax_members(While, "while");

    SyntaxWhile(const Token& token,
                unique_ptr<Syntax> cond,
                unique_ptr<SyntaxBlock> suite,
                unique_ptr<SyntaxBlock> elseSuite)
      : Syntax(token),
        cond_(move(cond)),
        suite_(move(suite)),
        elseSuite_(move(elseSuite))
    {}

    const Syntax* cond() const { return cond_.get(); }
    const Syntax* suite() const { return suite_.get(); }
    const Syntax* elseSuite() const { return elseSuite_.get(); }

  private:
    unique_ptr<Syntax> cond_;
    unique_ptr<SyntaxBlock> suite_;
    unique_ptr<SyntaxBlock> elseSuite_;
};

struct SyntaxAssert : public Syntax
{
    define_syntax_members(Assert, "assert");

    SyntaxAssert(const Token& token,
                 unique_ptr<Syntax> cond,
                 unique_ptr<Syntax> message)
        : Syntax(token), cond_(move(cond)), message_(move(message))
    {}

    const Syntax* cond() const { return cond_.get(); }
    const Syntax* message() const { return message_.get(); }

  private:
    unique_ptr<Syntax> cond_;
    unique_ptr<Syntax> message_;
};

struct SyntaxClass : public Syntax
{
    define_syntax_members(Class, "class");

    SyntaxClass(const Token& token,
                Name id,
                unique_ptr<SyntaxExprList> bases,
                unique_ptr<SyntaxBlock> suite)
      : Syntax(token), id_(id), bases_(move(bases)), suite_(move(suite))
    {}

    Name id() const { return id_; }
    const SyntaxExprList* bases() const { return bases_.get(); }
    const Syntax* suite() const { return suite_.get(); }

  private:
    Name id_;
    unique_ptr<SyntaxExprList> bases_;
    unique_ptr<SyntaxBlock> suite_;
};

struct SyntaxRaise : public UnarySyntax
{
    SyntaxRaise(const Token& token, unique_ptr<Syntax> right)
      : UnarySyntax(token, move(right)) {}
    define_syntax_type(Syntax_Raise)
    define_syntax_name("raise")
    define_syntax_accept()
};

struct SyntaxFor : public Syntax
{
    define_syntax_members(For, "for");

    SyntaxFor(const Token& token,
              unique_ptr<SyntaxTarget> targets,
              unique_ptr<Syntax> exprs,
              unique_ptr<SyntaxBlock> suite,
              unique_ptr<SyntaxBlock> elseSuite)
      : Syntax(token),
        targets_(move(targets)),
        exprs_(move(exprs)),
        suite_(move(suite)),
        elseSuite_(move(elseSuite))
    {}

    const SyntaxTarget* targets() const { return targets_.get(); }
    const Syntax* exprs() const { return exprs_.get(); }
    const SyntaxBlock* suite() const { return suite_.get(); }
    const SyntaxBlock* elseSuite() const { return elseSuite_.get(); }

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

  private:
    vector<Name> names_;
};

define_unary_syntax(Yield, "yield");

struct ExceptInfo
{
    ExceptInfo(const Token& token,
               unique_ptr<Syntax> expr,
               unique_ptr<SyntaxTarget> as,
               unique_ptr<SyntaxBlock> suite)
      : token_(token),
        expr_(move(expr)),
        as_(move(as)),
        suite_(move(suite))
    {}

    const Token& token() const { return token_; }
    const Syntax* expr() const { return expr_.get(); }
    const SyntaxTarget* as() const { return as_.get(); }
    const SyntaxBlock* suite() const { return suite_.get(); }

  private:
    Token token_;
    unique_ptr<Syntax> expr_;
    unique_ptr<SyntaxTarget> as_;
    unique_ptr<SyntaxBlock> suite_;
};

struct SyntaxTry : public Syntax
{
    define_syntax_members(Try, "try");

    SyntaxTry(const Token& token,
              unique_ptr<SyntaxBlock> trySuite,
              vector<unique_ptr<ExceptInfo>> excepts,
              unique_ptr<SyntaxBlock> elseSuite,
              unique_ptr<SyntaxBlock> finallySuite)
      : Syntax(token),
        trySuite_(move(trySuite)),
        excepts_(move(excepts)),
        elseSuite_(move(elseSuite)),
        finallySuite_(move(finallySuite))
    {}

    const SyntaxBlock* trySuite() const { return trySuite_.get(); }
    const vector<unique_ptr<ExceptInfo>>& excepts() const { return excepts_; }
    const SyntaxBlock* elseSuite() const { return elseSuite_.get(); }
    const SyntaxBlock* finallySuite() const { return finallySuite_.get(); }

  private:
    unique_ptr<SyntaxBlock> trySuite_;
    vector<unique_ptr<ExceptInfo>> excepts_;
    unique_ptr<SyntaxBlock> elseSuite_;
    unique_ptr<SyntaxBlock> finallySuite_;
};

#undef define_syntax_type
#undef define_syntax_name
#undef define_syntax_accept
#undef define_syntax_members
#undef define_binary_syntax
#undef define_simple_binary_syntax
#undef define_unary_syntax

#endif
