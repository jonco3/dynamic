#include "syntax.h"

struct SyntaxPrinter : public SyntaxVisitor
{
    SyntaxPrinter(ostream& os) : os_(os) {}

    void print(const Syntax& syntax);

#define define_visit_method(name)                                             \
    void visit(const Syntax##name&) override;

    for_each_syntax(define_visit_method)
#undef define_visit_method

    void setPos(const TokenPos& pos) override {}

    void visitNullary(const Syntax& s);
    void visitUnary(const UnarySyntax& s);

    template <typename BaseT, typename LeftT, typename RightT>
    void visitBinary(const BinarySyntax<BaseT, LeftT, RightT>& s);

    template <typename T>
    void printList(const vector<T>& elems, string seperator = ", ");

    template <typename T>
    bool printArgList(bool first, const vector<T>& elems, string prepend = "");

  private:
    ostream& os_;

    void print(const ArgInfo& arg);
    void print(const KeywordArgInfo& syntax);
    void print(const InternedString& name);
    void printListCompTail(const Syntax& expr);
};

ostream& operator<<(ostream& s, const Syntax& syntax) {
    SyntaxPrinter(s).print(syntax);
    return s;
}

void SyntaxPrinter::print(const Syntax& syntax)
{
    syntax.accept(*this);
}

void SyntaxPrinter::print(const InternedString& name)
{
    os_ << &name;
}

void SyntaxPrinter::print(const ArgInfo& arg)
{
    if (arg.isUnpacked)
        os_ << "*";
    print(*arg.arg);
}

void SyntaxPrinter::print(const KeywordArgInfo& keywordArg)
{
    print(*keywordArg.keyword);
    os_ << " = ";
    print(*keywordArg.arg);
}

template <typename T>
void SyntaxPrinter::printList(const vector<T>& elems, string seperator)
{
    bool first = true;
    for (const auto& i : elems) {
        if (!first)
            os_ << seperator;
        print(*i);
        first = false;
    }
}

template <typename T>
bool SyntaxPrinter::printArgList(bool first, const vector<T>& elems,
                                 string prepend)
{
    for (const auto& i : elems) {
        if (!first)
            os_ << ", ";
        os_ << prepend;
        print(*i);
        first = false;
    }
    return first;
}

void SyntaxPrinter::visitNullary(const Syntax& s)
{
    os_ << s.name();
}

void SyntaxPrinter::visitUnary(const UnarySyntax& s)
{
    os_ << s.name() << " ";
    print(*s.right);
}

template <typename BaseT, typename LeftT, typename RightT>
void SyntaxPrinter::visitBinary(const BinarySyntax<BaseT, LeftT, RightT>& s)
{
    print(*s.left);
    os_ << " " << s.name() << " ";
    print(*s.right);
}

void SyntaxPrinter::visit(const SyntaxBlock& s)
{
    printList(s.statements, "\n");
}

void SyntaxPrinter::visit(const SyntaxInteger& s)
{
    os_ << dec << s.token.text;
}

void SyntaxPrinter::visit(const SyntaxFloat& s)
{
    os_ << s.value;
}

void SyntaxPrinter::visit(const SyntaxString& s)
{
    os_ << "'" << s.value << "'";
}

void SyntaxPrinter::visit(const SyntaxName& s)
{
    os_ << s.id;
}

void SyntaxPrinter::visit(const SyntaxExprList& s)
{
    os_ << "(";
    printList(s.elements);
    if (s.elements.size() == 1)
        os_ << ",";
    os_ << ")";
}

void SyntaxPrinter::visit(const SyntaxList& s)
{
    os_ << "[";
    printList(s.elements);
    os_ << "]";
}

void SyntaxPrinter::visit(const SyntaxDict& s)
{
    os_ << "{";
    bool first = true;
    for (const auto& i : s.entries) {
        if (!first)
            os_ << ", ";
        print(*i.first);
        os_ << ": ";
        print(*i.second);
        first = false;
    }
    os_ << "}";
}

void SyntaxPrinter::visit(const SyntaxPass& s) { visitNullary(s); }
void SyntaxPrinter::visit(const SyntaxBreak& s) { visitNullary(s); }
void SyntaxPrinter::visit(const SyntaxContinue& s) { visitNullary(s); }

void SyntaxPrinter::visit(const SyntaxNeg& s) { visitUnary(s); }
void SyntaxPrinter::visit(const SyntaxPos& s) { visitUnary(s); }
void SyntaxPrinter::visit(const SyntaxInvert& s) { visitUnary(s); }
void SyntaxPrinter::visit(const SyntaxNot& s) { visitUnary(s); }
void SyntaxPrinter::visit(const SyntaxRaise& s) { visitUnary(s); }
void SyntaxPrinter::visit(const SyntaxYield& s) { visitUnary(s); }
void SyntaxPrinter::visit(const SyntaxReturn& s) { visitUnary(s); }

void SyntaxPrinter::visit(const SyntaxOr& s) { visitBinary(s); }
void SyntaxPrinter::visit(const SyntaxAnd& s) { visitBinary(s); }
void SyntaxPrinter::visit(const SyntaxIn& s) { visitBinary(s); }
void SyntaxPrinter::visit(const SyntaxIs& s) { visitBinary(s); }
void SyntaxPrinter::visit(const SyntaxAssign& s) { visitBinary(s); }
void SyntaxPrinter::visit(const SyntaxBinaryOp& s) { visitBinary(s); }
void SyntaxPrinter::visit(const SyntaxAugAssign& s) { visitBinary(s); }
void SyntaxPrinter::visit(const SyntaxCompareOp& s) { visitBinary(s); }

void SyntaxPrinter::visit(const SyntaxGlobal& s) { printList(s.names); }
void SyntaxPrinter::visit(const SyntaxNonLocal& s) { printList(s.names); }

void SyntaxPrinter::visit(const SyntaxAttrRef& s)
{
    if (s.left->is<SyntaxName>() ||
        s.left->is<SyntaxExprList>() ||
        s.left->is<SyntaxAttrRef>())
    {
        print(*s.left);
    } else {
        os_ << "(";
        print(*s.left);
        os_ << ")";
    }
    os_ << ".";
    print(*s.right);
}

void SyntaxPrinter::visit(const SyntaxTargetList& s)
{
    os_ << "(";
    printList(s.targets);
    os_ << ")";
}

void SyntaxPrinter::visit(const SyntaxSubscript& s)
{
    print(*s.left);
    os_ << "[";
    print(*s.right);
    os_ << "]";
}

void SyntaxPrinter::visit(const SyntaxSlice& s)
{
    print(*s.lower);
    os_ << ":";
    print(*s.upper);
    if (s.stride) {
        os_ << ":";
        print(*s.stride);
    }
}

void SyntaxPrinter::visit(const SyntaxCall& s)
{
    print(*s.target);
    os_ << "(";
    bool first = true;
    first = printArgList(first, s.positionalArgs);
    first = printArgList(first, s.keywordArgs);
    if (s.mappingArg) {
        if (!first)
            os_ << ", **";
        print(*s.mappingArg);
    }
    os_ << ")";
}

void SyntaxPrinter::visit(const SyntaxCond& s)
{
    print(*s.cons);
    os_ << " if ";
    print(*s.cond);
    os_ << " else ";
    print(*s.alt);
}

void SyntaxPrinter::visit(const SyntaxLambda& s)
{
    os_ << "lambda";
    for (const auto& i : s.params) {
        if (i.takesRest)
            os_ << "*";
        os_ << " " << i.name;
        if (i.maybeDefault) {
            os_ << " = ";
            print(*i.maybeDefault);
        }
    }
    os_ << ": ";
    print(*s.expr);
}

void SyntaxPrinter::visit(const SyntaxDef& s)
{
    os_ << "def " << s.id << "(";
    bool first = true;
    for (const auto& i : s.params) {
        if (!first)
            os_ << ", ";
        if (i.takesRest)
            os_ << "*";
        os_ << i.name;
        if (i.maybeDefault) {
            os_ << " = ";
            print(*i.maybeDefault);
        }
        first = false;
    }
    os_ << "):" << endl;
    print(*s.suite);
}

void SyntaxPrinter::visit(const SyntaxIf& s)
{
    // todo: indentation
    for (unsigned i = 0; i < s.branches.size(); ++i) {
        os_ << (i == 0 ? "if" : "elif") << " ";
        print(*s.branches[i].cond);
        os_ << ":" << endl;
        print(*s.branches[i].suite);
        os_ << endl;
    }
    if (s.elseSuite) {
        os_ << "else:" << endl;
        print(*s.elseSuite);
        os_ << endl;
    }
}

void SyntaxPrinter::visit(const SyntaxWhile& s)
{
    // todo: indentation
    os_ << "while ";
    print(*s.cond);
    os_ << ": " << endl;
    print(*s.suite);
    os_ << endl;
    if (s.elseSuite) {
        print(*s.elseSuite);
        os_ << endl;
    }
}

void SyntaxPrinter::visit(const SyntaxAssert& s)
{
    os_ << "assert ";
    print(*s.cond);
    if (s.message) {
        os_ << ", ";
        print(*s.message);
    }
}

void SyntaxPrinter::visit(const SyntaxClass& s)
{
    // todo: indentation
    os_ << "class " << s.id;
    if (!s.bases->elements.empty()) {
        print(*s.bases);
        os_ << " ";
    }
    os_ << ":" << endl;
    print(*s.suite);
    os_ << endl;
}

void SyntaxPrinter::visit(const SyntaxFor& s)
{
    // todo: indentation
    os_ << "for ";
    print(*s.targets);
    os_ << " in ";
    print(*s.exprs);
    os_ << ":" << endl;
    print(*s.suite);
    if (s.elseSuite) {
        os_ << endl << "else: " << endl;
        print(*s.elseSuite);
    }
}

void SyntaxPrinter::visit(const SyntaxTry& s)
{
    // todo: indentation
    os_ << "try:" << endl;
    print(*s.trySuite);
    for (const auto& except : s.excepts) {
        os_ << "except ";
        print(*except->expr);
        if (except->as) {
            os_ << " as ";
            print(*except->as);
        }
        os_ << ":" << endl;
        print(*except->suite);
    }
    if (s.finallySuite) {
        os_ << endl << "finally: " << endl;
        print(*s.finallySuite);
    }
}

void SyntaxPrinter::visit(const SyntaxDel& s)
{
    os_ << s.name() << " ";
    print(*s.targets);
}

void SyntaxPrinter::visit(const SyntaxImport& s)
{
    bool first = true;
    os_ << s.name() << " ";
    for (const auto& i : s.modules) {
        if (!first)
            os_ << ", ";
        os_ << i->name;
        if (i->name != i->localName)
            os_ << " as " << i->localName;
        first = false;
    }
    os_ << endl;
}

void SyntaxPrinter::visit(const SyntaxFrom& s)
{
    bool first = true;
    for (unsigned i = 0; i < s.level; i++)
        os_ << ".";
    os_ << s.name() << " " << s.module << " import ";
    for (const auto& i : s.ids) {
        if (!first)
            os_ << ", ";
        os_ << i->name;
        if (i->name != i->localName)
            os_ << " as " << i->localName;
        first = false;
    }
    os_ << endl;
}

static const SyntaxCompIterand& GetListCompIterand(const Syntax& expr)
{
    if (expr.is<SyntaxFor>())
        return GetListCompIterand(*expr.as<SyntaxFor>()->suite);

    if (expr.is<SyntaxIf>())
        return GetListCompIterand(*expr.as<SyntaxIf>()->branches.at(0).suite);

    assert(expr.is<SyntaxCompIterand>());
    return *expr.as<SyntaxCompIterand>();
}

void SyntaxPrinter::printListCompTail(const Syntax& expr)
{
    if (expr.is<SyntaxFor>()) {
        const SyntaxFor& s = *expr.as<SyntaxFor>();
        os_ << " for ";
        print(*s.targets);
        os_ << " in ";
        print(*s.exprs);
        printListCompTail(*s.suite);
        return;
    }

    if (expr.is<SyntaxIf>()) {
        const SyntaxIf& s = *expr.as<SyntaxIf>();
        os_ << " if ";
        print(*s.branches.at(0).cond);
        printListCompTail(*s.branches.at(0).suite);
        return;
    }

    assert(expr.is<SyntaxCompIterand>());
}

void SyntaxPrinter::visit(const SyntaxListComp& s)
{
    os_ << "[ ";
    print(GetListCompIterand(*s.expr));
    printListCompTail(*s.expr);
    os_ << " ]";
}

void SyntaxPrinter::visit(const SyntaxCompIterand& s)
{
    print(*s.expr);
}
