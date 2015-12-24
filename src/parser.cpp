#include "parser.h"

#include "generator.h"
#include "syntax.h"
#include "test.h"
#include "utils.h"

#include "value-inl.h"

#include <sstream>

ParseError::ParseError(const Token& token, string message) :
  runtime_error(message), pos(token.pos)
{
    ostringstream s;
    s << "ParseError: " << message << " at " << token.pos;
    maybeAbortTests(s.str());
}

/*
 * Match a comma separated list of expressions up to an end token.
 */
template <typename T>
vector<T> Parser<T>::exprList(TokenType separator, TokenType end)
{
    vector<T> exprs;
    if (opt(end))
        return exprs;

    for (;;) {
        exprs.push_back(expression());
        if (opt(end))
            break;
        match(separator);
    }
    return exprs;
}

/*
 * Match a comma separated list of expressions up to an end token, allowing a
 * trailing comma.
 */
template <typename T>
vector<T> Parser<T>::exprListTrailing(TokenType separator,
                                      TokenType end)
{
    vector<T> exprs;
    while (!opt(end)) {
        exprs.push_back(expression());
        if (opt(end))
            break;
        match(separator);
    }
    return exprs;
}

SyntaxParser::SyntaxParser()
  : Parser(tokenizer), inFunction(false), isGenerator(false)
{
    // Atoms

    createNodeForAtom<SyntaxName>(Token_Identifier);
    createNodeForAtom<SyntaxString>(Token_String);
    createNodeForAtom<SyntaxInteger>(Token_Integer);
    createNodeForAtom<SyntaxFloat>(Token_Float);
    // todo: longinteger imagnumber

    // Unary operations

    // todo: check the precedence of these works as expected
    createNodeForUnary<SyntaxPos>(Token_Add);
    createNodeForUnary<SyntaxNeg>(Token_Sub);
    createNodeForUnary<SyntaxNot>(Token_Not, 0);
    createNodeForUnary<SyntaxInvert>(Token_BitNot);

    // Displays

    addPrefixHandler(Token_Bra, [=] (ParserT& parser, Token token) -> unique_ptr<Syntax> {
        if (parser.opt(Token_Ket))
            return make_unique<SyntaxExprList>(token);
        unique_ptr<Syntax> result = parseExprOrExprList();
        parser.match(Token_Ket);
        return result;
    });

    addPrefixHandler(Token_SBra, [=] (ParserT& parser, Token token) -> unique_ptr<Syntax> {
        return parseListDisplay(token);
    });

    addPrefixHandler(Token_CBra, [=] (ParserT& parser, Token token) -> unique_ptr<Syntax> {
        vector<SyntaxDict::Entry> entries;
        while (!opt(Token_CKet)) {
            unique_ptr<Syntax> key = parseExpr();
            match(Token_Colon);
            unique_ptr<Syntax> value = parseExpr();
            entries.emplace_back(move(key), move(value));
            if (opt(Token_CKet))
                break;
            match(Token_Comma);
        }
        return make_unique<SyntaxDict>(token, move(entries));
    });

    // Lambda

    addPrefixHandler(Token_Lambda, [=] (ParserT& parser, Token token) {
        vector<Parameter> params;
        if (!opt(Token_Colon)) {
            // todo: may need to allow bracketed parameters here
            SyntaxParser& sp = static_cast<SyntaxParser&>(parser);
            params = sp.parseParameterList(Token_Colon);
        }
        unique_ptr<Syntax> expr(parseExpr());
        return make_unique<SyntaxLambda>(token, move(params), move(expr));
    });

    // Boolean operators

    createNodeForBinary<SyntaxOr>(Token_Or, 100, Assoc_Left);
    createNodeForBinary<SyntaxAnd>(Token_And, 100, Assoc_Left);

    // Comparison operators

    createNodeForBinary<SyntaxIn>(Token_In, 120, Assoc_Left);
    createNodeForBinary<SyntaxIs>(Token_Is, 120, Assoc_Left);
    createNodeForBinary<SyntaxCompareOp>(Token_LT, 120, Assoc_Left, CompareLT);
    createNodeForBinary<SyntaxCompareOp>(Token_LE, 120, Assoc_Left, CompareLE);
    createNodeForBinary<SyntaxCompareOp>(Token_GT, 120, Assoc_Left, CompareGT);
    createNodeForBinary<SyntaxCompareOp>(Token_GE, 120, Assoc_Left, CompareGE);
    createNodeForBinary<SyntaxCompareOp>(Token_EQ, 120, Assoc_Left, CompareEQ);
    createNodeForBinary<SyntaxCompareOp>(Token_NE, 120, Assoc_Left, CompareNE);
    addBinaryOp(Token_NotIn, 120, Assoc_Left, [] (Token token, unique_ptr<Syntax> l, unique_ptr<Syntax> r) {
        return make_unique<SyntaxNot>(token, make_unique<SyntaxIn>(token, move(l), move(r)));
    });
    addBinaryOp(Token_IsNot, 120, Assoc_Left, [] (Token token, unique_ptr<Syntax> l, unique_ptr<Syntax> r) {
        return make_unique<SyntaxNot>(token, make_unique<SyntaxIs>(token, move(l), move(r)));
    });

    // Bitwise binary operators

    createNodeForBinary<SyntaxBinaryOp>(Token_BitOr, 130, Assoc_Left, BinaryOr);
    createNodeForBinary<SyntaxBinaryOp>(Token_BitXor, 140, Assoc_Left, BinaryXor);
    createNodeForBinary<SyntaxBinaryOp>(Token_BitAnd, 150, Assoc_Left, BinaryAnd);
    createNodeForBinary<SyntaxBinaryOp>(Token_BitLeftShift, 160, Assoc_Left,
                                        BinaryLeftShift);
    createNodeForBinary<SyntaxBinaryOp>(Token_BitRightShift, 160, Assoc_Left,
                                        BinaryRightShift);

    // Arithermetic binary operators

    createNodeForBinary<SyntaxBinaryOp>(Token_Add, 170, Assoc_Left, BinaryAdd);
    createNodeForBinary<SyntaxBinaryOp>(Token_Sub, 170, Assoc_Left, BinarySub);
    createNodeForBinary<SyntaxBinaryOp>(Token_Times, 180, Assoc_Left, BinaryMul);
    createNodeForBinary<SyntaxBinaryOp>(Token_TrueDiv, 180, Assoc_Left, BinaryTrueDiv);
    createNodeForBinary<SyntaxBinaryOp>(Token_FloorDiv, 180, Assoc_Left, BinaryFloorDiv);
    createNodeForBinary<SyntaxBinaryOp>(Token_Modulo, 180, Assoc_Left, BinaryModulo);
    createNodeForBinary<SyntaxBinaryOp>(Token_Power, 180, Assoc_Left, BinaryPower);

    addInfixHandler(Token_SBra, 200, [=] (ParserT& parser, Token token, unique_ptr<Syntax> l) {
        unique_ptr<Syntax> expr;
        if (opt(Token_Colon)) {
            expr = parseSlice(token, move(expr));
        } else {
            expr = parseExpr();
            if (opt(Token_Colon))
                expr = parseSlice(token, move(expr));
        }
        match(Token_SKet);
        return make_unique<SyntaxSubscript>(token, move(l), move(expr));
    });

    addInfixHandler(Token_Bra, 200, [=] (ParserT& parser, Token token, unique_ptr<Syntax> l) {
        vector<unique_ptr<Syntax>> args;
        while (!opt(Token_Ket)) {
            if (!args.empty())
                match(Token_Comma);
            args.emplace_back(parseExpr());
        }
        return make_unique<SyntaxCall>(token, move(l), move(args));
    });

    addBinaryOp(Token_Period, 200, Assoc_Left, [] (Token token, unique_ptr<Syntax> l, unique_ptr<Syntax> r) {
        if (!r->is<SyntaxName>())
            throw ParseError(token, "Bad attribute reference");
        unique_ptr<SyntaxName> name(r.release()->as<SyntaxName>());
        return make_unique<SyntaxAttrRef>(token, move(l), move(name));
    });

    addPrefixHandler(Token_Return, [=] (ParserT& parser, Token token) {
        unique_ptr<Syntax> expr;
        if (parser.notFollowedBy(Token_EOF) &&
            parser.notFollowedBy(Token_Newline) &&
            parser.notFollowedBy(Token_Dedent))
        {
            expr = parseExprOrExprList();
        }
        return make_unique<SyntaxReturn>(token, move(expr));
    });
}

