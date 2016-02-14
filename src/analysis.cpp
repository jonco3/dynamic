#include "analysis.h"
#include "numeric.h"
#include "object.h"
#include "parser.h"
#include "utils.h"

// Find the names that are defined in the current block
struct DefinitionFinder : public DefaultSyntaxVisitor
{
    DefinitionFinder(Traced<Layout*> layout, vector<Name>& globalsOut)
      : inAssignTarget_(false),
        globals_(globalsOut),
        hasNestedFuctions_(false)
    {
        defs_ = gc.create<Object>(Object::ObjectClass, layout);
    }

    ~DefinitionFinder() {
        assert(!inAssignTarget_);
    }

    bool hasNestedFuctions() { return hasNestedFuctions_; }
    Object* definitions() { return defs_; }

    void addName(Name name) {
        if (contains(globals_, name) || contains(nonLocals_, name))
            return;

        if (defs_->hasOwnAttr(name))
            return;

        defs_->setAttr(name, Boolean::False);
    }

    // Record assignments and definitions

    virtual void visit(const SyntaxAssign& s) {
        {
            assert(!inAssignTarget_);
            AutoSetAndRestore setInAssign(inAssignTarget_, true);
            s.left->accept(*this);
        }
        s.right->accept(*this);
    }

    virtual void visit(const SyntaxTargetList& s) {
        assert(inAssignTarget_);
        for (const auto& i : s.targets)
            i->accept(*this);
    }

    virtual void visit(const SyntaxName& s) {
        if (inAssignTarget_)
            addName(s.id);
    }

    virtual void visit(const SyntaxDef& s) {
        addName(s.id);
        hasNestedFuctions_ = true;
    }

    virtual void visit(const SyntaxLambda& s) {
        hasNestedFuctions_ = true;
    }

    virtual void visit(const SyntaxClass& s) {
        addName(s.id);
    }

    virtual void visit(const SyntaxImport& s) {
        for (const auto& i : s.modules) {
            addName(i->localName);
        }
    }

    virtual void visit(const SyntaxFrom& s) {
        for (const auto& i : s.ids) {
            addName(i->localName);
            // todo: what about import *?
        }
    }

    void addGlobalOrNonLocalNames(const Token& token,
                                  const vector<Name>& names,
                                  vector<Name>& dest,
                                  const vector<Name>& others)
    {
        for (auto name : names) {
            // todo: this should only be a warning
            if (defs_->hasAttr(name))
                throw ParseError(token, "name assigned to before declaration");
            if (contains(others, name))
                throw ParseError(token, "name is nonlocal and global");
            dest.push_back(name);
        }
    }

    virtual void visit(const SyntaxGlobal& s) {
        addGlobalOrNonLocalNames(s.token, s.names, globals_, nonLocals_);
    }

    virtual void visit(const SyntaxNonLocal& s) {
        addGlobalOrNonLocalNames(s.token, s.names, nonLocals_, globals_);
    }

    // Recurse into expressions

    virtual void visit(const SyntaxBlock& s) {
        for (const auto& i : s.statements)
            i->accept(*this);
    }

    virtual void visit(const SyntaxExprList& s) {
        for (const auto& i : s.elements)
            i->accept(*this);
    }

    virtual void visit(const SyntaxList& s) {
        for (const auto& i : s.elements)
            i->accept(*this);
    }

    virtual void visit(const SyntaxDict& s) {
        for (const auto& i : s.entries) {
            i.first->accept(*this);
            i.second->accept(*this);
        }
    }

    virtual void visit(const SyntaxOr& s) {
        s.left->accept(*this);
        s.right->accept(*this);
    }

    virtual void visit(const SyntaxAnd& s) {
        s.left->accept(*this);
        s.right->accept(*this);
    }

    virtual void visit(const SyntaxNot& s) {
        s.right->accept(*this);
    }

    virtual void visit(const SyntaxPos& s) {
        s.right->accept(*this);
    }

    virtual void visit(const SyntaxNeg& s) {
        s.right->accept(*this);
    }

    virtual void visit(const SyntaxInvert& s) {
        s.right->accept(*this);
    }

    virtual void visit(const SyntaxCompareOp& s) {
        s.left->accept(*this);
        s.right->accept(*this);
    }

    virtual void visit(const SyntaxBinaryOp& s) {
        s.left->accept(*this);
        s.right->accept(*this);
    }

    virtual void visit(const SyntaxCall& s) {
        s.left->accept(*this);
        for (const auto& i : s.right)
            i->accept(*this);
    }

    virtual void visit(const SyntaxCond& s) {
        s.cons->accept(*this);
        s.alt->accept(*this);
    }

    virtual void visit(const SyntaxIf& s) {
        for (const auto& i : s.branches)
            i.suite->accept(*this);
        if (s.elseSuite)
            s.elseSuite->accept(*this);
    }

    virtual void visit(const SyntaxWhile& s) {
        s.suite->accept(*this);
        if (s.elseSuite)
            s.elseSuite->accept(*this);
    }

    virtual void visit(const SyntaxFor& s) {
        {
            assert(!inAssignTarget_);
            AutoSetAndRestore setInAssign(inAssignTarget_, true);
            s.targets->accept(*this);
        }
        s.suite->accept(*this);
        if (s.elseSuite)
            s.elseSuite->accept(*this);
    }

    virtual void visit(const SyntaxTry& s) {
        assert(!inAssignTarget_);
        s.trySuite->accept(*this);
        for (const auto& except : s.excepts) {
            if (except->as) {
                AutoSetAndRestore setInAssign(inAssignTarget_, true);
                except->as->accept(*this);
            }
            except->suite->accept(*this);
        }
        if (s.elseSuite)
            s.elseSuite->accept(*this);
        if (s.finallySuite)
            s.finallySuite->accept(*this);
    }

    virtual void visit(const SyntaxListComp& s) {
        hasNestedFuctions_ = true;
    }

    virtual void visit(const SyntaxCompIterand& s) {
        s.expr->accept(*this);
    }

  private:
    bool inAssignTarget_;
    Root<Object*> defs_;
    vector<Name>& globals_;
    vector<Name> nonLocals_;
    bool hasNestedFuctions_;
};

Object* FindDefinitions(const Syntax& s,
                        Traced<Layout*> layout,
                        vector<Name>& globalsOut,
                        bool& hasNestedFuctionsOut)
{
    DefinitionFinder finder(layout, globalsOut);
    s.accept(finder);
    hasNestedFuctionsOut = finder.hasNestedFuctions();
    return finder.definitions();
}
