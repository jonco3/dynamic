#ifndef __SYNTAX_H__
#define __SYNTAX_H__

#include "name.h"
#include "specials.h"
#include "token.h"

#include <cassert>
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
    syntax(NonLocal)                                                          \
    syntax(AugAssign)                                                         \
    syntax(Yield)                                                             \
    syntax(Try)                                                               \
    syntax(Break)                                                             \
    syntax(Continue)                                                          \
    syntax(Del)

enum class SyntaxType
{
#define syntax_enum(name) name,
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
    Syntax(const Token& token) : token(token) {}

    template <typename T> bool is() const { return type() == T::Type; }

    template <typename T> T* as() {
        assert(is<T>());
        return static_cast<T*>(this);
    }

    virtual ~Syntax() {}
    virtual SyntaxType type() const = 0;
    virtual string name() const = 0;
    virtual void accept(SyntaxVisitor& v) const = 0;

    const Token token;
};

template <typename T, typename S>
static unique_ptr<T> unique_ptr_as(S&& p)
{
    return unique_ptr<T>(p.release()->template as<T>());
}

ostream& operator<<(ostream& s, const Syntax& syntax);

#define define_syntax_type(st)                                                \
    static const SyntaxType Type = SyntaxType::st;                            \
    virtual SyntaxType type() const override { return Type; }

#define define_syntax_name(nm)                                                \
    virtual string name() const override { return nm; }

#define define_syntax_accept()                                                \
    virtual void accept(SyntaxVisitor& v) const override                      \
    {                                                                         \
        v.setPos(token.pos);                                                  \
        v.visit(*this);                                                       \
    }

#define define_syntax_members(st, nm)                                         \
    define_syntax_type(st)                                                    \
    define_syntax_name(nm)                                                    \
    define_syntax_accept()

struct UnarySyntax : public Syntax
{
    UnarySyntax(const Token& token, unique_ptr<Syntax> r)
      : Syntax(token), right(move(r))
    {}

    const unique_ptr<Syntax> right;
};

template <typename BaseT, typename LeftT, typename RightT>
struct BinarySyntax : public BaseT
{
    BinarySyntax(const Token& token, unique_ptr<LeftT> l, unique_ptr<RightT> r)
      : BaseT(token), left(move(l)), right(move(r))
    {}

    const unique_ptr<LeftT> left;
    const unique_ptr<RightT> right;
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

    const vector<unique_ptr<Syntax>> statements;
};

struct SyntaxInteger : public Syntax
{
    define_syntax_members(Integer, "integer");

    // todo: this probably doesn't correctly parse all python integers
    SyntaxInteger(const Token& token)
      : Syntax(token), value(atoi(token.text.c_str()))
    {}

    const int value;
};

struct SyntaxString : public Syntax
{
    define_syntax_members(String, "string");
    SyntaxString(const Token& token) : Syntax(token), value(token.text) {}

    const string value;
};

struct SyntaxName : public SyntaxSingleTarget
{
    define_syntax_members(Name, "name");

    SyntaxName(const Token& token)
      : SyntaxSingleTarget(token), id(token.text) {}

    const Name id;
};

struct SyntaxExprList : public Syntax
{
    define_syntax_members(ExprList, "exprList");

    SyntaxExprList(const Token& token) : Syntax(token) {}

    SyntaxExprList(const Token& token, vector<unique_ptr<Syntax>> elems)
      : Syntax(token), elements(move(elems))
    {}

    // todo: non-const because of converstion to SyntaxTargetList in
    // SyntaxParser::makeAssignTarget()
    vector<unique_ptr<Syntax>> elements;
};

struct SyntaxList : public Syntax
{
    define_syntax_members(List, "list");

    SyntaxList(const Token& token, vector<unique_ptr<Syntax>> elems)
      : Syntax(token), elements(move(elems)) {}

    // todo: non-const because of converstion to SyntaxTargetList in
    // SyntaxParser::makeAssignTarget()
    vector<unique_ptr<Syntax>> elements;
};

