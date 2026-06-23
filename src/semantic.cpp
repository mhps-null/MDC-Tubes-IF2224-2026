#include "semantic.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>

namespace
{
    enum class TypeKind
    {
        Unknown,
        Void,
        Integer,
        Real,
        Char,
        Boolean,
        String,
        Subrange,
        Enumerated,
        Array,
        Record
    };

    struct TypeInfo
    {
        TypeKind kind = TypeKind::Unknown;
        std::string name = "unknown";
        TypeKind base = TypeKind::Unknown;
        int ref = 0;
        int low = 0;
        int high = 0;
        int stringLength = -1;
        bool anonymous = false;

        bool isUnknown() const { return kind == TypeKind::Unknown; }
        bool isNumeric() const { return kind == TypeKind::Integer || kind == TypeKind::Real || base == TypeKind::Integer; }
        bool isOrdinal() const
        {
            return kind == TypeKind::Integer || kind == TypeKind::Char || kind == TypeKind::Boolean ||
                   kind == TypeKind::Enumerated || kind == TypeKind::Subrange;
        }
    };

    struct ExprInfo
    {
        TypeInfo type;
        std::shared_ptr<SemanticNode> node;
        int tabIndex = 0;
        bool constant = false;
        int intValue = 0;
        std::string stringValue;
    };

    struct ParamInfo
    {
        std::string name;
        TypeInfo type;
        int tabIndex = 0;
    };

    static std::string lower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    static bool startsWith(const std::string &s, const std::string &prefix)
    {
        return s.rfind(prefix, 0) == 0;
    }

    static std::string typeKindName(TypeKind kind)
    {
        switch (kind)
        {
        case TypeKind::Void:
            return "void";
        case TypeKind::Integer:
            return "integer";
        case TypeKind::Real:
            return "real";
        case TypeKind::Char:
            return "char";
        case TypeKind::Boolean:
            return "boolean";
        case TypeKind::String:
            return "string";
        case TypeKind::Subrange:
            return "subrange";
        case TypeKind::Enumerated:
            return "enumerated";
        case TypeKind::Array:
            return "array";
        case TypeKind::Record:
            return "record";
        default:
            return "unknown";
        }
    }

    static TypeKind primitiveKind(const std::string &name)
    {
        std::string id = lower(name);
        if (id == "integer")
            return TypeKind::Integer;
        if (id == "real")
            return TypeKind::Real;
        if (id == "char")
            return TypeKind::Char;
        if (id == "boolean")
            return TypeKind::Boolean;
        if (id == "string")
            return TypeKind::String;
        if (id == "void")
            return TypeKind::Void;
        return TypeKind::Unknown;
    }

    static TypeInfo primitive(TypeKind kind)
    {
        TypeInfo type;
        type.kind = kind;
        type.base = kind;
        type.name = typeKindName(kind);
        return type;
    }

    static TypeInfo unknownType()
    {
        return primitive(TypeKind::Unknown);
    }

    static TypeInfo namedPrimitive(const std::string &name)
    {
        TypeKind kind = primitiveKind(name);
        if (kind == TypeKind::Unknown)
            return unknownType();
        return primitive(kind);
    }

    class ParseTreeView
    {
    public:
        static std::string tag(const std::shared_ptr<ParseNode> &node)
        {
            if (!node)
                return "";
            size_t pos = node->label.find('(');
            return lower(pos == std::string::npos ? node->label : node->label.substr(0, pos));
        }

        static std::string value(const std::shared_ptr<ParseNode> &node)
        {
            if (!node)
                return "";
            size_t l = node->label.find('(');
            size_t r = node->label.rfind(')');
            if (l == std::string::npos || r == std::string::npos || r <= l)
                return "";
            return node->label.substr(l + 1, r - l - 1);
        }

        static bool node(const std::shared_ptr<ParseNode> &node, const std::string &label)
        {
            return node && node->label == label;
        }

        static bool terminal(const std::shared_ptr<ParseNode> &node, const std::string &tagName)
        {
            return tag(node) == tagName;
        }
    };

    class SemanticTreePrinter
    {
    public:
        static void print(const std::shared_ptr<SemanticNode> &node, std::ostream &out)
        {
            if (!node)
            {
                out << "(empty AST)\n";
                return;
            }

            printNodeHeader(node, out);
            out << '\n';
            for (size_t i = 0; i < node->children.size(); ++i)
                printChild(node->children[i], out, "", i + 1 == node->children.size());
        }

    private:
        static void printNodeHeader(const std::shared_ptr<SemanticNode> &node, std::ostream &out)
        {
            out << node->label;
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
        }

        static void printChild(const std::shared_ptr<SemanticNode> &node, std::ostream &out,
                               const std::string &prefix, bool isLast)
        {
            if (!node)
                return;

            out << prefix << (isLast ? "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80 " : "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80 ");
            printNodeHeader(node, out);
            out << '\n';

            std::string childPrefix = prefix + (isLast ? "    " : "\xe2\x94\x82   ");
            for (size_t i = 0; i < node->children.size(); ++i)
                printChild(node->children[i], out, childPrefix, i + 1 == node->children.size());
        }
    };

    class SymbolTable
    {
    public:
        explicit SymbolTable(SemanticResult &semanticResult) : result(semanticResult)
        {
            result.tab.clear();
            result.btab.clear();
            result.atab.clear();
            result.tab.push_back({"<none>", 0, "sentinel", "none", 0, 1, 0, 0, true, {}});
            result.btab.push_back({});
            display.push_back(0);
        }

        int lexicalLevel() const { return level; }
        int currentBlock() const { return display.empty() ? 0 : display.back(); }

        int pushBlock()
        {
            result.btab.push_back({});
            int idx = static_cast<int>(result.btab.size()) - 1;
            display.push_back(idx);
            level++;
            return idx;
        }

        void pushExistingBlock(int block)
        {
            display.push_back(block);
            level++;
        }

        void popBlock()
        {
            if (display.size() > 1)
                display.pop_back();
            if (level > 0)
                level--;
        }

        int declare(const std::string &name, const std::string &obj, const TypeInfo &type,
                    bool initialized, std::vector<std::string> params = {}, int nrm = 1)
        {
            int block = currentBlock();
            int previous = result.btab[block].last;
            TabEntry entry{lower(name), previous, obj, type.name, type.ref, nrm,
                           level, 0, initialized, params};
            result.tab.push_back(entry);
            int idx = static_cast<int>(result.tab.size()) - 1;
            result.btab[block].last = idx;
            symbolTypes[idx] = type;
            return idx;
        }

        int lookupCurrent(const std::string &name) const
        {
            return lookupInBlock(currentBlock(), name);
        }