unique_ptr<Syntax> SyntaxParser::parseListDisplay(Token token)
{
    if (opt(Token_SKet))
        return make_unique<SyntaxList>(token);

    unique_ptr<Syntax> firstExpr = parseExpr();

    Token t2 = currentToken();
    if (opt(Token_For))
        return parseListComprehension(t2, move(firstExpr));

    vector<unique_ptr<Syntax>> exprs;
    exprs.push_back(move(firstExpr));
    if (opt(Token_SKet))
        return make_unique<SyntaxList>(token, move(exprs));

    while (!opt(Token_SKet)) {
        match(Token_Comma);
        if (opt(Token_SKet))
            break;
        exprs.push_back(parseExpr());
    }
    return make_unique<SyntaxList>(token, move(exprs));
}

unique_ptr<Syntax>
SyntaxParser::parseCompFor(Token token, unique_ptr<Syntax> iterand)
{
    unique_ptr<SyntaxTarget> targets = parseTargetList();
    match(Token_In);
    unique_ptr<Syntax> expr = expression();
    unique_ptr<Syntax> next = parseNextCompExpr(move(iterand));
    return make_unique<SyntaxFor>(token, move(targets), move(expr), move(next));
}

unique_ptr<Syntax>
SyntaxParser::parseCompIf(Token token, unique_ptr<Syntax> iterand)
{
    unique_ptr<Syntax> expr = expression();
    unique_ptr<Syntax> next = parseNextCompExpr(move(iterand));
    vector<IfBranch> branches;
    branches.push_back({move(expr), move(next)});
    return make_unique<SyntaxIf>(token, move(branches));
}