struct SyntaxDict : public Syntax
{
    define_syntax_members(Dict, "dict");

    typedef pair<unique_ptr<Syntax>, unique_ptr<Syntax>> Entry;

    SyntaxDict(const Token& token, vector<Entry> entries)
      : Syntax(token), entries(move(entries)) {}

    const vector<Entry> entries;
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
    define_syntax_type(BinaryOp);
    define_syntax_accept();

    SyntaxBinaryOp(const Token& token,
                   unique_ptr<Syntax> l,
                   unique_ptr<Syntax> r,
                   BinaryOp op)
      : BinarySyntax<Syntax, Syntax, Syntax>(token, move(l), move(r)), op(op)
    {}

    virtual string name() const override {
        return BinaryOpNames[op];
    }

    const BinaryOp op;
};

struct SyntaxAugAssign : public BinarySyntax<Syntax, SyntaxSingleTarget, Syntax>
{
    define_syntax_type(BinaryOp);
    define_syntax_accept();

    typedef BinarySyntax<Syntax, SyntaxSingleTarget, Syntax> Base;
    SyntaxAugAssign(const Token& token,
                    unique_ptr<SyntaxSingleTarget> l,
                    unique_ptr<Syntax> r,
                    BinaryOp op)
      : Base(token, move(l), move(r)), op(op)
    {}

    virtual string name() const override {
        return AugAssignNames[op];
    }

    const BinaryOp op;
};

struct SyntaxCompareOp : public BinarySyntax<Syntax, Syntax, Syntax>
{
    define_syntax_type(CompareOp);
    define_syntax_accept();

    SyntaxCompareOp(const Token& token,
                    unique_ptr<Syntax> l,
                    unique_ptr<Syntax> r, CompareOp op)
      : BinarySyntax<Syntax, Syntax, Syntax>(token, move(l), move(r)), op(op)
    {}

    virtual string name() const override {
        return CompareOpNames[op];
    }

    const CompareOp op;
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
      : SyntaxTarget(token), targets(move(targets))
    {}

    const vector<unique_ptr<SyntaxTarget>> targets;
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
      : Syntax(token), left(move(l)), right(move(r))
    {}

    const unique_ptr<Syntax> left;
    const vector<unique_ptr<Syntax>> right;
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
        cons(move(cons)),
        cond(move(cond)),
        alt(move(alt))
    {}

    const unique_ptr<Syntax> cons;
    const unique_ptr<Syntax> cond;
    const unique_ptr<Syntax> alt;
};

struct Parameter
{
    Name name;
    unique_ptr<Syntax> maybeDefault;
    bool takesRest;
};

struct SyntaxLambda : public Syntax
{
    define_syntax_members(Lambda, "lambda");

    SyntaxLambda(const Token& token, vector<Parameter> params,
                 unique_ptr<Syntax> expr)
        : Syntax(token), params(move(params)), expr(move(expr))
    {}

    const vector<Parameter> params;
    const unique_ptr<Syntax> expr;
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
        id(id),
        params(move(params)),
        expr(move(expr)),
        isGenerator(isGenerator)
    {}

    const Name id;
    const vector<Parameter> params;
    const unique_ptr<Syntax> expr;
    const bool isGenerator;
};

struct IfBranch
{
    unique_ptr<Syntax> cond;
    unique_ptr<SyntaxBlock> suite;
};

struct SyntaxIf : public Syntax
{
    define_syntax_members(If, "if");

    SyntaxIf(const Token& token,
             vector<IfBranch> branches,
             unique_ptr<SyntaxBlock> elseSuite)
        : Syntax(token), branches(move(branches)), elseSuite(move(elseSuite))
    {}

    const vector<IfBranch> branches;
    const unique_ptr<SyntaxBlock> elseSuite;
};

struct SyntaxWhile : public Syntax
{
    define_syntax_members(While, "while");