        int lookup(const std::string &name) const
        {
            std::string id = lower(name);
            for (int i = static_cast<int>(display.size()) - 1; i >= 0; --i)
            {
                int found = lookupInBlock(display[i], id);
                if (found)
                    return found;
            }
            return 0;
        }

        int lookupCallable(const std::string &name) const
        {
            std::string id = lower(name);
            for (int i = static_cast<int>(display.size()) - 1; i >= 0; --i)
            {
                int block = display[i];
                if (block < 0 || block >= static_cast<int>(result.btab.size())) continue;
                int idx = result.btab[block].last;
                while (idx > 0)
                {
                    if (result.tab[idx].identifier == id && (result.tab[idx].obj == "function" || result.tab[idx].obj == "procedure"))
                        return idx;
                    idx = result.tab[idx].link;
                }
            }
            return 0;
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

        TypeInfo typeOf(int tabIndex) const
        {
            auto it = symbolTypes.find(tabIndex);
            if (it != symbolTypes.end())
                return it->second;
            if (tabIndex > 0 && tabIndex < static_cast<int>(result.tab.size()))
                return namedPrimitive(result.tab[tabIndex].type);
            return unknownType();
        }

        void setType(int tabIndex, const TypeInfo &type)
        {
            if (tabIndex <= 0 || tabIndex >= static_cast<int>(result.tab.size()))
                return;
            symbolTypes[tabIndex] = type;
            result.tab[tabIndex].type = type.name;
            result.tab[tabIndex].ref = type.ref;
        }

        void markInitialized(int tabIndex)
        {
            if (tabIndex > 0 && tabIndex < static_cast<int>(result.tab.size()))
                result.tab[tabIndex].initialized = true;
        }

        bool isInitialized(int tabIndex) const
        {
            return tabIndex > 0 && tabIndex < static_cast<int>(result.tab.size()) &&
                   result.tab[tabIndex].initialized;
        }

        int addArray(const TypeInfo &indexType, const TypeInfo &elementType,
                     int low, int high, int elementSize)
        {
            int count = high >= low ? high - low + 1 : 0;
            ATabEntry entry{indexType.name, elementType.name, elementType.ref,
                            low, high, elementSize, count * elementSize};
            result.atab.push_back(entry);
            return static_cast<int>(result.atab.size()) - 1;
        }

        int addRecordBlock()
        {
            result.btab.push_back({});
            return static_cast<int>(result.btab.size()) - 1;
        }

        int sizeOf(const TypeInfo &type) const
        {
            if (type.kind == TypeKind::Array && type.ref >= 0 &&
                type.ref < static_cast<int>(result.atab.size()))
                return std::max(1, result.atab[type.ref].size);
            if (type.kind == TypeKind::Record && type.ref >= 0 &&
                type.ref < static_cast<int>(result.btab.size()))
                return std::max(1, result.btab[type.ref].vsze);
            return 1;
        }

    private:
        SemanticResult &result;
        std::vector<int> display;
        int level = 0;
        std::unordered_map<int, TypeInfo> symbolTypes;
    };

    class SemanticAnalyzer
    {
    public:
        SemanticAnalyzer() : symbols(result) {}

        SemanticResult analyze(const std::shared_ptr<ParseNode> &root)
        {
            initPredefined();
            result.ast = visitProgram(root);
            return std::move(result);
        }

    private:
        SemanticResult result;
        SymbolTable symbols;
        int nextAddress = 0;
        std::vector<int> functionStack;
        std::unordered_map<int, std::vector<ParamInfo>> subprogramParams;
        std::set<int> functionsWithReturn;

        static std::shared_ptr<SemanticNode> makeNode(const std::string &label)
        {
            return std::make_shared<SemanticNode>(label);
        }

        static std::shared_ptr<SemanticNode> annotated(const std::string &label,
                                                       const std::vector<std::string> &annotations)
        {
            auto node = makeNode(label);
            node->annotations = annotations;
            return node;
        }

        void error(const std::string &message)
        {
            result.errors.push_back(message);
        }

        void initPredefined()
        {
            static const char *reserved[] = {
                "and", "array", "begin", "case", "const", "div", "downto", "do",
                "else", "end", "for", "function", "if", "mod", "not", "of",
                "or", "procedure", "program", "record", "repeat", "then", "to",
                "type", "until", "var", "while"};

            for (const char *word : reserved)
                symbols.declare(word, "reserved", primitive(TypeKind::Unknown), true);

            symbols.declare("real", "type", primitive(TypeKind::Real), true);
            symbols.declare("integer", "type", primitive(TypeKind::Integer), true);
            symbols.declare("char", "type", primitive(TypeKind::Char), true);
            symbols.declare("boolean", "type", primitive(TypeKind::Boolean), true);
            symbols.declare("string", "type", primitive(TypeKind::String), true);

            int trueIdx = symbols.declare("true", "constant", primitive(TypeKind::Boolean), true);
            result.tab[trueIdx].adr = 1;
            int falseIdx = symbols.declare("false", "constant", primitive(TypeKind::Boolean), true);
            result.tab[falseIdx].adr = 0;

            symbols.declare("readln", "procedure", primitive(TypeKind::Void), true);
            symbols.declare("write", "procedure", primitive(TypeKind::Void), true);
            symbols.declare("writeln", "procedure", primitive(TypeKind::Void), true);
        }

        std::shared_ptr<SemanticNode> visitProgram(const std::shared_ptr<ParseNode> &node)
        {
            if (!node || node->children.size() < 3)
                return makeNode("ProgramNode(name: '<unknown>')");

            std::string name = ParseTreeView::value(node->children[0]->children.size() > 1
                                                        ? node->children[0]->children[1]
                                                        : nullptr);

            std::string programName = name.empty() ? "<program>" : name;

            auto ast = makeNode("ProgramNode(name: '" + programName + "')");

            int idx = declare(programName, "program", primitive(TypeKind::Void), true);
            ast->annotations = {"tab_index:" + std::to_string(idx), "lev:0"};

            ast->children.push_back(visitDeclarationPart(node->children[1]));

            int mainBlock = symbols.pushBlock();
            auto main = visitCompoundStatement(node->children[2]);
            main->annotations.push_back("block_index:" + std::to_string(mainBlock));
            main->annotations.push_back("lev:" + std::to_string(symbols.lexicalLevel()));
            ast->children.push_back(main);
            symbols.popBlock();

            return ast;
        }