unique_ptr<Syntax>
SyntaxParser::parseNextCompExpr(unique_ptr<Syntax> iterand)
{
    Token token;
    if (opt(Token_For, token))
        return parseCompFor(token, move(iterand));

    if (opt(Token_If, token))
        return parseCompIf(token, move(iterand));

    return iterand;
}

unique_ptr<Syntax>
SyntaxParser::parseListComprehension(Token token,
                                     unique_ptr<Syntax> initalExpr)
{
    unique_ptr<SyntaxCompIterand> iterand =
        make_unique<SyntaxCompIterand>(token, move(initalExpr));
    unique_ptr<Syntax> expr = parseCompFor(token, move(iterand));
    match(Token_SKet);
    return make_unique<SyntaxListComp>(token, move(expr));
}

unique_ptr<Syntax> SyntaxParser::parseSlice(Token token,
                                            unique_ptr<Syntax> lower)
{
    // Called to parse a slice after parsing an optional expression followed by
    // a colon
    // todo: slicing is more complicated than this.
    unique_ptr<Syntax> upper;
    unique_ptr<Syntax> stride;
    if (notFollowedBy(Token_SKet)) {
        if (notFollowedBy(Token_Colon))
            upper = parseExpr();
        if (opt(Token_Colon)) {
            if (notFollowedBy(Token_SKet)) {
                stride = parseExpr();
            }
        }
    }
    return make_unique<SyntaxSlice>(token,
                                    move(lower), move(upper), move(stride));
}

bool SyntaxParser::maybeExprToken()
{
    // Return whether the next token can be the start of an expression.
    TokenType t = currentToken().type;
    return t == Token_Identifier ||
        t == Token_Integer || t == Token_Float || t == Token_String ||
        t == Token_Bra || t == Token_SBra || t == Token_CBra ||
        t == Token_Sub || t == Token_Add || t == Token_BitNot ||
        t == Token_Not || t == Token_Lambda || t == Token_Yield ||
        t == Token_Backtick;
}

unique_ptr<Syntax> SyntaxParser::parseExpr()
{
    // Parse 'expression'
    unique_ptr<Syntax> expr = expression();

    Token token;
    if (opt(Token_If, token)) {
        unique_ptr<Syntax> cond = expression();
        match(Token_Else);
        unique_ptr<Syntax> alt = parseExpr();
        return make_unique<SyntaxCond>(token,
                                       move(expr), move(cond), move(alt));
    }

    return expr;
}