    SyntaxWhile(const Token& token,
                unique_ptr<Syntax> cond,
                unique_ptr<SyntaxBlock> suite,
                unique_ptr<SyntaxBlock> elseSuite)
      : Syntax(token),
        cond(move(cond)),
        suite(move(suite)),
        elseSuite(move(elseSuite))
    {}

    const unique_ptr<Syntax> cond;
    const unique_ptr<SyntaxBlock> suite;
    const unique_ptr<SyntaxBlock> elseSuite;
};

struct SyntaxAssert : public Syntax
{
    define_syntax_members(Assert, "assert");

    SyntaxAssert(const Token& token,
                 unique_ptr<Syntax> cond,
                 unique_ptr<Syntax> message)
        : Syntax(token), cond(move(cond)), message(move(message))
    {}

    const unique_ptr<Syntax> cond;
    const unique_ptr<Syntax> message;
};

struct SyntaxClass : public Syntax
{
    define_syntax_members(Class, "class");

    SyntaxClass(const Token& token,
                Name id,
                unique_ptr<SyntaxExprList> bases,
                unique_ptr<SyntaxBlock> suite)
      : Syntax(token), id(id), bases(move(bases)), suite(move(suite))
    {}

    const Name id;
    const unique_ptr<SyntaxExprList> bases;
    const unique_ptr<SyntaxBlock> suite;
};

struct SyntaxRaise : public UnarySyntax
{
    define_syntax_members(Raise, "raise");

    SyntaxRaise(const Token& token, unique_ptr<Syntax> right)
      : UnarySyntax(token, move(right)) {}
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
        targets(move(targets)),
        exprs(move(exprs)),
        suite(move(suite)),
        elseSuite(move(elseSuite))
    {}

    const unique_ptr<SyntaxTarget> targets;
    const unique_ptr<Syntax> exprs;
    const unique_ptr<SyntaxBlock> suite;
    const unique_ptr<SyntaxBlock> elseSuite;
};

struct NameListSyntax : public Syntax
{
    NameListSyntax(const Token& token, vector<Name> names)
        : Syntax(token), names(names) {}

    const vector<Name> names;
};

struct SyntaxGlobal : public NameListSyntax
{
    define_syntax_members(Global, "global");

    SyntaxGlobal(const Token& token, vector<Name> names)
        : NameListSyntax(token, names) {}
};

struct SyntaxNonLocal : public NameListSyntax
{
    define_syntax_members(NonLocal, "nonlocal");

    SyntaxNonLocal(const Token& token, vector<Name> names)
        : NameListSyntax(token, names) {}
};

define_unary_syntax(Yield, "yield");

struct ExceptInfo
{
    ExceptInfo(const Token& token,
               unique_ptr<Syntax> expr,
               unique_ptr<SyntaxTarget> as,
               unique_ptr<SyntaxBlock> suite)
      : token(token),
        expr(move(expr)),
        as(move(as)),
        suite(move(suite))
    {
        assert(!this->as || this->expr);
        assert(this->suite);
    }

    const Token token;
    const unique_ptr<Syntax> expr;
    const unique_ptr<SyntaxTarget> as;
    const unique_ptr<SyntaxBlock> suite;
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
        trySuite(move(trySuite)),
        excepts(move(excepts)),
        elseSuite(move(elseSuite)),
        finallySuite(move(finallySuite))
    {}

    const unique_ptr<SyntaxBlock> trySuite;
    const vector<unique_ptr<ExceptInfo>> excepts;
    const unique_ptr<SyntaxBlock> elseSuite;
    const unique_ptr<SyntaxBlock> finallySuite;
};

struct SyntaxDel : public Syntax
{
    define_syntax_members(Del, "del");

    SyntaxDel(const Token& token, unique_ptr<SyntaxTarget> targets)
      : Syntax(token), targets(move(targets))
    {}

    const unique_ptr<SyntaxTarget> targets;
};

#undef define_syntax_type
#undef define_syntax_name
#undef define_syntax_accept
#undef define_syntax_members
#undef define_binary_syntax
#undef define_simple_binary_syntax
#undef define_unary_syntax

#endif