        std::shared_ptr<SemanticNode> visitDeclarationPart(const std::shared_ptr<ParseNode> &node)
        {
            auto ast = makeNode("Declarations");
            if (!node)
                return ast;

            for (const auto &child : node->children)
            {
                if (ParseTreeView::node(child, "<const-declaration>"))
                    ast->children.push_back(visitConstDeclaration(child));
                else if (ParseTreeView::node(child, "<type-declaration>"))
                    ast->children.push_back(visitTypeDeclaration(child));
                else if (ParseTreeView::node(child, "<var-declaration>"))
                    ast->children.push_back(visitVarDeclaration(child));
                else if (ParseTreeView::node(child, "<subprogram-declaration>"))
                    ast->children.push_back(visitSubprogramDeclaration(child));
            }
            return ast;
        }

        std::shared_ptr<SemanticNode> visitConstDeclaration(const std::shared_ptr<ParseNode> &node)
        {
            auto ast = makeNode("ConstDecls");
            for (size_t i = 1; i + 2 < node->children.size(); i += 4)
            {
                std::string name = ParseTreeView::value(node->children[i]);
                ExprInfo value = constantInfo(node->children[i + 2]);
                int idx = declare(name, "constant", value.type, true);
                if (value.constant)
                {
                    result.tab[idx].adr = value.intValue;
                    if (value.type.kind == TypeKind::Real && !value.stringValue.empty())
                    {
                        try { result.tab[idx].realValue = std::stod(value.stringValue); } catch (...) {}
                        if (value.node)
                            for (const auto &ann : value.node->annotations)
                                if (ann == "unary:-") { result.tab[idx].realValue = -result.tab[idx].realValue; break; }
                    }
                }
                auto c = makeNode("ConstDecl(" + name + ")");
                c->annotations = {"tab_index:" + std::to_string(idx),
                                  "type:" + value.type.name,
                                  "lev:" + std::to_string(symbols.lexicalLevel())};
                if (value.constant)
                    c->annotations.push_back("value:" + literalText(value));
                ast->children.push_back(c);
            }
            return ast;
        }

        std::shared_ptr<SemanticNode> visitTypeDeclaration(const std::shared_ptr<ParseNode> &node)
        {
            auto ast = makeNode("TypeDecls");
            for (size_t i = 1; i + 2 < node->children.size(); i += 4)
            {
                std::string name = ParseTreeView::value(node->children[i]);
                TypeInfo type = resolveType(node->children[i + 2], name);
                type.anonymous = false;
                type.name = type.kind == TypeKind::Subrange ? name : type.name;
                int idx = declare(name, "type", type, true);
                symbols.setType(idx, type);
                auto t = makeNode("TypeDecl(" + name + ")");
                t->annotations = typeAnnotations(idx, type);
                ast->children.push_back(t);
            }
            return ast;
        }

        std::shared_ptr<SemanticNode> visitVarDeclaration(const std::shared_ptr<ParseNode> &node)
        {
            auto ast = makeNode("VarDecls");

            for (size_t i = 1; i + 2 < node->children.size(); i += 4)
            {
                std::vector<std::string> names = identifierList(node->children[i]);
                TypeInfo type = resolveType(node->children[i + 2], "");

                for (const auto &name : names)
                {
                    int idx = declare(name, "variable", type, false);

                    auto var = makeNode("VarDecl(" + name + ")");

                    if (idx == 0)
                    {
                        var->annotations = {
                            "tab_index:0",
                            "type:" + type.name,
                            "ref:" + std::to_string(type.ref),
                            "lev:" + std::to_string(symbols.lexicalLevel()),
                            "error:redeclaration"};

                        ast->children.push_back(var);
                        continue;
                    }

                    int size = symbols.sizeOf(type);
                    int currentBlock = symbols.currentBlock();

                    result.tab[idx].adr = result.btab[currentBlock].vsze + result.btab[currentBlock].psze;
                    nextAddress += size;
                    result.btab[currentBlock].vsze += size;

                    var->annotations = typeAnnotations(idx, type);
                    ast->children.push_back(var);
                }
            }

            return ast;
        }

        std::shared_ptr<SemanticNode> visitSubprogramDeclaration(const std::shared_ptr<ParseNode> &node)
        {
            if (!node || node->children.empty())
                return makeNode("Subprogram");

            auto decl = node->children[0];
            bool function = ParseTreeView::node(decl, "<function-declaration>");
            std::string name = ParseTreeView::value(decl->children.size() > 1 ? decl->children[1] : nullptr);
            TypeInfo returnType = function ? functionReturnType(decl) : primitive(TypeKind::Void);

            int blockRef = static_cast<int>(result.btab.size());
            int subIdx = declare(name, function ? "function" : "procedure", returnType, true);
            result.tab[subIdx].ref = blockRef;

            int block = symbols.pushBlock();
            if (function)
            {
                functionStack.push_back(subIdx);
            }

            auto ast = makeNode(function ? "FunctionDecl(" + name + ")" : "ProcedureDecl(" + name + ")");
            ast->annotations = {"tab_index:" + std::to_string(subIdx),
                                "block_index:" + std::to_string(block),
                                "lev:" + std::to_string(symbols.lexicalLevel() - 1),
                                "type:" + returnType.name};

            for (const auto &child : decl->children)
            {
                if (ParseTreeView::node(child, "<formal-parameter-list>"))
                    ast->children.push_back(visitFormalParameters(child, subIdx));
                else if (ParseTreeView::node(child, "<block>"))
                {
                    if (function)
                    {
                        int resIdx = symbols.declare(name, "function-result", returnType, false);
                        result.tab[resIdx].adr = result.btab[block].vsze + result.btab[block].psze;
                        result.btab[block].vsze += symbols.sizeOf(returnType);
                    }
                    ast->children.push_back(visitBlock(child));
                }
            }

            if (function)
            {
                if (!functionsWithReturn.count(subIdx))
                    error("function '" + name + "' may not assign a return value");
                functionStack.pop_back();
            }
            symbols.popBlock();
            return ast;
        }

        std::shared_ptr<SemanticNode> visitFormalParameters(const std::shared_ptr<ParseNode> &node, int ownerTab)
        {
            auto ast = makeNode("Parameters");
            std::vector<ParamInfo> params;
            std::vector<std::string> paramTypes;
            for (const auto &group : node->children)
            {
                if (!ParseTreeView::node(group, "<parameter-group>"))
                    continue;
                std::vector<std::string> names = identifierList(group->children[0]);
                TypeInfo type = resolveParameterType(group);
                for (const auto &name : names)
                {
                    int idx = declare(name, "parameter", type, true);
                    int pSize = symbols.sizeOf(type);
                    result.tab[idx].adr = result.btab[symbols.currentBlock()].psze;
                    result.btab[symbols.currentBlock()].psze += pSize;
                    result.btab[symbols.currentBlock()].lpar = idx;
                    params.push_back({name, type, idx});
                    paramTypes.push_back(type.name);

                    auto param = makeNode("Param(" + name + ")");
                    param->annotations = typeAnnotations(idx, type);
                    ast->children.push_back(param);
                }
            }
            subprogramParams[ownerTab] = params;
            result.tab[ownerTab].params = paramTypes;
            return ast;
        }