unique_ptr<Syntax> SyntaxParser::parseExprOrExprList()
{
    Token startToken = currentToken();
    vector<unique_ptr<Syntax>> exprs;
    exprs.emplace_back(parseExpr());
    bool singleExpr = true;
    while (opt(Token_Comma)) {
        singleExpr = false;
        if (!maybeExprToken())
            break;
        exprs.emplace_back(parseExpr());
    }
    if (singleExpr)
        return move(exprs[0]);
    return make_unique<SyntaxExprList>(startToken, move(exprs));
}

unique_ptr<SyntaxTarget> SyntaxParser::makeAssignTarget(unique_ptr<Syntax> target)
{
    if (target->is<SyntaxExprList>()) {
        unique_ptr<SyntaxExprList> exprs(target.release()->as<SyntaxExprList>());
        vector<unique_ptr<Syntax>>& elems = exprs->elements;
        vector<unique_ptr<SyntaxTarget>> targets;
        for (unsigned i = 0; i < elems.size(); i++)
            targets.push_back(makeAssignTarget(move(elems[i])));
        return make_unique<SyntaxTargetList>(exprs->token, move(targets));
    } else if (target->is<SyntaxList>()) {
        unique_ptr<SyntaxList> list(unique_ptr_as<SyntaxList>(target));
        vector<unique_ptr<Syntax>>& elems = list->elements;
        vector<unique_ptr<SyntaxTarget>> targets;
        for (unsigned i = 0; i < elems.size(); i++)
            targets.push_back(makeAssignTarget(move(elems[i])));
        return make_unique<SyntaxTargetList>(list->token, move(targets));
    } else if (target->is<SyntaxName>()) {
        return unique_ptr<SyntaxTarget>(unique_ptr_as<SyntaxName>(target));
    } else if (target->is<SyntaxAttrRef>()) {
        return unique_ptr_as<SyntaxAttrRef>(target);
    } else if (target->is<SyntaxSubscript>()) {
        return unique_ptr_as<SyntaxSubscript>(target);
    } else {
        throw ParseError(target->token,
                         "Illegal target for assignment: " + target->name());
    }
}

unique_ptr<SyntaxTarget> SyntaxParser::parseTarget()
{
    // Unfortunately we have two different paths for parsing assignment targets.
    // This is called when we know we have to match a target, but the main
    // expression patch can also match an expression which is later
    // checked/converted by makeAssignTarget() above.

    Token token = currentToken();
    unique_ptr<Syntax> syn;
    if (opt(Token_Bra) | opt(Token_SBra)) {
        // Might be an expression before period or it might be a target list
        syn = parseExprOrExprList();
        match(token.type == Token_Bra ? Token_Ket : Token_SKet);
    } else {
        match(Token_Identifier);
        syn = make_unique<SyntaxName>(token);
    }

    for (;;) {
        if (opt(Token_Period)) {
            Token attr = match(Token_Identifier);
            unique_ptr<SyntaxName> name(make_unique<SyntaxName>(move(attr)));
            syn = make_unique<SyntaxAttrRef>(token, move(syn), move(name));
        } else if (opt(Token_SBra)) {
            unique_ptr<Syntax> index(parseExpr());
            match(Token_SKet);
            syn = make_unique<SyntaxSubscript>(token, move(syn), move(index));
        } else {
            return makeAssignTarget(move(syn));
        }
    }
}

unique_ptr<SyntaxTarget> SyntaxParser::parseTargetList()
{
    Token startToken = currentToken();
    vector<unique_ptr<SyntaxTarget>> targets;
    targets.emplace_back(parseTarget());
    bool singleTarget = true;
    while (opt(Token_Comma)) {
        singleTarget = false;
        if (!maybeExprToken())
            break;
        targets.emplace_back(parseTarget());
    }
    if (singleTarget)
        return move(targets[0]);
    return make_unique<SyntaxTargetList>(startToken, move(targets));
}

