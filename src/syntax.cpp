#include "syntax.h"

struct SyntaxPrinter : public SyntaxVisitor
{
    SyntaxPrinter(ostream& os) : os_(os) {}

#define define_visit_method(name)                                             \
    void visit(const Syntax##name&) override;

    for_each_syntax(define_visit_method)
#undef define_visit_method

    void setPos(const TokenPos& pos) override {}

    void visitNullary(const Syntax& s);
    void visitUnary(const UnarySyntax& s);

    template <typename BaseT, typename LeftT, typename RightT>
    void visitBinary(const BinarySyntax<BaseT, LeftT, RightT>& s);

    void visitNameList(const NameListSyntax& s);

    template <typename T>
    void printList(const vector<T>& elems, string seperator = ", ");

  private:
    ostream& os_;
};

template <typename T>
void SyntaxPrinter::printList(const vector<T>& elems, string seperator)
{
    bool first = true;
    for (const auto& i : elems) {
        if (!first)
            os_ << seperator;
        os_ << *i;
        first = false;
    }
}

void SyntaxPrinter::visitNullary(const Syntax& s)
{
    os_ << s.name();
}

void SyntaxPrinter::visitUnary(const UnarySyntax& s)
{
    os_ << s.name() << " " << *s.right();
}

template <typename BaseT, typename LeftT, typename RightT>
void SyntaxPrinter::visitBinary(const BinarySyntax<BaseT, LeftT, RightT>& s)
{
    os_ << *s.left() << " " << s.name() << " " << *s.right();
}

void SyntaxPrinter::visitNameList(const NameListSyntax& s)
{
    os_ << s.name() << " ";
    bool first = true;
    for (const auto& n : s.names()) {
        if (!first)
            os_ << ", ";
        os_ << n;
    }
}

void SyntaxPrinter::visit(const SyntaxBlock& s)
{
    printList(s.stmts(), "\n");
}

void SyntaxPrinter::visit(const SyntaxInteger& s)
{
    os_ << dec << s.value();
}

void SyntaxPrinter::visit(const SyntaxString& s)
{
    os_ << "'" << s.value() << "'";
}

void SyntaxPrinter::visit(const SyntaxName& s)
{
    os_ << s.id();
}

void SyntaxPrinter::visit(const SyntaxExprList& s)
{
    os_ << "(";
    printList(s.elems());
    if (s.elems().size() == 1)
        os_ << ",";
    os_ << ")";
}

void SyntaxPrinter::visit(const SyntaxList& s)
{
    os_ << "[";
    printList(s.elems());
    os_ << "]";
}

void SyntaxPrinter::visit(const SyntaxDict& s)
{
    os_ << "{";
    bool first = true;
    for (const auto& i : s.entries()) {
        if (!first)
            os_ << ", ";
        os_ << *i.first << ": " << *i.second;
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

void SyntaxPrinter::visit(const SyntaxGlobal& s) { visitNameList(s); }
void SyntaxPrinter::visit(const SyntaxNonLocal& s) { visitNameList(s); }


void SyntaxPrinter::visit(const SyntaxAttrRef& s)
{
    if (s.left()->is<SyntaxName>() ||
        s.left()->is<SyntaxExprList>() ||
        s.left()->is<SyntaxAttrRef>())
    {
        os_ << *s.left();
    } else {
        os_ << "(" << *s.left() << ")";
    }
    os_ << "." << *s.right();
}

void SyntaxPrinter::visit(const SyntaxTargetList& s)
{
    os_ << "(";
    printList(s.targets());
    os_ << ")";
}

void SyntaxPrinter::visit(const SyntaxSubscript& s)
{
    os_ << *s.left() << "[" << *s.right() << "]";
}

void SyntaxPrinter::visit(const SyntaxCall& s)
{
    os_ << *s.left() << "(";
    printList(s.right());
    os_ << ")";
}

void SyntaxPrinter::visit(const SyntaxCond& s)
{
    os_ << *s.cons() << " if " << *s.cond() << " else " << *s.alt();
}

void SyntaxPrinter::visit(const SyntaxLambda& s)
{
    os_ << "lambda";
    for (const auto& i : s.params()) {
        if (i.takesRest)
            os_ << "*";
        os_ << " " << i.name;
        if (i.maybeDefault)
            os_ << " = " << *i.maybeDefault;
    }
    os_ << ": " << *s.expr();
}

void SyntaxPrinter::visit(const SyntaxDef& s)
{
    os_ << "def " << s.id() << "(";
    bool first = true;
    for (const auto& i : s.params()) {
        if (!first)
            os_ << ", ";
        if (i.takesRest)
            os_ << "*";
        os_ << i.name;
        if (i.maybeDefault)
            os_ << " = " << *i.maybeDefault;
        first = false;
    }
    os_ << "):" << endl;
    os_ << *s.expr();
}

void SyntaxPrinter::visit(const SyntaxIf& s)
{
    // todo: indentation
    for (unsigned i = 0; i < s.branches().size(); ++i) {
        os_ << (i == 0 ? "if" : "elif") << " ";
        os_ << s.branches()[i].cond.get() << ":" << endl;
        os_ << s.branches()[i].block.get() << endl;
    }
    if (s.elseBranch()) {
        os_ << "else:" << endl;
        os_ << *s.elseBranch() << endl;
    }
}

void SyntaxPrinter::visit(const SyntaxWhile& s)
{
    // todo: indentation
    os_ << "while " << *s.cond() << ": " << endl;
    os_ << *s.suite() << endl;
    if (s.elseSuite())
        os_ << *s.elseSuite() << endl;
}

void SyntaxPrinter::visit(const SyntaxAssert& s)
{
    os_ << "assert " << s.cond();
    if (s.message())
        os_ << ", " << s.message();
}

void SyntaxPrinter::visit(const SyntaxClass& s)
{
    // todo: indentation
    os_ << "class " << s.id();
    if (!s.bases()->elems().empty())
        os_ << s.bases() << " ";
    os_ << ":" << endl << s.suite() << endl;
}

void SyntaxPrinter::visit(const SyntaxFor& s)
{
    // todo: indentation
    os_ << "for " << *s.targets() << " in " << *s.exprs() << ":" << endl;
    os_ << *s.suite();
    if (s.elseSuite()) {
        os_ << endl << "else: " << endl;
        os_ << *s.elseSuite();
    }
}

void SyntaxPrinter::visit(const SyntaxTry& s)
{
    // todo: indentation
    os_ << "try:" << endl;
    os_ << *s.trySuite();
    for (const auto& except : s.excepts()) {
        os_ << "except " << *except->expr();
        if (except->as())
            os_ << " as " << *except->as();
        os_ << ":" << endl;
        os_ << *except->suite();
    }
    if (s.finallySuite()) {
        os_ << endl << "finally: " << endl;
        os_ << *s.finallySuite();
    }
}

void SyntaxPrinter::visit(const SyntaxDel& s)
{
    os_ << s.name() << " " << s.targets();
}

ostream& operator<<(ostream& s, const Syntax& syntax) {
    SyntaxPrinter printer(s);
    syntax.accept(printer);
    return s;
}