        std::shared_ptr<SemanticNode> visitBlock(const std::shared_ptr<ParseNode> &node)
        {
            auto ast = makeNode("Block");
            ast->annotations = {"block_index:" + std::to_string(symbols.currentBlock()),
                                "lev:" + std::to_string(symbols.lexicalLevel())};
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
                if (ParseTreeView::node(child, "<statement-list>"))
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
                auto statement = visitStatement(child);
                if (statement)
                    ast->children.push_back(statement);
            }
            return ast;
        }

        std::shared_ptr<SemanticNode> visitStatement(const std::shared_ptr<ParseNode> &node)
        {
            if (!node || ParseTreeView::node(node, "<statement>") || ParseTreeView::terminal(node, "semicolon"))
                return nullptr;
            if (ParseTreeView::node(node, "<assignment-statement>"))
                return visitAssignment(node);
            if (ParseTreeView::node(node, "<procedure/function-call>"))
                return visitCall(node).node;
            if (ParseTreeView::node(node, "<if-statement>"))
                return visitIf(node);
            if (ParseTreeView::node(node, "<case-statement>"))
                return visitCase(node);
            if (ParseTreeView::node(node, "<while-statement>"))
                return visitWhile(node);
            if (ParseTreeView::node(node, "<repeat-statement>"))
                return visitRepeat(node);
            if (ParseTreeView::node(node, "<for-statement>"))
                return visitFor(node);
            if (ParseTreeView::node(node, "<compound-statement>"))
                return visitCompoundStatement(node);
            return nullptr;
        }

        std::shared_ptr<SemanticNode> visitAssignment(const std::shared_ptr<ParseNode> &node)
        {
            ExprInfo target = variableInfo(node->children[0], false);
            ExprInfo value = expressionInfo(node->children[2]);

            if (!assignmentCompatible(target.type, value))
                error("assignment incompatible: cannot assign " + value.type.name + " to " + target.type.name);

            if (!functionStack.empty() && target.tabIndex > 0)
            {
                int currentFunction = functionStack.back();
                if (result.tab[target.tabIndex].identifier == result.tab[currentFunction].identifier)
                    functionsWithReturn.insert(currentFunction);
            }

            if (target.tabIndex > 0)
                symbols.markInitialized(target.tabIndex);

            auto ast = makeNode("Assign");
            ast->annotations.push_back("type:void");

            auto targetNode = makeNode("Target");
            targetNode->children.push_back(target.node);

            auto valueNode = makeNode("Value");
            valueNode->children.push_back(value.node);

            ast->children.push_back(targetNode);
            ast->children.push_back(valueNode);

            return ast;
        }

        ExprInfo visitCall(const std::shared_ptr<ParseNode> &node)
        {
            std::string name = ParseTreeView::value(node->children.empty() ? nullptr : node->children[0]);
            int idx = symbols.lookupCallable(name);

            if (!idx)
                error("undeclared procedure/function: " + name);
            else if (result.tab[idx].obj != "procedure" && result.tab[idx].obj != "function")
                error("'" + name + "' is not callable");

            auto ast = makeNode("Call(" + name + ")");
            ast->annotations = {
                "tab_index:" + std::to_string(idx),
                "lev:" + std::to_string(symbols.lexicalLevel())};

            std::vector<ExprInfo> args;
            auto argsNode = makeNode("Args");

            for (const auto &child : node->children)
            {
                if (ParseTreeView::node(child, "<parameter-list>"))
                {
                    for (const auto &arg : child->children)
                    {
                        if (ParseTreeView::node(arg, "<expression>"))
                        {
                            args.push_back(expressionInfo(arg));
                            argsNode->children.push_back(args.back().node);
                        }
                    }
                }
            }

            if (!argsNode->children.empty())
                ast->children.push_back(argsNode);

            validateCallArguments(name, idx, args);

            TypeInfo returnType = idx ? symbols.typeOf(idx) : unknownType();
            return {returnType, ast, idx, false, 0, ""};
        }

        std::shared_ptr<SemanticNode> visitIf(const std::shared_ptr<ParseNode> &node)
        {
            auto ast = makeNode("If");
            ExprInfo cond = expressionInfo(node->children[1]);
            requireBoolean(cond, "if condition");
            ast->children.push_back(cond.node);
            for (size_t i = 3; i < node->children.size(); ++i)
            {
                if (ParseTreeView::terminal(node->children[i], "elsesy"))
                    continue;
                auto statement = visitStatement(node->children[i]);
                if (statement)
                    ast->children.push_back(statement);
            }
            return ast;
        }

        std::shared_ptr<SemanticNode> visitCase(const std::shared_ptr<ParseNode> &node)
        {
            auto ast = makeNode("Case");
            ExprInfo selector = expressionInfo(node->children[1]);
            if (!selector.type.isOrdinal() && !selector.type.isUnknown())
                error("case selector must be ordinal, got " + selector.type.name);
            ast->children.push_back(selector.node);
            for (const auto &child : node->children)
                if (ParseTreeView::node(child, "<case-block>"))
                    ast->children.push_back(visitCaseBlock(child, selector.type));
            return ast;
        }

        std::shared_ptr<SemanticNode> visitCaseBlock(const std::shared_ptr<ParseNode> &node, const TypeInfo &selectorType)
        {
            auto ast = makeNode("CaseBlock");
            for (const auto &child : node->children)
            {
                if (ParseTreeView::node(child, "<constant>"))
                {
                    ExprInfo label = constantInfo(child);
                    if (!compatible(selectorType, label.type))
                        error("case label type " + label.type.name + " is incompatible with selector " + selectorType.name);
                    ast->children.push_back(label.node);
                }
                else if (ParseTreeView::node(child, "<case-block>"))
                {
                    ast->children.push_back(visitCaseBlock(child, selectorType));
                }
                else
                {
                    auto stmt = visitStatement(child);
                    if (stmt)
                        ast->children.push_back(stmt);
                }
            }
            return ast;
        }

        std::shared_ptr<SemanticNode> visitWhile(const std::shared_ptr<ParseNode> &node)
        {
            auto ast = makeNode("While");
            ExprInfo cond = expressionInfo(node->children[1]);
            requireBoolean(cond, "while condition");
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
            requireBoolean(cond, "repeat condition");
            ast->children.push_back(cond.node);
            return ast;
        }