unique_ptr<Syntax> SyntaxParser::parseAugAssign(Token token, unique_ptr<Syntax> syntax, BinaryOp op)
{
    unique_ptr<SyntaxSingleTarget> target;
    // todo: special case is/as for SyntaxSingleTarget
    if (syntax->is<SyntaxName>()) {
        target = unique_ptr_as<SyntaxName>(syntax);
    } else if (syntax->is<SyntaxAttrRef>()) {
        target = unique_ptr_as<SyntaxAttrRef>(syntax);
    } else if (syntax->is<SyntaxSubscript>()) {
        target = unique_ptr_as<SyntaxSubscript>(syntax);
    } else {
        throw ParseError(token,
                         "Illegal target for augmented assignment: " +
                         syntax->name());
    }

    unique_ptr<Syntax> expr(parseExpr());
    return make_unique<SyntaxAugAssign>(token, move(target), move(expr), op);
}

unique_ptr<Syntax> SyntaxParser::parseAssignSource()
{
    unique_ptr<Syntax> expr = parseExprOrExprList();
    if (!opt(Token_Assign))
        return expr;

    Token token = currentToken();
    unique_ptr<SyntaxTarget> target = makeAssignTarget(move(expr));
    expr = parseAssignSource();
    return make_unique<SyntaxAssign>(token, move(target), move(expr));
}

unique_ptr<ImportInfo> SyntaxParser::parseModuleImport()
{
    // todo: parse |(identifier ".")* identifier| here
    Token t = match(Token_Identifier);
    Name moduleName(internString(t.text));
    Name localName(moduleName);
    if (opt(Token_As)) {
        t = match(Token_Identifier);
        localName = internString(t.text);
    }
    return make_unique<ImportInfo>(moduleName, localName);
}

unique_ptr<ImportInfo> SyntaxParser::parseIdImport()
{
    Token t = match(Token_Identifier);
    Name idName(internString(t.text));
    Name localName(idName);
    if (opt(Token_As)) {
        t = match(Token_Identifier);
        localName = internString(t.text);
    }
    return make_unique<ImportInfo>(idName, localName);
}

unique_ptr<Syntax> SyntaxParser::parseSimpleStatement()
{
    Token token = currentToken();
    if (opt(Token_Assert)) {
        unique_ptr<Syntax> cond(parseExpr());
        unique_ptr<Syntax> message;
        if (opt(Token_Comma))
            message = parseExpr();
        return make_unique<SyntaxAssert>(token, move(cond), move(message));
    } else if (opt(Token_Pass)) {
        return make_unique<SyntaxPass>(token);
    } else if (opt(Token_Raise)) {
        unique_ptr<Syntax> expr(parseExpr());
        if (opt(Token_Comma))
            throw ParseError(token,
                             "Multiple exressions for raise not supported"); // todo
        return make_unique<SyntaxRaise>(token, move(expr));
    } else if (opt(Token_Global)) {
        vector<Name> names;
        do {
            Token t = match(Token_Identifier);
            names.push_back(internString(t.text));
        } while (opt(Token_Comma));
        return make_unique<SyntaxGlobal>(token, move(names));
    } else if (opt(Token_NonLocal)) {
        vector<Name> names;
        do {
            Token t = match(Token_Identifier);
            names.push_back(internString(t.text));
        } while (opt(Token_Comma));
        return make_unique<SyntaxNonLocal>(token, move(names));
    } else if (opt(Token_Yield)) {
        isGenerator = true;
        return make_unique<SyntaxYield>(token, parseExpr());
    } else if (opt(Token_Break)) {
        return make_unique<SyntaxBreak>(token);
    } else if (opt(Token_Continue)) {
        return make_unique<SyntaxContinue>(token);
    } else if (opt(Token_Del)) {
        unique_ptr<SyntaxTarget> targets(parseTargetList());
        return make_unique<SyntaxDel>(token, move(targets));
    } else if (opt(Token_Import)) {
        vector<unique_ptr<ImportInfo>> imports;
        do {
            imports.emplace_back(parseModuleImport());
        } while (opt(Token_Comma));
        return make_unique<SyntaxImport>(token, move(imports));
    } else if (opt(Token_From)) {
        // todo: parse |"."* module | "."+| here
        Token t = match(Token_Identifier);
        Name moduleName(internString(t.text));
        match(Token_Import);
        bool brackets = opt(Token_Bra);
        vector<unique_ptr<ImportInfo>> imports;
        do {
            imports.emplace_back(parseIdImport());
        } while (opt(Token_Comma));
        if (brackets)
            match(Token_Ket);
        return make_unique<SyntaxFrom>(token, moduleName, move(imports));
    }

    unique_ptr<Syntax> expr = parseExprOrExprList();
    if (opt(Token_Assign)) {
        unique_ptr<SyntaxTarget> target = makeAssignTarget(move(expr));
        return make_unique<SyntaxAssign>(token, move(target), parseAssignSource());
    } else if (opt(Token_AssignAdd)) {
        return parseAugAssign(token, move(expr), BinaryAdd);
    } else if (opt(Token_AssignSub)) {
        return parseAugAssign(token, move(expr), BinarySub);
    } else if (opt(Token_AssignTimes)) {
        return parseAugAssign(token, move(expr), BinaryMul);
    } else if (opt(Token_AssignTrueDiv)) {
        return parseAugAssign(token, move(expr), BinaryTrueDiv);
    } else if (opt(Token_AssignFloorDiv)) {
        return parseAugAssign(token, move(expr), BinaryFloorDiv);
    } else if (opt(Token_AssignModulo)) {
        return parseAugAssign(token, move(expr), BinaryModulo);
    } else if (opt(Token_AssignPower)) {
        return parseAugAssign(token, move(expr), BinaryPower);
    } else if (opt(Token_AssignBitLeftShift)) {
        return parseAugAssign(token, move(expr), BinaryLeftShift);
    } else if (opt(Token_AssignBitRightShift)) {
        return parseAugAssign(token, move(expr), BinaryRightShift);
    } else if (opt(Token_AssignBitAnd)) {
        return parseAugAssign(token, move(expr), BinaryAnd);
    } else if (opt(Token_AssignBitXor)) {
        return parseAugAssign(token, move(expr), BinaryXor);
    } else if (opt(Token_AssignBitOr)) {
        return parseAugAssign(token, move(expr), BinaryOr);
    }

    return expr;
}

