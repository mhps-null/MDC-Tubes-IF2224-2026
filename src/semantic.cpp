#include "semantic.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace
{
struct TypeInfo
{
    std::string name = "unknown";
    int ref = 0;
    bool isArray = false;
    bool isRecord = false;
};

struct ExprInfo
{
    TypeInfo type;
    std::shared_ptr<SemanticNode> node;
    int tabIndex = 0;
};

class SemanticAnalyzer
{
public:
    SemanticResult analyze(const std::shared_ptr<ParseNode> &root)
    {
        initTables();
        result.ast = visitProgram(root);
        return result;
    }

private:
    SemanticResult result;
    std::vector<int> display;
    int level = 0;
    int nextAddress = 0;

    static std::string lower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }

    static bool startsWith(const std::string &s, const std::string &prefix)
    {
        return s.rfind(prefix, 0) == 0;
    }

    static std::string valueOf(const std::shared_ptr<ParseNode> &node)
    {
        if (!node)
            return "";
        size_t l = node->label.find('(');
        size_t r = node->label.rfind(')');
        if (l == std::string::npos || r == std::string::npos || r <= l)
            return "";
        return node->label.substr(l + 1, r - l - 1);
    }

    static std::string tagOf(const std::shared_ptr<ParseNode> &node)
    {
        if (!node)
            return "";
        size_t l = node->label.find('(');
        std::string tag = l == std::string::npos ? node->label : node->label.substr(0, l);
        return lower(tag);
    }

    static bool isNode(const std::shared_ptr<ParseNode> &node, const std::string &name)
    {
        return node && node->label == name;
    }

    static bool isTerminal(const std::shared_ptr<ParseNode> &node, const std::string &tag)
    {
        return tagOf(node) == tag;
    }

    static std::shared_ptr<SemanticNode> makeNode(const std::string &label)
    {
        return std::make_shared<SemanticNode>(label);
    }

    void error(const std::string &message)
    {
        result.errors.push_back(message);
    }

    void initTables()
    {
        static const char *reserved[] = {
            "and", "array", "begin", "case", "const", "div", "downto", "do",
            "else", "end", "for", "function", "if", "mod", "not", "of",
            "or", "procedure", "program", "record", "repeat", "then", "to",
            "type", "until", "var", "while"};

        result.tab.push_back({"<none>", 0, "sentinel", "none", 0, 1, 0, 0, true});
        for (const char *word : reserved)
            result.tab.push_back({word, 0, "reserved", "none", 0, 1, 0, 0, true});

        result.btab.push_back({});
        display.push_back(0);

        insertRaw("real", "type", "real", 0, true);
        insertRaw("integer", "type", "integer", 0, true);
        insertRaw("char", "type", "char", 0, true);
        insertRaw("boolean", "type", "boolean", 0, true);
        insertRaw("string", "type", "string", 0, true);
        insertRaw("true", "constant", "boolean", 1, true);
        insertRaw("false", "constant", "boolean", 0, true);
        insertRaw("readln", "procedure", "void", 0, true);
        insertRaw("writeln", "procedure", "void", 0, true);
    }

    int currentBlock() const
    {
        return display.empty() ? 0 : display.back();
    }

    int insertRaw(const std::string &name, const std::string &obj, const std::string &type,
                  int adr, bool initialized, int ref = 0, int nrm = 1)
    {
        int block = currentBlock();
        int prev = result.btab[block].last;
        TabEntry entry{lower(name), prev, obj, type, ref, nrm, level, adr, initialized};
        result.tab.push_back(entry);
        int idx = static_cast<int>(result.tab.size()) - 1;
        result.btab[block].last = idx;
        return idx;
    }

    int lookupCurrent(const std::string &name) const
    {
        std::string id = lower(name);
        int idx = result.btab[currentBlock()].last;
        while (idx > 0)
        {
            if (result.tab[idx].identifier == id)
                return idx;
            idx = result.tab[idx].link;
        }
        return 0;
    }

    int lookup(const std::string &name) const
    {
        std::string id = lower(name);
        for (int i = static_cast<int>(display.size()) - 1; i >= 0; --i)
        {
            int idx = result.btab[display[i]].last;
            while (idx > 0)
            {
                if (result.tab[idx].identifier == id)
                    return idx;
                idx = result.tab[idx].link;
            }
        }
        return 0;
    }

    int pushBlock()
    {
        result.btab.push_back({});
        int block = static_cast<int>(result.btab.size()) - 1;
        display.push_back(block);
        level++;
        return block;
    }

    void popBlock()
    {
        if (display.size() > 1)
            display.pop_back();
        if (level > 0)
            level--;
    }

    // Symmetric type compatibility: two types can interact (e.g. in relational exprs)
    bool typeCompatible(const TypeInfo &a, const TypeInfo &b) const
    {
        if (a.name == "unknown" || b.name == "unknown")
            return true;
        if (a.name == b.name)
            return true;
        // Integer and Real are mutually compatible for comparisons
        if ((a.name == "integer" || a.name == "real") &&
            (b.name == "integer" || b.name == "real"))
            return true;
        return false;
    }

    // Assignment compatibility: value type can be assigned to target type
    bool assignmentCompatible(const TypeInfo &target, const TypeInfo &value) const
    {
        if (target.name == "unknown" || value.name == "unknown")
            return true;
        if (target.name == value.name)
            return true;
        // real := integer is allowed (implicit widening)
        if (target.name == "real" && value.name == "integer")
            return true;
        return false;
    }

    // Keep sameType as alias for assignmentCompatible for backward compatibility
    bool sameType(const TypeInfo &a, const TypeInfo &b) const
    {
        return assignmentCompatible(a, b);
    }

    std::shared_ptr<SemanticNode> visitProgram(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("Program");
        if (!node || node->children.size() < 3)
            return ast;

        std::string programName = valueOf(node->children[0]->children.size() > 1
                                              ? node->children[0]->children[1]
                                              : nullptr);
        int idx = insertRaw(programName.empty() ? "<program>" : programName,
                            "program", "none", 0, true);
        ast->annotations.push_back("tab_index:" + std::to_string(idx));
        ast->annotations.push_back("lev:0");

        ast->children.push_back(visitDeclarationPart(node->children[1]));

        int mainBlock = pushBlock();
        auto block = visitCompoundStatement(node->children[2]);
        block->annotations.push_back("block_index:" + std::to_string(mainBlock));
        block->annotations.push_back("lev:" + std::to_string(level));
        ast->children.push_back(block);
        popBlock();
        return ast;
    }

    std::shared_ptr<SemanticNode> visitDeclarationPart(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("Declarations");
        if (!node)
            return ast;

        for (const auto &child : node->children)
        {
            if (isNode(child, "<const-declaration>"))
                ast->children.push_back(visitConstDeclaration(child));
            else if (isNode(child, "<type-declaration>"))
                ast->children.push_back(visitTypeDeclaration(child));
            else if (isNode(child, "<var-declaration>"))
                ast->children.push_back(visitVarDeclaration(child));
            else if (isNode(child, "<subprogram-declaration>"))
                ast->children.push_back(visitSubprogramDeclaration(child));
        }
        return ast;
    }

    std::shared_ptr<SemanticNode> visitConstDeclaration(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("ConstDecls");
        for (size_t i = 1; i + 2 < node->children.size();)
        {
            std::string name = valueOf(node->children[i]);
            ExprInfo val = constantInfo(node->children[i + 2]);
            int idx = declare(name, "constant", val.type, true);
            result.tab[idx].adr = literalAddress(node->children[i + 2]);
            auto c = makeNode("ConstDecl(" + name + ")");
            c->annotations = {"tab_index:" + std::to_string(idx), "type:" + val.type.name,
                              "lev:" + std::to_string(level)};
            ast->children.push_back(c);
            i += 4;
        }
        return ast;
    }

    std::shared_ptr<SemanticNode> visitTypeDeclaration(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("TypeDecls");
        for (size_t i = 1; i + 2 < node->children.size();)
        {
            std::string name = valueOf(node->children[i]);
            TypeInfo type = resolveType(node->children[i + 2]);
            int idx = declare(name, "type", type, true);
            result.tab[idx].ref = type.ref;
            auto t = makeNode("TypeDecl(" + name + ")");
            t->annotations = {"tab_index:" + std::to_string(idx), "type:" + type.name,
                              "ref:" + std::to_string(type.ref), "lev:" + std::to_string(level)};
            ast->children.push_back(t);
            i += 4;
        }
        return ast;
    }

    std::shared_ptr<SemanticNode> visitVarDeclaration(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("VarDecls");
        for (size_t i = 1; i + 2 < node->children.size();)
        {
            std::vector<std::string> names = identifierList(node->children[i]);
            TypeInfo type = resolveType(node->children[i + 2]);
            for (const auto &name : names)
            {
                int idx = declare(name, "variable", type, false);
                result.tab[idx].adr = nextAddress++;
                auto v = makeNode("VarDecl(" + name + ")");
                v->annotations = {"tab_index:" + std::to_string(idx), "type:" + type.name,
                                  "ref:" + std::to_string(type.ref), "lev:" + std::to_string(level)};
                ast->children.push_back(v);
                result.btab[currentBlock()].vsze++;
            }
            i += 4;
        }
        return ast;
    }

    std::shared_ptr<SemanticNode> visitSubprogramDeclaration(const std::shared_ptr<ParseNode> &node)
    {
        if (!node || node->children.empty())
            return makeNode("Subprogram");

        auto decl = node->children[0];
        bool isFunction = isNode(decl, "<function-declaration>");
        std::string name = valueOf(decl->children.size() > 1 ? decl->children[1] : nullptr);

        int blockRef = static_cast<int>(result.btab.size());
        TypeInfo returnType;
        returnType.name = isFunction ? "unknown" : "void";
        if (isFunction)
        {
            for (size_t i = 0; i + 1 < decl->children.size(); ++i)
                if (isTerminal(decl->children[i], "colon") && isTerminal(decl->children[i + 1], "ident"))
                    returnType = typeFromIdentifier(valueOf(decl->children[i + 1]));
        }

        int subIdx = declare(name, isFunction ? "function" : "procedure", returnType, true);
        result.tab[subIdx].ref = blockRef;

        int block = pushBlock();
        auto ast = makeNode(isFunction ? "FunctionDecl(" + name + ")" : "ProcedureDecl(" + name + ")");
        ast->annotations = {"tab_index:" + std::to_string(subIdx), "block_index:" + std::to_string(block),
                            "lev:" + std::to_string(level - 1), "type:" + returnType.name};

        for (const auto &child : decl->children)
        {
            if (isNode(child, "<formal-parameter-list>"))
                ast->children.push_back(visitFormalParameters(child));
            else if (isNode(child, "<block>"))
                ast->children.push_back(visitBlock(child));
        }
        popBlock();
        return ast;
    }

    std::shared_ptr<SemanticNode> visitFormalParameters(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("Parameters");
        for (const auto &child : node->children)
        {
            if (!isNode(child, "<parameter-group>"))
                continue;
            auto names = identifierList(child->children[0]);
            TypeInfo type = resolveParameterType(child);
            for (const auto &name : names)
            {
                int idx = declare(name, "parameter", type, true);
                result.tab[idx].adr = result.btab[currentBlock()].psze++;
                result.btab[currentBlock()].lpar = idx;
                auto p = makeNode("Param(" + name + ")");
                p->annotations = {"tab_index:" + std::to_string(idx), "type:" + type.name,
                                  "lev:" + std::to_string(level)};
                ast->children.push_back(p);
            }
        }
        return ast;
    }

    std::shared_ptr<SemanticNode> visitBlock(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("Block");
        ast->annotations.push_back("block_index:" + std::to_string(currentBlock()));
        ast->annotations.push_back("lev:" + std::to_string(level));
        if (node && node->children.size() >= 2)
        {
            ast->children.push_back(visitDeclarationPart(node->children[0]));
            ast->children.push_back(visitCompoundStatement(node->children[1]));
        }
        return ast;
    }

    std::shared_ptr<SemanticNode> visitCompoundStatement(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("Compound");
        if (!node)
            return ast;
        for (const auto &child : node->children)
            if (isNode(child, "<statement-list>"))
                ast->children.push_back(visitStatementList(child));
        return ast;
    }

    std::shared_ptr<SemanticNode> visitStatementList(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("Statements");
        if (!node)
            return ast;
        for (const auto &child : node->children)
        {
            auto stmt = visitStatement(child);
            if (stmt)
                ast->children.push_back(stmt);
        }
        return ast;
    }

    std::shared_ptr<SemanticNode> visitStatement(const std::shared_ptr<ParseNode> &node)
    {
        if (!node)
            return nullptr;
        if (isNode(node, "<statement>") || isTerminal(node, "semicolon"))
            return nullptr;
        if (isNode(node, "<assignment-statement>"))
            return visitAssignment(node);
        if (isNode(node, "<procedure/function-call>"))
            return visitCall(node);
        if (isNode(node, "<if-statement>"))
            return visitIf(node);
        if (isNode(node, "<while-statement>"))
            return visitWhile(node);
        if (isNode(node, "<repeat-statement>"))
            return visitRepeat(node);
        if (isNode(node, "<for-statement>"))
            return visitFor(node);
        if (isNode(node, "<case-statement>"))
            return visitGeneric("Case", node);
        if (isNode(node, "<compound-statement>"))
            return visitCompoundStatement(node);
        return nullptr;
    }

    std::shared_ptr<SemanticNode> visitAssignment(const std::shared_ptr<ParseNode> &node)
    {
        ExprInfo target = variableInfo(node->children[0], false);
        ExprInfo value = expressionInfo(node->children[2]);
        if (!assignmentCompatible(target.type, value.type))
            error("assignment incompatible: cannot assign " + value.type.name +
                  " to " + target.type.name);
        if (target.tabIndex > 0)
            result.tab[target.tabIndex].initialized = true;
        auto ast = makeNode("Assign");
        ast->annotations.push_back("type:void");
        ast->children.push_back(target.node);
        ast->children.push_back(value.node);
        return ast;
    }

    std::shared_ptr<SemanticNode> visitCall(const std::shared_ptr<ParseNode> &node)
    {
        std::string name = valueOf(node->children.empty() ? nullptr : node->children[0]);
        int idx = lookup(name);
        if (!idx)
            error("undeclared procedure/function: " + name);
        auto ast = makeNode("Call(" + name + ")");
        ast->annotations = {"tab_index:" + std::to_string(idx), "lev:" + std::to_string(level)};
        for (const auto &child : node->children)
            if (isNode(child, "<parameter-list>"))
                for (const auto &arg : child->children)
                    if (isNode(arg, "<expression>"))
                        ast->children.push_back(expressionInfo(arg).node);
        return ast;
    }

    std::shared_ptr<SemanticNode> visitIf(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("If");
        ExprInfo cond = expressionInfo(node->children[1]);
        if (cond.type.name != "boolean" && cond.type.name != "unknown")
            error("if condition must be boolean, got " + cond.type.name);
        ast->children.push_back(cond.node);
        for (size_t i = 3; i < node->children.size(); ++i)
            if (!isTerminal(node->children[i], "elsesy"))
            {
                auto stmt = visitStatement(node->children[i]);
                if (stmt)
                    ast->children.push_back(stmt);
            }
        return ast;
    }

    std::shared_ptr<SemanticNode> visitWhile(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("While");
        ExprInfo cond = expressionInfo(node->children[1]);
        if (cond.type.name != "boolean" && cond.type.name != "unknown")
            error("while condition must be boolean, got " + cond.type.name);
        ast->children.push_back(cond.node);
        if (node->children.size() > 3)
            ast->children.push_back(visitStatement(node->children[3]));
        return ast;
    }

    std::shared_ptr<SemanticNode> visitRepeat(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("Repeat");
        ast->children.push_back(visitStatementList(node->children[1]));
        ExprInfo cond = expressionInfo(node->children[3]);
        if (cond.type.name != "boolean" && cond.type.name != "unknown")
            error("repeat condition must be boolean, got " + cond.type.name);
        ast->children.push_back(cond.node);
        return ast;
    }

    std::shared_ptr<SemanticNode> visitFor(const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode("For");
        std::string name = valueOf(node->children[1]);
        ExprInfo var = identifierInfo(name);
        ExprInfo start = expressionInfo(node->children[3]);
        ExprInfo finish = expressionInfo(node->children[5]);
        if (var.type.name != "integer" && var.type.name != "char" && var.type.name != "unknown")
            error("for variable must be ordinal, got " + var.type.name);
        if (!typeCompatible(var.type, start.type) || !typeCompatible(var.type, finish.type))
            error("for range type is incompatible with variable " + name);
        ast->children = {var.node, start.node, finish.node};
        if (node->children.size() > 7)
            ast->children.push_back(visitStatement(node->children[7]));
        return ast;
    }

    std::shared_ptr<SemanticNode> visitGeneric(const std::string &label, const std::shared_ptr<ParseNode> &node)
    {
        auto ast = makeNode(label);
        for (const auto &child : node->children)
        {
            if (isNode(child, "<expression>"))
                ast->children.push_back(expressionInfo(child).node);
            else
            {
                auto stmt = visitStatement(child);
                if (stmt)
                    ast->children.push_back(stmt);
            }
        }
        return ast;
    }

    int declare(const std::string &name, const std::string &obj, const TypeInfo &type, bool initialized)
    {
        if (lookupCurrent(name))
            error("redeclaration in same scope: " + name);
        return insertRaw(name, obj, type.name, 0, initialized, type.ref);
    }

    TypeInfo typeFromIdentifier(const std::string &name)
    {
        int idx = lookup(name);
        if (!idx)
        {
            error("unknown type: " + name);
            return {"unknown", 0, false, false};
        }
        if (result.tab[idx].obj != "type")
            error(name + " is not a type identifier");
        return {result.tab[idx].type, result.tab[idx].ref,
                result.tab[idx].type == "array", result.tab[idx].type == "record"};
    }

    TypeInfo resolveParameterType(const std::shared_ptr<ParseNode> &group)
    {
        for (const auto &child : group->children)
        {
            if (isNode(child, "<array-type>"))
                return resolveArrayType(child);
            if (isTerminal(child, "ident"))
                return typeFromIdentifier(valueOf(child));
        }
        return {"unknown", 0, false, false};
    }

    TypeInfo resolveType(const std::shared_ptr<ParseNode> &node)
    {
        if (!node)
            return {"unknown", 0, false, false};
        if (isNode(node, "<type>") && !node->children.empty())
            return resolveType(node->children[0]);
        if (isTerminal(node, "ident"))
            return typeFromIdentifier(valueOf(node));
        if (isNode(node, "<array-type>"))
            return resolveArrayType(node);
        if (isNode(node, "<record-type>"))
            return resolveRecordType(node);
        if (isNode(node, "<range>"))
            return rangeType(node);
        if (isNode(node, "<enumerated>"))
            return {"integer", 0, false, false};
        return {"unknown", 0, false, false};
    }

    TypeInfo resolveArrayType(const std::shared_ptr<ParseNode> &node)
    {
        TypeInfo indexType{"integer", 0, false, false};
        int low = 0;
        int high = 0;
        TypeInfo elemType{"unknown", 0, false, false};
        for (const auto &child : node->children)
        {
            if (isNode(child, "<range>"))
            {
                indexType = rangeType(child);
                bounds(child, low, high);
            }
            else if (isTerminal(child, "ident") && elemType.name == "unknown")
            {
                indexType = typeFromIdentifier(valueOf(child));
            }
            else if (isNode(child, "<type>"))
            {
                elemType = resolveType(child);
            }
        }
        if (indexType.name == "real")
            error("array index type cannot be real");
        int count = high >= low ? high - low + 1 : 0;
        result.atab.push_back({indexType.name, elemType.name, elemType.ref, low, high, 1, count});
        int ref = static_cast<int>(result.atab.size()) - 1;
        return {"array", ref, true, false};
    }

    TypeInfo resolveRecordType(const std::shared_ptr<ParseNode> &node)
    {
        result.btab.push_back({});
        int recordBlock = static_cast<int>(result.btab.size()) - 1;
        display.push_back(recordBlock);
        level++;
        for (const auto &child : node->children)
            if (isNode(child, "<field-list>"))
                declareFields(child);
        popBlock();
        return {"record", recordBlock, false, true};
    }

    void declareFields(const std::shared_ptr<ParseNode> &node)
    {
        if (isNode(node, "<field-part>"))
        {
            auto names = identifierList(node->children[0]);
            TypeInfo type = resolveType(node->children[2]);
            for (const auto &name : names)
                declare(name, "field", type, true);
            return;
        }
        for (const auto &child : node->children)
            declareFields(child);
    }

    TypeInfo rangeType(const std::shared_ptr<ParseNode> &range)
    {
        ExprInfo left = expressionInfo(range->children[0]);
        ExprInfo right = expressionInfo(range->children[3]);
        if (left.type.name != right.type.name && left.type.name != "unknown" && right.type.name != "unknown")
            error("range bounds must have same type");
        if (left.type.name == "real")
            error("subrange cannot use real bounds");
        int low = 0;
        int high = 0;
        bounds(range, low, high);
        if (low > high)
            error("range lower bound is greater than upper bound");
        return left.type;
    }

    void bounds(const std::shared_ptr<ParseNode> &range, int &low, int &high)
    {
        low = literalAddress(range->children[0]);
        high = literalAddress(range->children[3]);
    }

    std::vector<std::string> identifierList(const std::shared_ptr<ParseNode> &node)
    {
        std::vector<std::string> names;
        if (!node)
            return names;
        for (const auto &child : node->children)
            if (isTerminal(child, "ident"))
                names.push_back(valueOf(child));
        return names;
    }

    ExprInfo constantInfo(const std::shared_ptr<ParseNode> &node)
    {
        if (!node)
            return {{"unknown", 0, false, false}, makeNode("Const(?)"), 0};
        if (isNode(node, "<constant>"))
        {
            if (node->children.size() == 2)
            {
                ExprInfo expr = constantInfo(node->children[1]);
                expr.node->label = valueOf(node->children[0]) == "-" ? "UnaryMinus" : expr.node->label;
                return expr;
            }
            if (!node->children.empty())
                return constantInfo(node->children.back());
        }
        std::string tag = tagOf(node);
        std::string val = valueOf(node);
        if (tag == "intcon")
            return {{"integer", 0, false, false}, annotated("Int(" + val + ")", "type:integer"), 0};
        if (tag == "realcon")
            return {{"real", 0, false, false}, annotated("Real(" + val + ")", "type:real"), 0};
        if (tag == "charcon")
            return {{"char", 0, false, false}, annotated("Char(" + val + ")", "type:char"), 0};
        if (tag == "string")
            return {{"string", 0, false, false}, annotated("String(" + val + ")", "type:string"), 0};
        if (tag == "ident")
            return identifierInfo(val);
        return {{"unknown", 0, false, false}, makeNode("Const(?)"), 0};
    }

    std::shared_ptr<SemanticNode> annotated(const std::string &label, const std::string &annotation)
    {
        auto node = makeNode(label);
        node->annotations.push_back(annotation);
        return node;
    }

    int literalAddress(const std::shared_ptr<ParseNode> &node)
    {
        if (!node)
            return 0;
        if (isNode(node, "<constant>") || isNode(node, "<expression>") ||
            isNode(node, "<simple-expression>") || isNode(node, "<term>") ||
            isNode(node, "<factor>"))
        {
            int sign = 1;
            for (const auto &child : node->children)
            {
                if (isTerminal(child, "minus"))
                    sign = -1;
                if (isTerminal(child, "intcon") || isTerminal(child, "charcon") || isTerminal(child, "ident"))
                    return sign * literalAddress(child);
                int nested = literalAddress(child);
                if (nested != 0)
                    return sign * nested;
            }
            return 0;
        }
        std::string tag = tagOf(node);
        std::string val = valueOf(node);
        if (tag == "intcon")
            return std::stoi(val);
        if (tag == "charcon" && !val.empty())
            return static_cast<int>(val[0]);
        if (tag == "ident")
        {
            int idx = lookup(val);
            return idx ? result.tab[idx].adr : 0;
        }
        return 0;
    }

    ExprInfo expressionInfo(const std::shared_ptr<ParseNode> &node)
    {
        if (!node)
            return {{"unknown", 0, false, false}, makeNode("Expr(?)"), 0};
        if (isNode(node, "<expression>"))
        {
            ExprInfo left = expressionInfo(node->children[0]);
            if (node->children.size() == 1)
                return left;
            ExprInfo right = expressionInfo(node->children[2]);
            std::string op = tagOf(node->children[1]);
            auto bin = makeNode("BinOp(" + op + ")");
            bin->annotations.push_back("type:boolean");
            bin->children = {left.node, right.node};
            // Relational operators require compatible types (symmetric check)
            if (!typeCompatible(left.type, right.type))
                error("relational operands are incompatible: " + left.type.name +
                      " " + op + " " + right.type.name);
            return {{"boolean", 0, false, false}, bin, 0};
        }
        if (isNode(node, "<simple-expression>"))
            return foldExpression(node, {"plus", "minus", "orsy"});
        if (isNode(node, "<term>"))
            return foldExpression(node, {"times", "rdiv", "idiv", "imod", "andsy"});
        if (isNode(node, "<factor>"))
            return factorInfo(node);
        return constantInfo(node);
    }

    ExprInfo foldExpression(const std::shared_ptr<ParseNode> &node, const std::vector<std::string> &ops)
    {
        size_t pos = 0;
        bool unaryMinus = false;
        if (!node->children.empty() && (isTerminal(node->children[0], "plus") || isTerminal(node->children[0], "minus")))
        {
            unaryMinus = isTerminal(node->children[0], "minus");
            pos = 1;
        }
        ExprInfo acc = expressionInfo(node->children[pos++]);
        if (unaryMinus)
            acc.node->annotations.push_back("unary:-");
        while (pos + 1 < node->children.size())
        {
            std::string op = tagOf(node->children[pos]);
            ExprInfo rhs = expressionInfo(node->children[pos + 1]);
            TypeInfo resultType = operationType(op, acc.type, rhs.type);
            auto bin = makeNode("BinOp(" + op + ")");
            bin->annotations.push_back("type:" + resultType.name);
            bin->children = {acc.node, rhs.node};
            acc = {resultType, bin, 0};
            pos += 2;
        }
        (void)ops;
        return acc;
    }

    TypeInfo operationType(const std::string &op, const TypeInfo &left, const TypeInfo &right)
    {
        // Logical operators: require boolean operands
        if (op == "orsy" || op == "andsy")
        {
            if (left.name != "boolean" && left.name != "unknown")
                error("operator '" + op + "' requires boolean operands, got " + left.name);
            if (right.name != "boolean" && right.name != "unknown")
                error("operator '" + op + "' requires boolean operands, got " + right.name);
            return {"boolean", 0, false, false};
        }
        // Real division: always returns real
        if (op == "rdiv")
        {
            if (left.name != "integer" && left.name != "real" && left.name != "unknown")
                error("operator '/' requires numeric operands, got " + left.name);
            if (right.name != "integer" && right.name != "real" && right.name != "unknown")
                error("operator '/' requires numeric operands, got " + right.name);
            return {"real", 0, false, false};
        }
        // Integer division and modulus: require integer operands
        if (op == "idiv" || op == "imod")
        {
            if (left.name != "integer" && left.name != "unknown")
                error("operator '" + op + "' requires integer operands, got " + left.name);
            if (right.name != "integer" && right.name != "unknown")
                error("operator '" + op + "' requires integer operands, got " + right.name);
            return {"integer", 0, false, false};
        }
        // Arithmetic (+, -, *): numeric operands
        if ((left.name == "integer" || left.name == "real" || left.name == "unknown") &&
            (right.name == "integer" || right.name == "real" || right.name == "unknown"))
        {
            if (left.name == "real" || right.name == "real")
                return {"real", 0, false, false};
            if (left.name == "unknown" || right.name == "unknown")
                return {"unknown", 0, false, false};
            return {"integer", 0, false, false};
        }
        // String concatenation with + is not supported in Arion (no implicit coercion)
        error("arithmetic operands are incompatible: " + left.name + " " + op + " " + right.name);
        return {"unknown", 0, false, false};
    }

    ExprInfo factorInfo(const std::shared_ptr<ParseNode> &node)
    {
        if (node->children.empty())
            return {{"unknown", 0, false, false}, makeNode("Factor(?)"), 0};
        if (isTerminal(node->children[0], "notsy"))
        {
            ExprInfo e = expressionInfo(node->children[1]);
            if (e.type.name != "boolean" && e.type.name != "unknown")
                error("not operator requires boolean operand");
            auto n = makeNode("Not");
            n->annotations.push_back("type:boolean");
            n->children.push_back(e.node);
            return {{"boolean", 0, false, false}, n, 0};
        }
        if (isTerminal(node->children[0], "lparent") && node->children.size() > 1)
            return expressionInfo(node->children[1]);
        if (isNode(node->children[0], "<variable>"))
            return variableInfo(node->children[0], true);
        if (isNode(node->children[0], "<procedure/function-call>"))
        {
            auto call = visitCall(node->children[0]);
            return {{"unknown", 0, false, false}, call, 0};
        }
        return constantInfo(node->children[0]);
    }

    void refreshTypeAnnotation(const std::shared_ptr<SemanticNode> &node, const TypeInfo &type)
    {
        if (!node)
            return;
        for (auto &annotation : node->annotations)
            if (startsWith(annotation, "type:"))
            {
                annotation = "type:" + type.name;
                return;
            }
        node->annotations.push_back("type:" + type.name);
    }

    int lookupInBlock(int block, const std::string &name) const
    {
        if (block < 0 || block >= static_cast<int>(result.btab.size()))
            return 0;
        std::string id = lower(name);
        int idx = result.btab[block].last;
        while (idx > 0)
        {
            if (result.tab[idx].identifier == id)
                return idx;
            idx = result.tab[idx].link;
        }
        return 0;
    }

    ExprInfo variableInfo(const std::shared_ptr<ParseNode> &node, bool warnUninitialized = true)
    {
        if (!node || node->children.empty())
            return {{"unknown", 0, false, false}, makeNode("Var(?)"), 0};
        ExprInfo base = identifierInfo(valueOf(node->children[0]), warnUninitialized);
        for (size_t i = 1; i < node->children.size(); ++i)
        {
            if (!isNode(node->children[i], "<component-variable>"))
                continue;
            auto comp = node->children[i];
            if (!comp->children.empty() && isTerminal(comp->children[0], "lbrack"))
            {
                base.node->label += "[]";
                if (base.type.name != "array")
                    error("indexing non-array variable");
                else if (base.type.ref >= 0 && base.type.ref < static_cast<int>(result.atab.size()))
                {
                    base.type = {result.atab[base.type.ref].etyp, result.atab[base.type.ref].eref,
                                 result.atab[base.type.ref].etyp == "array",
                                 result.atab[base.type.ref].etyp == "record"};
                    refreshTypeAnnotation(base.node, base.type);
                }
            }
            else if (!comp->children.empty() && isTerminal(comp->children[0], "period"))
            {
                std::string fieldName = valueOf(comp->children[1]);
                base.node->label += "." + fieldName;
                if (base.type.name != "record")
                {
                    error("field access on non-record variable");
                    base.type = {"unknown", 0, false, false};
                }
                else
                {
                    int fieldIdx = lookupInBlock(base.type.ref, fieldName);
                    if (!fieldIdx)
                    {
                        error("unknown record field: " + fieldName);
                        base.type = {"unknown", 0, false, false};
                    }
                    else
                    {
                        const auto &field = result.tab[fieldIdx];
                        base.type = {field.type, field.ref, field.type == "array", field.type == "record"};
                        base.tabIndex = fieldIdx;
                    }
                }
                refreshTypeAnnotation(base.node, base.type);
            }
        }
        return base;
    }

    ExprInfo identifierInfo(const std::string &name, bool warnUninitialized = true)
    {
        int idx = lookup(name);
        if (!idx)
        {
            error("undeclared identifier: " + name);
            auto n = makeNode("Var(" + name + ")");
            n->annotations = {"tab_index:0", "type:unknown", "lev:" + std::to_string(level)};
            return {{"unknown", 0, false, false}, n, 0};
        }
        const auto &entry = result.tab[idx];
        auto n = makeNode("Var(" + name + ")");
        n->annotations = {"tab_index:" + std::to_string(idx), "type:" + entry.type,
                          "lev:" + std::to_string(entry.lev)};
        if (warnUninitialized && entry.obj == "variable" && !entry.initialized)
            n->annotations.push_back("warning:possibly_uninitialized");
        return {{entry.type, entry.ref, entry.type == "array", entry.type == "record"}, n, idx};
    }
};