        std::shared_ptr<SemanticNode> visitFor(const std::shared_ptr<ParseNode> &node)
        {
            auto ast = makeNode("For");
            ExprInfo var = identifierInfo(ParseTreeView::value(node->children[1]));
            ExprInfo start = expressionInfo(node->children[3]);
            ExprInfo finish = expressionInfo(node->children[5]);
            if (!var.type.isOrdinal() && !var.type.isUnknown())
                error("for variable must be ordinal, got " + var.type.name);
            if (!compatible(var.type, start.type) || !compatible(var.type, finish.type))
                error("for range type is incompatible with variable " + ParseTreeView::value(node->children[1]));
            std::string direction = ParseTreeView::tag(node->children[4]);
            ast->annotations.push_back("direction:" + direction);
            ast->children = {var.node, start.node, finish.node};
            if (node->children.size() > 7)
                ast->children.push_back(visitStatement(node->children[7]));
            return ast;
        }

        int declare(const std::string &name, const std::string &obj, const TypeInfo &type,
                    bool initialized, std::vector<std::string> params = {})
        {
            if (symbols.lookupCurrent(name))
            {
                error("redeclaration in same scope: " + name);
                return 0;
            }

            return symbols.declare(name, obj, type, initialized, params);
        }

        TypeInfo functionReturnType(const std::shared_ptr<ParseNode> &decl)
        {
            for (size_t i = 0; i + 1 < decl->children.size(); ++i)
                if (ParseTreeView::terminal(decl->children[i], "colon") &&
                    ParseTreeView::terminal(decl->children[i + 1], "ident"))
                    return typeFromIdentifier(ParseTreeView::value(decl->children[i + 1]));
            return unknownType();
        }

        TypeInfo typeFromIdentifier(const std::string &name)
        {
            int idx = symbols.lookup(name);
            if (!idx)
            {
                error("unknown type: " + name);
                return unknownType();
            }
            if (result.tab[idx].obj != "type")
                error(name + " is not a type identifier");
            return symbols.typeOf(idx);
        }

        TypeInfo resolveParameterType(const std::shared_ptr<ParseNode> &group)
        {
            for (const auto &child : group->children)
            {
                if (ParseTreeView::node(child, "<array-type>"))
                    return resolveArrayType(child, "");
                if (ParseTreeView::terminal(child, "ident"))
                    return typeFromIdentifier(ParseTreeView::value(child));
            }
            return unknownType();
        }

        TypeInfo resolveType(const std::shared_ptr<ParseNode> &node, const std::string &declaredName)
        {
            if (!node)
                return unknownType();
            if (ParseTreeView::node(node, "<type>") && !node->children.empty())
                return resolveType(node->children[0], declaredName);
            if (ParseTreeView::terminal(node, "ident"))
                return typeFromIdentifier(ParseTreeView::value(node));
            if (ParseTreeView::node(node, "<array-type>"))
                return resolveArrayType(node, declaredName);
            if (ParseTreeView::node(node, "<record-type>"))
                return resolveRecordType(node, declaredName);
            if (ParseTreeView::node(node, "<range>"))
                return resolveRangeType(node, declaredName);
            if (ParseTreeView::node(node, "<enumerated>"))
                return resolveEnumeratedType(node, declaredName);
            return unknownType();
        }

        TypeInfo resolveArrayType(const std::shared_ptr<ParseNode> &node, const std::string &declaredName)
        {
            TypeInfo indexType = primitive(TypeKind::Integer);
            int low = 0;
            int high = 0;
            bool hasRange = false;
            TypeInfo elemType = unknownType();

            for (const auto &child : node->children)
            {
                if (ParseTreeView::node(child, "<range>"))
                {
                    indexType = resolveRangeType(child, "");
                    low = indexType.low;
                    high = indexType.high;
                    hasRange = true;
                }
                else if (ParseTreeView::terminal(child, "ident") && elemType.isUnknown())
                {
                    indexType = typeFromIdentifier(ParseTreeView::value(child));
                }
                else if (ParseTreeView::node(child, "<type>"))
                {
                    elemType = resolveType(child, "");
                }
            }

            if (indexType.kind == TypeKind::Real || indexType.base == TypeKind::Real)
                error("array index type cannot be real");
            if (!indexType.isOrdinal() && !indexType.isUnknown())
                error("array index type must be ordinal, got " + indexType.name);

            int elementSize = symbols.sizeOf(elemType);
            int ref = symbols.addArray(indexType, elemType, low, high, elementSize);
            TypeInfo type;
            type.kind = TypeKind::Array;
            type.base = TypeKind::Array;
            type.name = "array";
            type.ref = ref;
            type.low = hasRange ? low : 0;
            type.high = hasRange ? high : 0;
            type.anonymous = declaredName.empty();
            return type;
        }

        TypeInfo resolveRecordType(const std::shared_ptr<ParseNode> &node, const std::string &declaredName)
        {
            int block = symbols.addRecordBlock();
            symbols.pushExistingBlock(block);
            int savedAddress = nextAddress;
            nextAddress = 0;
            for (const auto &child : node->children)
                if (ParseTreeView::node(child, "<field-list>"))
                    declareFields(child);
            nextAddress = savedAddress;
            symbols.popBlock();

            TypeInfo type;
            type.kind = TypeKind::Record;
            type.base = TypeKind::Record;
            type.name = "record";
            type.ref = block;
            type.anonymous = declaredName.empty();
            return type;
        }

        void declareFields(const std::shared_ptr<ParseNode> &node)
        {
            if (ParseTreeView::node(node, "<field-part>"))
            {
                std::vector<std::string> names = identifierList(node->children[0]);
                TypeInfo type = resolveType(node->children[2], "");
                for (const auto &name : names)
                {
                    int idx = declare(name, "field", type, true);
                    result.tab[idx].adr = result.btab[symbols.currentBlock()].vsze;
                    result.btab[symbols.currentBlock()].vsze += symbols.sizeOf(type);
                }
                return;
            }
            for (const auto &child : node->children)
                declareFields(child);
        }

        TypeInfo resolveRangeType(const std::shared_ptr<ParseNode> &range, const std::string &declaredName)
        {
            ExprInfo left = expressionInfo(range->children[0]);
            ExprInfo right = expressionInfo(range->children[3]);
            if (!compatible(left.type, right.type))
                error("range bounds must have compatible types");
            if (left.type.kind == TypeKind::Real || left.type.base == TypeKind::Real)
                error("subrange cannot use real bounds");
            if (!left.constant || !right.constant)
                error("range bounds must be constant");
            if (left.constant && right.constant && left.intValue > right.intValue)
                error("range lower bound is greater than upper bound");

            TypeInfo type;
            type.kind = TypeKind::Subrange;
            type.base = baseKind(left.type);
            type.name = declaredName.empty() ? "subrange" : declaredName;
            type.low = left.intValue;
            type.high = right.intValue;
            type.anonymous = declaredName.empty();
            return type;
        }