vector<Parameter> SyntaxParser::parseParameterList(TokenType endToken)
{
    vector<Parameter> params;
    bool hadDefault = false;
    bool hadRest = false;
    while (!opt(endToken)) {
        // todo: kwargs
        bool takesRest = false;
        if (opt(Token_Times))
            takesRest = true;
        Token t = match(Token_Identifier);
        if (hadRest)
            throw ParseError(t, "Parameter following rest parameter");
        Name name(internString(t.text));
        unique_ptr<Syntax> defaultExpr;
        if (opt(Token_Assign)) {
            if (takesRest)
                throw ParseError(t, "Rest parameter can't take default");
            defaultExpr = parseExpr();
            hadDefault = true;
        } else if (hadDefault && !takesRest) {
            throw ParseError(t, "Non-default parameter following default parameter");
        }
        for (auto i = params.begin(); i != params.end(); i++)
            if (name == i->name)
                throw ParseError(t, "Duplicate parameter name: " +
                                 name->value());
        params.push_back({name, move(defaultExpr), takesRest});
        if (opt(endToken))
            break;
        match(Token_Comma);
        hadRest = takesRest;
    }
    return params;
}

unique_ptr<Syntax> SyntaxParser::parseCompoundStatement()
{
    Token token = currentToken();
    if (opt(Token_If)) {
        vector<IfBranch> branches;
        do {
            unique_ptr<Syntax> cond(parseExpr());
            unique_ptr<SyntaxBlock> suite(parseSuite());
            branches.push_back({move(cond), move(suite)});
        } while (opt(Token_Elif));
        unique_ptr<SyntaxBlock> elseSuite;
        if (opt(Token_Else))
            elseSuite = parseSuite();
        return make_unique<SyntaxIf>(token, move(branches), move(elseSuite));
    } else if (opt(Token_While)) {
        unique_ptr<Syntax> cond(parseExpr());
        unique_ptr<SyntaxBlock> suite(parseSuite());
        unique_ptr<SyntaxBlock> elseSuite;
        if (opt(Token_Else))
            elseSuite = parseSuite();
        return make_unique<SyntaxWhile>(token, move(cond), move(suite),
                               move(elseSuite));
    } else if (opt(Token_For)) {
        unique_ptr<SyntaxTarget> targets(parseTargetList());
        match(Token_In);
        unique_ptr<Syntax> exprs(parseExprOrExprList());
        unique_ptr<SyntaxBlock> suite(parseSuite());
        unique_ptr<SyntaxBlock> elseSuite;
        if (opt(Token_Else))
            elseSuite = parseSuite();
        return make_unique<SyntaxFor>(token, move(targets), move(exprs),
                                      move(suite), move(elseSuite));
    } else if (opt(Token_Try)) {
        Token token = currentToken();
        unique_ptr<SyntaxBlock> suite(parseSuite());
        unique_ptr<SyntaxBlock> elseSuite;
        vector<unique_ptr<ExceptInfo>> excepts;
        unique_ptr<SyntaxBlock> finallySuite;
        bool hadExpr = true;
        while (opt(Token_Except)) {
            if (!hadExpr) {
                throw ParseError(token,
                                 "Expresion-less except clause must be last");
            }
            Token token = currentToken();
            unique_ptr<Syntax> expr;
            unique_ptr<SyntaxTarget> as;
            if (token.type != Token_Colon) {
                expr = parseExprOrExprList();
                if (opt(Token_As))
                    as = parseTarget();
            } else {
                hadExpr = false;
            }
            unique_ptr<SyntaxBlock> suite(parseSuite());
            excepts.emplace_back(new ExceptInfo(token, move(expr), move(as),
                                                move(suite)));
        }
        if (!excepts.empty() && opt(Token_Else))
            elseSuite = parseSuite();
        if (opt(Token_Finally))
            finallySuite = parseSuite();
        return make_unique<SyntaxTry>(token, move(suite), move(excepts),
                                      move(elseSuite), move(finallySuite));
    //} else if (opt(Token_With)) {
    } else if (opt(Token_Def)) {
        Token id = match(Token_Identifier);
        match(Token_Bra);
        vector<Parameter> params = parseParameterList(Token_Ket);
        AutoSetAndRestore asar1(inFunction, true);
        AutoSetAndRestore asar2(isGenerator, false);
        unique_ptr<SyntaxBlock> suite(parseSuite());
        Name name(internString(id.text));
        return make_unique<SyntaxDef>(token, name, move(params), move(suite),
                                      move(isGenerator));
    } else if (opt(Token_Class)) {
        Token id = match(Token_Identifier);
        vector<unique_ptr<Syntax>> bases;
        if (opt(Token_Bra))
            bases = exprListTrailing(Token_Comma, Token_Ket);
        unique_ptr<SyntaxExprList> baseList(
            make_unique<SyntaxExprList>(token, move(bases)));
        unique_ptr<SyntaxBlock> suite(parseSuite());
        Name name(internString(id.text));
        return make_unique<SyntaxClass>(token, name, move(baseList),
                                        move(suite));
    } else {
        unique_ptr<Syntax> stmt = parseSimpleStatement();
        if (opt(Token_Semicolon)) {
            opt(Token_Newline);
        } else {
            if (!atEnd())
                match(Token_Newline);
        }
        return stmt;
    }
}