void printSemanticTree(const std::shared_ptr<SemanticNode> &node, std::ostream &out,
                       const std::string &prefix, bool isLast)
{
    if (!node)
        return;
    out << prefix << (isLast ? "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 "
                             : "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 ")
        << node->label;
    if (!node->annotations.empty())
    {
        out << " -> ";
        for (size_t i = 0; i < node->annotations.size(); ++i)
        {
            if (i)
                out << ", ";
            out << node->annotations[i];
        }
    }
    out << '\n';

    std::string childPrefix = prefix + (isLast ? "    " : "\xe2\x94\x82   ");
    for (size_t i = 0; i < node->children.size(); ++i)
        printSemanticTree(node->children[i], out, childPrefix, i + 1 == node->children.size());
}
} // namespace

SemanticResult analyzeSemantics(const std::shared_ptr<ParseNode> &root)
{
    SemanticAnalyzer analyzer;
    return analyzer.analyze(root);
}

void printSemanticResult(const SemanticResult &result, std::ostream &out)
{
    out << "=== DECORATED AST ===\n";
    if (result.ast)
    {
        out << result.ast->label;
        if (!result.ast->annotations.empty())
        {
            out << " -> ";
            for (size_t i = 0; i < result.ast->annotations.size(); ++i)
            {
                if (i)
                    out << ", ";
                out << result.ast->annotations[i];
            }
        }
        out << '\n';
        for (size_t i = 0; i < result.ast->children.size(); ++i)
            printSemanticTree(result.ast->children[i], out, "", i + 1 == result.ast->children.size());
    }
    else
    {
        out << "(empty AST)\n";
    }

    out << "\n=== SYMBOL TABLE: tab ===\n";
    out << "idx  " << std::left << std::setw(24) << "identifier"
        << std::setw(12) << "obj" << std::setw(10) << "type"
        << "ref  nrm  lev  adr  link  init\n";
    out << "---------------------------------------------------------------\n";
    for (size_t i = 0; i < result.tab.size(); ++i)
    {
        const auto &e = result.tab[i];
        out << std::right << std::setw(3) << i << "  " << std::left
            << std::setw(24) << e.identifier << std::setw(12) << e.obj
            << std::setw(10) << e.type << std::right << std::setw(3) << e.ref
            << std::setw(5) << e.nrm << std::setw(5) << e.lev
            << std::setw(5) << e.adr << std::setw(6) << e.link
            << "  " << (e.initialized ? "yes" : "no") << '\n';
    }

    out << "\n=== SYMBOL TABLE: btab ===\n";
    out << "idx  last  lpar  psze  vsze\n";
    out << "----------------------------\n";
    for (size_t i = 0; i < result.btab.size(); ++i)
    {
        const auto &e = result.btab[i];
        out << std::setw(3) << i << std::setw(6) << e.last << std::setw(6)
            << e.lpar << std::setw(6) << e.psze << std::setw(6) << e.vsze << '\n';
    }

    out << "\n=== SYMBOL TABLE: atab ===\n";
    out << "idx  xtyp       etyp       eref  low  high  elsz  size\n";
    out << "------------------------------------------------------\n";
    for (size_t i = 0; i < result.atab.size(); ++i)
    {
        const auto &e = result.atab[i];
        out << std::setw(3) << i << "  " << std::left << std::setw(10) << e.xtyp
            << std::setw(10) << e.etyp << std::right << std::setw(4) << e.eref
            << std::setw(5) << e.low << std::setw(6) << e.high
            << std::setw(6) << e.elsz << std::setw(6) << e.size << '\n';
    }

    out << "\n=== SEMANTIC DIAGNOSTICS ===\n";
    if (result.errors.empty())
        out << "No semantic errors.\n";
    else
        for (const auto &err : result.errors)
            out << "Semantic error: " << err << '\n';
}