        TypeInfo resolveEnumeratedType(const std::shared_ptr<ParseNode> &node, const std::string &declaredName)
        {
            TypeInfo type;
            type.kind = TypeKind::Enumerated;
            type.base = TypeKind::Enumerated;
            type.name = declaredName.empty() ? "enumerated" : declaredName;
            type.low = 0;
            int value = 0;
            std::set<std::string> seen;
            for (const auto &child : node->children)
            {
                if (!ParseTreeView::terminal(child, "ident"))
                    continue;
                std::string name = ParseTreeView::value(child);
                if (!seen.insert(lower(name)).second)
                    error("duplicate enumerated value: " + name);
                int idx = declare(name, "constant", type, true);
                result.tab[idx].adr = value++;
            }
            type.high = std::max(0, value - 1);
            return type;
        }

        ExprInfo expressionInfo(const std::shared_ptr<ParseNode> &node)
        {
            if (!node)
                return {unknownType(), makeNode("Expr(?)"), 0, false, 0, ""};
            if (ParseTreeView::node(node, "<expression>"))
            {
                ExprInfo left = expressionInfo(node->children[0]);
                if (node->children.size() == 1)
                    return left;
                ExprInfo right = expressionInfo(node->children[2]);
                std::string op = ParseTreeView::tag(node->children[1]);
                if (!compatible(left.type, right.type))
                    error("relational operands are incompatible: " + left.type.name + " " + op + " " + right.type.name);
                auto bin = makeNode("BinOp(" + op + ")");
                bin->annotations.push_back("type:boolean");
                bin->children = {left.node, right.node};
                return {primitive(TypeKind::Boolean), bin, 0, false, 0, ""};
            }
            if (ParseTreeView::node(node, "<simple-expression>"))
                return foldExpression(node);
            if (ParseTreeView::node(node, "<term>"))
                return foldExpression(node);
            if (ParseTreeView::node(node, "<factor>"))
                return factorInfo(node);
            return constantInfo(node);
        }

        ExprInfo foldExpression(const std::shared_ptr<ParseNode> &node)
        {
            size_t pos = 0;
            bool unaryMinus = false;
            if (!node->children.empty() &&
                (ParseTreeView::terminal(node->children[0], "plus") ||
                 ParseTreeView::terminal(node->children[0], "minus")))
            {
                unaryMinus = ParseTreeView::terminal(node->children[0], "minus");
                pos = 1;
            }

            ExprInfo acc = expressionInfo(node->children[pos++]);
            if (unaryMinus)
            {
                if (!acc.type.isNumeric() && !acc.type.isUnknown())
                    error("unary minus requires numeric operand, got " + acc.type.name);
                acc.node->annotations.push_back("unary:-");
                if (acc.constant)
                    acc.intValue = -acc.intValue;
            }

            while (pos + 1 < node->children.size())
            {
                std::string op = ParseTreeView::tag(node->children[pos]);
                ExprInfo rhs = expressionInfo(node->children[pos + 1]);
                TypeInfo resultType = operationType(op, acc.type, rhs.type);
                auto bin = makeNode("BinOp(" + op + ")");
                bin->annotations.push_back("type:" + resultType.name);
                bin->children = {acc.node, rhs.node};
                acc = {resultType, bin, 0, false, 0, ""};
                pos += 2;
            }
            return acc;
        }

        TypeInfo operationType(const std::string &op, const TypeInfo &left, const TypeInfo &right)
        {
            if (op == "orsy" || op == "andsy")
            {
                if (left.kind != TypeKind::Boolean && !left.isUnknown())
                    error("operator '" + op + "' requires boolean operands, got " + left.name);
                if (right.kind != TypeKind::Boolean && !right.isUnknown())
                    error("operator '" + op + "' requires boolean operands, got " + right.name);
                return primitive(TypeKind::Boolean);
            }

            if (op == "idiv" || op == "imod")
            {
                if (baseKind(left) != TypeKind::Integer && !left.isUnknown())
                    error("operator '" + op + "' requires integer operands, got " + left.name);
                if (baseKind(right) != TypeKind::Integer && !right.isUnknown())
                    error("operator '" + op + "' requires integer operands, got " + right.name);
                return primitive(TypeKind::Integer);
            }

            if (op == "rdiv")
            {
                requireNumeric(left, "operator '/'");
                requireNumeric(right, "operator '/'");
                return primitive(TypeKind::Real);
            }

            requireNumeric(left, "operator '" + op + "'");
            requireNumeric(right, "operator '" + op + "'");
            if (baseKind(left) == TypeKind::Real || baseKind(right) == TypeKind::Real)
                return primitive(TypeKind::Real);
            if (left.isUnknown() || right.isUnknown())
                return unknownType();
            return primitive(TypeKind::Integer);
        }

        ExprInfo factorInfo(const std::shared_ptr<ParseNode> &node)
        {
            if (node->children.empty())
                return {unknownType(), makeNode("Factor(?)"), 0, false, 0, ""};
            if (ParseTreeView::terminal(node->children[0], "notsy"))
            {
                ExprInfo operand = expressionInfo(node->children[1]);
                requireBoolean(operand, "not operand");
                auto ast = makeNode("Not");
                ast->annotations.push_back("type:boolean");
                ast->children.push_back(operand.node);
                return {primitive(TypeKind::Boolean), ast, 0, false, 0, ""};
            }
            if (ParseTreeView::terminal(node->children[0], "lparent") && node->children.size() > 1)
                return expressionInfo(node->children[1]);
            if (ParseTreeView::node(node->children[0], "<variable>"))
                return variableInfo(node->children[0], true);
            if (ParseTreeView::node(node->children[0], "<procedure/function-call>"))
                return visitCall(node->children[0]);
            return constantInfo(node->children[0]);
        }