unique_ptr<SyntaxBlock> SyntaxParser::parseBlock()
{
    Token token = currentToken();
    vector<unique_ptr<Syntax>> stmts;
    match(Token_Indent);
    while (!opt(Token_Dedent)) {
        stmts.emplace_back(parseCompoundStatement());
    }
    return make_unique<SyntaxBlock>(token, move(stmts));
}

unique_ptr<SyntaxBlock> SyntaxParser::parseSuite()
{
    Token token = currentToken();
    vector<unique_ptr<Syntax>> stmts;
    match(Token_Colon);
    if (opt(Token_Newline)) {
        match(Token_Indent);
        while (!opt(Token_Dedent)) {
            stmts.emplace_back(parseCompoundStatement());
        }
    } else {
        do {
            stmts.emplace_back(parseSimpleStatement());
            if (atEnd() || opt(Token_Newline))
                break;
            match(Token_Semicolon);
        } while (!atEnd() && !opt(Token_Newline));
    }
    return make_unique<SyntaxBlock>(token, move(stmts));
}

unique_ptr<SyntaxBlock> SyntaxParser::parseModule()
{
    Token token = currentToken();
    vector<unique_ptr<Syntax>> stmts;
    while (!atEnd())
        stmts.emplace_back(parseCompoundStatement());
    return make_unique<SyntaxBlock>(token, move(stmts));
}