        ExprInfo constantInfo(const std::shared_ptr<ParseNode> &node)
        {
            if (!node)
                return {unknownType(), makeNode("Const(?)"), 0, false, 0, ""};
            if (ParseTreeView::node(node, "<constant>"))
            {
                int sign = 1;
                size_t valueIdx = 0;
                if (node->children.size() == 2)
                {
                    sign = ParseTreeView::terminal(node->children[0], "minus") ? -1 : 1;
                    valueIdx = 1;
                }
                ExprInfo value = constantInfo(node->children[valueIdx]);
                if (sign == -1)
                {
                    value.intValue *= -1;
                    value.node->annotations.push_back("unary:-");
                }
                return value;
            }

            std::string tag = ParseTreeView::tag(node);
            std::string value = ParseTreeView::value(node);
            if (tag == "intcon")
                return {primitive(TypeKind::Integer), annotated("Int(" + value + ")", {"type:integer"}),
                        0, true, std::stoi(value), value};
            if (tag == "realcon")
                return {primitive(TypeKind::Real), annotated("Real(" + value + ")", {"type:real"}),
                        0, true, 0, value};
            if (tag == "charcon")
            {
                int ordinal = value.empty() ? 0 : static_cast<unsigned char>(value[0]);
                return {primitive(TypeKind::Char), annotated("Char(" + value + ")", {"type:char"}),
                        0, true, ordinal, value};
            }
            if (tag == "string")
            {
                std::string raw = value;
                std::string content = raw;

                if (content.size() >= 2 && content.front() == '\'' && content.back() == '\'')
                    content = content.substr(1, content.size() - 2);

                TypeInfo type = primitive(TypeKind::String);
                type.stringLength = static_cast<int>(content.size());

                return {
                    type,
                    annotated("String(" + raw + ")", {"type:string",
                                                      "len:" + std::to_string(type.stringLength)}),
                    0,
                    true,
                    0,
                    content};
            }
            if (tag == "ident")
                return identifierInfo(value);
            return expressionInfo(node);
        }

        ExprInfo variableInfo(const std::shared_ptr<ParseNode> &node, bool warnUninitialized)
        {
            if (!node || node->children.empty())
                return {unknownType(), makeNode("Var(?)"), 0, false, 0, ""};

            ExprInfo base = identifierInfo(ParseTreeView::value(node->children[0]), warnUninitialized);
            int ownerTab = base.tabIndex;

            for (size_t i = 1; i < node->children.size(); ++i)
            {
                if (!ParseTreeView::node(node->children[i], "<component-variable>"))
                    continue;

                auto comp = node->children[i];
                if (!comp->children.empty() && ParseTreeView::terminal(comp->children[0], "lbrack"))
                {
                    std::vector<ExprInfo> indexes = collectIndexExpressions(comp);
                    if (indexes.empty())
                        indexes.push_back({unknownType(), makeNode("Index(?)"), 0, false, 0, ""});

                    for (const auto &idxExpr : indexes)
                    {
                        base.node->label += "[]";
                        auto idxNode = makeNode("Index");

                        if (base.type.kind != TypeKind::Array || base.type.ref < 0 ||
                            base.type.ref >= static_cast<int>(result.atab.size()))
                        {
                            error("indexing non-array variable");
                            base.type = unknownType();
                            idxNode->children.push_back(idxExpr.node);
                            base.node->children.push_back(idxNode);
                            refreshTypeAnnotation(base.node, base.type);
                            continue;
                        }

                        const ATabEntry &array = result.atab[base.type.ref];
                        TypeInfo expected = typeFromComposite(array.xtyp, 0);
                        if (!compatible(expected, idxExpr.type))
                            error("array index type " + idxExpr.type.name + " is incompatible with " + expected.name);
                        if (idxExpr.constant && (idxExpr.intValue < array.low || idxExpr.intValue > array.high))
                            error("array index " + std::to_string(idxExpr.intValue) + " out of bounds [" +
                                  std::to_string(array.low) + ".." + std::to_string(array.high) + "]");

                        idxNode->annotations.push_back("array_ref:" + std::to_string(base.type.ref));
                        idxNode->annotations.push_back("low:" + std::to_string(array.low));
                        idxNode->annotations.push_back("high:" + std::to_string(array.high));
                        idxNode->annotations.push_back("elsz:" + std::to_string(array.elsz));
                        idxNode->children.push_back(idxExpr.node);
                        base.node->children.push_back(idxNode);

                        TypeInfo elem = typeFromComposite(array.etyp, array.eref);
                        base.type = elem;
                        refreshTypeAnnotation(base.node, elem);
                    }
                }
                else if (!comp->children.empty() && ParseTreeView::terminal(comp->children[0], "period"))
                {
                    std::string fieldName = ParseTreeView::value(comp->children[1]);
                    base.node->label += "." + fieldName;
                    if (base.type.kind != TypeKind::Record)
                    {
                        error("field access on non-record variable");
                        base.type = unknownType();
                    }
                    else
                    {
                        int fieldIdx = symbols.lookupInBlock(base.type.ref, fieldName);
                        if (!fieldIdx)
                        {
                            error("unknown record field: " + fieldName);
                            base.type = unknownType();
                        }
                        else
                        {
                            auto fieldNode = makeNode("Field(" + fieldName + ")");
                            fieldNode->annotations.push_back("field_tab_index:" + std::to_string(fieldIdx));
                            fieldNode->annotations.push_back("offset:" + std::to_string(result.tab[fieldIdx].adr));
                            fieldNode->annotations.push_back("type:" + result.tab[fieldIdx].type);
                            base.node->children.push_back(fieldNode);
                            base.type = symbols.typeOf(fieldIdx);
                            base.tabIndex = ownerTab;
                        }
                    }
                    refreshTypeAnnotation(base.node, base.type);
                }
            }
            return base;
        }

        ExprInfo identifierInfo(const std::string &name, bool warnUninitialized = true)
        {
            int idx = symbols.lookup(name);
            if (!idx)
            {
                error("undeclared identifier: " + name);
                return {unknownType(), annotated("Var(" + name + ")", {"tab_index:0", "type:unknown", "lev:" + std::to_string(symbols.lexicalLevel())}),
                        0, false, 0, ""};
            }

            TypeInfo type = symbols.typeOf(idx);
            auto node = makeNode("Var(" + name + ")");
            node->annotations = {"tab_index:" + std::to_string(idx), "type:" + type.name,
                                 "lev:" + std::to_string(result.tab[idx].lev)};
            if (warnUninitialized && result.tab[idx].obj == "variable" && !symbols.isInitialized(idx))
                node->annotations.push_back("warning:possibly_uninitialized");
            bool constant = result.tab[idx].obj == "constant";
            return {type, node, idx, constant, result.tab[idx].adr, ""};
        }

        std::vector<ExprInfo> collectIndexExpressions(const std::shared_ptr<ParseNode> &component)
        {
            std::vector<ExprInfo> indexes;
            if (!component)
                return indexes;

            for (const auto &child : component->children)
            {
                if (!ParseTreeView::node(child, "<index-list>"))
                    continue;
                for (const auto &idxNode : child->children)
                {
                    if (ParseTreeView::terminal(idxNode, "comma"))
                        continue;
                    indexes.push_back(expressionInfo(idxNode));
                }
            }
            return indexes;
        }

        void validateCallArguments(const std::string &name, int idx, const std::vector<ExprInfo> &args)
        {
            if (!idx || name == "writeln" || name == "write" || name == "readln")
                return;
            auto it = subprogramParams.find(idx);
            if (it == subprogramParams.end())
                return;
            const auto &params = it->second;
            if (args.size() != params.size())
            {
                error("call to '" + name + "' expects " + std::to_string(params.size()) +
                      " argument(s), got " + std::to_string(args.size()));
                return;
            }
            for (size_t i = 0; i < args.size(); ++i)
                if (!assignmentCompatible(params[i].type, args[i]))
                    error("argument " + std::to_string(i + 1) + " of '" + name +
                          "' expects " + params[i].type.name + ", got " + args[i].type.name);
        }

        bool compatible(const TypeInfo &a, const TypeInfo &b) const
        {
            if (a.isUnknown() || b.isUnknown())
                return true;
            if (a.kind == TypeKind::String && b.kind == TypeKind::String)
                return a.stringLength < 0 || b.stringLength < 0 || a.stringLength == b.stringLength;
            if (a.kind == TypeKind::Array || b.kind == TypeKind::Array ||
                a.kind == TypeKind::Record || b.kind == TypeKind::Record)
                return a.kind == b.kind && a.ref == b.ref && !a.anonymous && !b.anonymous;
            if (a.kind == b.kind && a.ref == b.ref)
                return true;
            if (a.kind == TypeKind::Subrange || b.kind == TypeKind::Subrange)
                return baseKind(a) == baseKind(b);
            if ((baseKind(a) == TypeKind::Integer || baseKind(a) == TypeKind::Real) &&
                (baseKind(b) == TypeKind::Integer || baseKind(b) == TypeKind::Real))
                return true;
            return false;
        }

        bool assignmentCompatible(const TypeInfo &target, const ExprInfo &value) const
        {
            if (target.isUnknown() || value.type.isUnknown())
                return true;
            if (target.kind == TypeKind::Real && baseKind(value.type) == TypeKind::Integer)
                return true;
            if (!compatible(target, value.type))
                return false;
            if (target.kind == TypeKind::Subrange && value.constant)
                return value.intValue >= target.low && value.intValue <= target.high;
            return true;
        }

        void requireBoolean(const ExprInfo &expr, const std::string &context)
        {
            if (expr.type.kind != TypeKind::Boolean && !expr.type.isUnknown())
                error(context + " must be boolean, got " + expr.type.name);
        }

        void requireNumeric(const TypeInfo &type, const std::string &context)
        {
            if (!type.isNumeric() && !type.isUnknown())
                error(context + " requires numeric operand, got " + type.name);
        }

        TypeKind baseKind(const TypeInfo &type) const
        {
            return type.kind == TypeKind::Subrange ? type.base : type.kind;
        }

        TypeInfo typeFromComposite(const std::string &name, int ref) const
        {
            TypeKind primitiveType = primitiveKind(name);
            if (primitiveType != TypeKind::Unknown)
                return primitive(primitiveType);
            TypeInfo type;
            type.name = name;
            type.ref = ref;
            if (name == "array")
            {
                type.kind = TypeKind::Array;
                type.base = TypeKind::Array;
            }
            else if (name == "record")
            {
                type.kind = TypeKind::Record;
                type.base = TypeKind::Record;
            }
            else if (name == "subrange")
            {
                type.kind = TypeKind::Subrange;
                type.base = TypeKind::Integer;
            }
            else
            {
                type.kind = TypeKind::Enumerated;
                type.base = TypeKind::Enumerated;
            }
            return type;
        }

        void refreshTypeAnnotation(const std::shared_ptr<SemanticNode> &node, const TypeInfo &type)
        {
            bool foundType = false, foundRef = false;
            for (auto &annotation : node->annotations)
            {
                if (startsWith(annotation, "type:")) { annotation = "type:" + type.name; foundType = true; }
                else if (startsWith(annotation, "ref:")) { annotation = "ref:" + std::to_string(type.ref); foundRef = true; }
            }
            if (!foundType) node->annotations.push_back("type:" + type.name);
            if (!foundRef) node->annotations.push_back("ref:" + std::to_string(type.ref));
        }

        std::vector<std::string> identifierList(const std::shared_ptr<ParseNode> &node)
        {
            std::vector<std::string> names;
            if (!node)
                return names;
            for (const auto &child : node->children)
                if (ParseTreeView::terminal(child, "ident"))
                    names.push_back(ParseTreeView::value(child));
            return names;
        }

        std::vector<std::string> typeAnnotations(int tabIndex, const TypeInfo &type) const
        {
            std::vector<std::string> annotations = {"tab_index:" + std::to_string(tabIndex),
                                                    "type:" + type.name,
                                                    "ref:" + std::to_string(type.ref),
                                                    "lev:" + std::to_string(symbols.lexicalLevel())};
            if (type.kind == TypeKind::Subrange)
                annotations.push_back("range:" + std::to_string(type.low) + ".." + std::to_string(type.high));
            if (type.kind == TypeKind::String && type.stringLength >= 0)
                annotations.push_back("len:" + std::to_string(type.stringLength));
            return annotations;
        }

        std::string literalText(const ExprInfo &value) const
        {
            if (!value.stringValue.empty())
                return value.stringValue;
            return std::to_string(value.intValue);
        }
    };
} // namespace

SemanticResult analyzeSemantics(const std::shared_ptr<ParseNode> &root)
{
    SemanticAnalyzer analyzer;
    return analyzer.analyze(root);
}

void printSemanticResult(const SemanticResult &result, std::ostream &out)
{
    out << "=== DECORATED AST ===\n";
    SemanticTreePrinter::print(result.ast, out);

    out << "\n=== SYMBOL TABLE: tab ===\n";
    out << "idx  " << std::left << std::setw(24) << "identifier"
        << std::setw(16) << "obj" << std::setw(12) << "type"
        << "ref  nrm  lev  adr  link  init  params\n";
    out << "----------------------------------------------------------------------------\n";
    for (size_t i = 0; i < result.tab.size(); ++i)
    {
        const auto &e = result.tab[i];
        out << std::right << std::setw(3) << i << "  " << std::left
            << std::setw(24) << e.identifier << std::setw(16) << e.obj
            << std::setw(12) << e.type << std::right << std::setw(3) << e.ref
            << std::setw(5) << e.nrm << std::setw(5) << e.lev
            << std::setw(5) << e.adr << std::setw(6) << e.link
            << "  " << std::setw(4) << (e.initialized ? "yes" : "no") << "  ";
        for (size_t p = 0; p < e.params.size(); ++p)
        {
            if (p)
                out << ",";
            out << e.params[p];
        }
        out << '\n';
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
