#include "codegen.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace opr
{
    constexpr int NEG   = 1;
    constexpr int ADD   = 2;
    constexpr int SUB   = 3;
    constexpr int MUL   = 4;
    constexpr int DIV   = 5;
    constexpr int MOD   = 6;
    constexpr int EQL   = 7;
    constexpr int NEQ   = 8;
    constexpr int LSS   = 9;
    constexpr int GEQ   = 10;
    constexpr int GTR   = 11;
    constexpr int LEQ   = 12;
    constexpr int WRT   = 13;
    constexpr int WRTLN = 14;
}

namespace
{
static std::string getAnnotation(const SemanticNode &node, const std::string &key)
{
    const std::string prefix = key + ":";
    for (const auto &ann : node.annotations)
        if (ann.rfind(prefix, 0) == 0)
            return ann.substr(prefix.size());
    return "";
}

static int getAnnotationInt(const SemanticNode &node, const std::string &key, int defaultVal = 0)
{
    const std::string val = getAnnotation(node, key);
    if (val.empty()) return defaultVal;
    try { return std::stoi(val); }
    catch (...) { return defaultVal; }
}

static bool hasAnnotation(const SemanticNode &node, const std::string &annotation)
{
    return std::find(node.annotations.begin(), node.annotations.end(), annotation) != node.annotations.end();
}

static bool labelIs(const std::string &label, const std::string &exact)
{
    return label == exact;
}

static bool labelStartsWith(const std::string &label, const std::string &prefix)
{
    return label.rfind(prefix, 0) == 0;
}

static std::string labelValue(const std::string &label)
{
    const auto l = label.find('(');
    const auto r = label.rfind(')');
    if (l == std::string::npos || r == std::string::npos || r <= l) return "";
    return label.substr(l + 1, r - l - 1);
}

static std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static std::string unquote(std::string s)
{
    if (s.size() >= 2 && ((s.front() == '\'' && s.back() == '\'') ||
                          (s.front() == '"' && s.back() == '"')))
        return s.substr(1, s.size() - 2);
    return s;
}

static bool isStatementLabel(const std::string &lbl)
{
    return labelIs(lbl, "Assign") || labelStartsWith(lbl, "Call(") || labelIs(lbl, "Compound") ||
           labelIs(lbl, "Statements") || labelIs(lbl, "If") || labelIs(lbl, "While") ||
           labelIs(lbl, "Repeat") || labelIs(lbl, "For") || labelIs(lbl, "Case");
}

static int binOpToOpr(const std::string &op)
{
    if (op == "plus")  return opr::ADD;
    if (op == "minus") return opr::SUB;
    if (op == "times") return opr::MUL;
    if (op == "idiv")  return opr::DIV;
    if (op == "rdiv")  return opr::DIV;
    if (op == "imod")  return opr::MOD;
    if (op == "eql")   return opr::EQL;
    if (op == "neq")   return opr::NEQ;
    if (op == "lss")   return opr::LSS;
    if (op == "geq")   return opr::GEQ;
    if (op == "gtr")   return opr::GTR;
    if (op == "leq")   return opr::LEQ;
    return -1;
}

class CodeGenerator
{
public:
    explicit CodeGenerator(const SemanticResult &sem) : semantic(sem) {}

    CodeGenResult generate()
    {
        if (!semantic.ast)
            errors.push_back("ICG error: decorated AST is null");
        else if (!labelStartsWith(semantic.ast->label, "ProgramNode("))
            errors.push_back("ICG error: AST root is not a ProgramNode");
        else
            visitProgram(*semantic.ast);

        for (const auto &pending : pendingCalls)
        {
            const int instrIdx = pending.first;
            const int tabIdx = pending.second;
            auto it = funcAddresses.find(tabIdx);
            if (it != funcAddresses.end())
                instructions[instrIdx].operand = it->second;
            else
                errors.push_back("ICG error: unresolved function/procedure address for tab index " + std::to_string(tabIdx));
        }

        return {std::move(instructions), std::move(errors)};
    }

private:
    const SemanticResult &semantic;
    std::vector<Instruction> instructions;
    std::vector<std::string> errors;
    std::unordered_map<int, int> funcAddresses;
    std::vector<std::pair<int, int>> pendingCalls;
    int currentLevel = 0;

    int emit(const std::string &mnemonic, int level = 0, int operand = 0)
    {
        Instruction instr;
        instr.mnemonic = mnemonic;
        instr.level = level;
        instr.operand = operand;
        instructions.push_back(instr);
        return static_cast<int>(instructions.size()) - 1;
    }

    int emitLitInt(int value, const std::string &type = "integer")
    {
        int idx = emit("LIT", 0, value);
        instructions[idx].valueType = type;
        return idx;
    }

    int emitLitReal(double value)
    {
        int idx = emit("LIT", 0, 0);
        instructions[idx].valueType = "real";
        instructions[idx].realOperand = value;
        return idx;
    }

    int emitLitString(const std::string &value)
    {
        int idx = emit("LIT", 0, 0);
        instructions[idx].valueType = "string";
        instructions[idx].stringOperand = value;
        return idx;
    }

    void emitRet()
    {
        emit("RET", 0, 0);
    }

    int functionReturnOffset(int functionTabIdx) const
    {
        if (functionTabIdx <= 0 || functionTabIdx >= static_cast<int>(semantic.tab.size())) return 0;
        const TabEntry &fn = semantic.tab[functionTabIdx];
        if (fn.obj != "function") return 0;
        const int blockIdx = fn.ref;
        if (blockIdx < 0 || blockIdx >= static_cast<int>(semantic.btab.size())) return 0;
        int idx = semantic.btab[blockIdx].last;
        while (idx > 0 && idx < static_cast<int>(semantic.tab.size()))
        {
            const TabEntry &entry = semantic.tab[idx];
            if (entry.obj == "function-result" && entry.identifier == fn.identifier)
                return entry.adr + FRAME_OVERHEAD;
            idx = entry.link;
        }
        return 0;
    }

    void visitProgram(const SemanticNode &node)
    {
        int jmpMainIdx = emit("JMP", 0, 0);

        for (const auto &child : node.children)
            if (child && labelIs(child->label, "Declarations"))
                visitDeclarations(*child);

        instructions[jmpMainIdx].operand = static_cast<int>(instructions.size());

        const int vsze = (!semantic.btab.empty()) ? semantic.btab[0].vsze : 0;
        emit("INT", 0, vsze + FRAME_OVERHEAD);

        for (const auto &child : node.children)
            if (child && labelIs(child->label, "Compound"))
                visitCompound(*child);

        emitRet();
    }

    void visitDeclarations(const SemanticNode &node)
    {
        for (const auto &child : node.children)
            if (child && (labelStartsWith(child->label, "ProcedureDecl") || labelStartsWith(child->label, "FunctionDecl")))
                visitSubprogram(*child);
    }

    void visitSubprogram(const SemanticNode &node)
    {
        const int tabIdx = getAnnotationInt(node, "tab_index");
        if (tabIdx <= 0 || tabIdx >= static_cast<int>(semantic.tab.size())) return;

        funcAddresses[tabIdx] = static_cast<int>(instructions.size());
        const int btabIdx = getAnnotationInt(node, "block_index", -1);
        const int vsze = (btabIdx >= 0 && btabIdx < static_cast<int>(semantic.btab.size())) ? semantic.btab[btabIdx].vsze : 0;
        const int psze = (btabIdx >= 0 && btabIdx < static_cast<int>(semantic.btab.size())) ? semantic.btab[btabIdx].psze : 0;
        emit("INT", 0, vsze + psze + FRAME_OVERHEAD);

        int oldLevel = currentLevel;
        currentLevel = semantic.tab[tabIdx].lev + 1;

        for (const auto &child : node.children)
        {
            if (!child || !labelIs(child->label, "Block")) continue;
            for (const auto &bChild : child->children)
                if (bChild && labelIs(bChild->label, "Declarations"))
                    visitDeclarations(*bChild);
            for (const auto &bChild : child->children)
                if (bChild && labelIs(bChild->label, "Compound"))
                    visitCompound(*bChild);
        }

        emitRet();
        currentLevel = oldLevel;
    }

    void visitCompound(const SemanticNode &node)
    {
        for (const auto &child : node.children)
            if (child && labelIs(child->label, "Statements"))
                visitStatements(*child);
    }

    void visitStatements(const SemanticNode &node)
    {
        for (const auto &child : node.children)
            if (child) visitStatement(*child);
    }

    void visitStatement(const SemanticNode &node)
    {
        const std::string &lbl = node.label;
        if (labelIs(lbl, "Assign")) { visitAssign(node); return; }
        if (labelStartsWith(lbl, "Call(")) { visitCall(node); return; }
        if (labelIs(lbl, "Compound")) { visitCompound(node); return; }
        if (labelIs(lbl, "Statements")) { visitStatements(node); return; }
        if (labelIs(lbl, "If")) { visitIf(node); return; }
        if (labelIs(lbl, "While")) { visitWhile(node); return; }
        if (labelIs(lbl, "Repeat")) { visitRepeat(node); return; }
        if (labelIs(lbl, "For")) { visitFor(node); return; }
        if (labelIs(lbl, "Case")) { visitCase(node); return; }
        errors.push_back("ICG error: unknown statement node '" + lbl + "'");
    }

    void visitIf(const SemanticNode &node)
    {
        if (node.children.empty()) return;
        visitExpression(*node.children[0]);
        int jpcIdx = emit("JPC", 0, 0);
        if (node.children.size() > 1 && node.children[1]) visitStatement(*node.children[1]);
        if (node.children.size() > 2 && node.children[2])
        {
            int jmpIdx = emit("JMP", 0, 0);
            instructions[jpcIdx].operand = static_cast<int>(instructions.size());
            visitStatement(*node.children[2]);
            instructions[jmpIdx].operand = static_cast<int>(instructions.size());
        }
        else
            instructions[jpcIdx].operand = static_cast<int>(instructions.size());
    }

    void visitWhile(const SemanticNode &node)
    {
        if (node.children.size() < 2) return;
        int condIdx = static_cast<int>(instructions.size());
        visitExpression(*node.children[0]);
        int jpcIdx = emit("JPC", 0, 0);
        visitStatement(*node.children[1]);
        emit("JMP", 0, condIdx);
        instructions[jpcIdx].operand = static_cast<int>(instructions.size());
    }

    void visitRepeat(const SemanticNode &node)
    {
        if (node.children.size() < 2) return;
        int loopIdx = static_cast<int>(instructions.size());
        visitStatement(*node.children[0]);
        visitExpression(*node.children[1]);
        emit("JPC", 0, loopIdx);
    }

    void visitFor(const SemanticNode &node)
    {
        if (node.children.size() < 4)
        {
            errors.push_back("ICG error: malformed For node");
            return;
        }

        const SemanticNode &varNode = *node.children[0];
        const int tabIdx = getAnnotationInt(varNode, "tab_index");
        if (tabIdx <= 0 || tabIdx >= static_cast<int>(semantic.tab.size()))
        {
            errors.push_back("ICG error: unresolved for variable");
            return;
        }
        const TabEntry &entry = semantic.tab[tabIdx];
        const int levelDiff = currentLevel - entry.lev;
        const int addr = entry.adr + FRAME_OVERHEAD;
        const bool downTo = (getAnnotation(node, "direction") == "downtosy");

        visitExpression(*node.children[1]);
        emit("STO", levelDiff, addr);

        int loopStart = static_cast<int>(instructions.size());
        emit("LOD", levelDiff, addr);
        visitExpression(*node.children[2]);
        emit("OPR", 0, downTo ? opr::GEQ : opr::LEQ);
        int jpcIdx = emit("JPC", 0, 0);

        visitStatement(*node.children[3]);

        emit("LOD", levelDiff, addr);
        emitLitInt(1);
        emit("OPR", 0, downTo ? opr::SUB : opr::ADD);
        emit("STO", levelDiff, addr);
        emit("JMP", 0, loopStart);
        instructions[jpcIdx].operand = static_cast<int>(instructions.size());
    }

    void visitCase(const SemanticNode &node)
    {
        if (node.children.empty()) return;
        visitExpression(*node.children[0]); // selector tetap di stack sampai match/no-match
        std::vector<int> endJumps;
        for (std::size_t i = 1; i < node.children.size(); ++i)
            if (node.children[i] && labelIs(node.children[i]->label, "CaseBlock"))
                compileCaseBlock(*node.children[i], endJumps);
        emit("POP", 0, 0); // selector dibuang saat tidak ada label yang match
        int end = static_cast<int>(instructions.size());
        for (int idx : endJumps) instructions[idx].operand = end;
    }

    void compileCaseBlock(const SemanticNode &block, std::vector<int> &endJumps)
    {
        std::vector<const SemanticNode*> labels;
        const SemanticNode *stmt = nullptr;
        std::vector<const SemanticNode*> nested;

        for (const auto &child : block.children)
        {
            if (!child) continue;
            if (labelIs(child->label, "CaseBlock")) nested.push_back(child.get());
            else if (!stmt && !isStatementLabel(child->label)) labels.push_back(child.get());
            else if (!stmt) stmt = child.get();
        }

        for (const SemanticNode *label : labels)
        {
            emit("DUP", 0, 0);
            visitExpression(*label);
            emit("OPR", 0, opr::EQL);
            int nextIdx = emit("JPC", 0, 0);
            emit("POP", 0, 0); // hapus selector sebelum menjalankan branch
            if (stmt) visitStatement(*stmt);
            endJumps.push_back(emit("JMP", 0, 0));
            instructions[nextIdx].operand = static_cast<int>(instructions.size());
        }

        for (const SemanticNode *n : nested)
            compileCaseBlock(*n, endJumps);
    }

    void visitAssign(const SemanticNode &node)
    {
        const SemanticNode *valueNode = nullptr;
        const SemanticNode *targetNode = nullptr;
        for (const auto &child : node.children)
        {
            if (!child) continue;
            if (labelIs(child->label, "Value")) valueNode = child.get();
            else if (labelIs(child->label, "Target")) targetNode = child.get();
        }
        if (!valueNode || !targetNode || valueNode->children.empty() || targetNode->children.empty())
        {
            errors.push_back("ICG error: malformed Assign node");
            return;
        }

        const SemanticNode &target = *targetNode->children[0];
        if (hasSelectors(target))
        {
            visitAddress(target);
            visitExpression(*valueNode->children[0]);
            emit("STI", 0, 0);
            return;
        }

        visitExpression(*valueNode->children[0]);
        const int tabIdx = getAnnotationInt(target, "tab_index");
        if (tabIdx <= 0 || tabIdx >= static_cast<int>(semantic.tab.size()))
        {
            errors.push_back("ICG error: unresolved assignment target: " + target.label);
            return;
        }
        const TabEntry &entry = semantic.tab[tabIdx];
        emit("STO", currentLevel - entry.lev, entry.adr + FRAME_OVERHEAD);
    }

    void visitCall(const SemanticNode &node)
    {
        const std::string name = toLower(labelValue(node.label));
        if (name == "writeln") { visitWriteCall(node, true); return; }
        if (name == "write") { visitWriteCall(node, false); return; }
        if (name == "readln") { visitReadCall(node); return; }

        const int tabIdx = getAnnotationInt(node, "tab_index");
        if (tabIdx <= 0 || tabIdx >= static_cast<int>(semantic.tab.size()))
        {
            errors.push_back("ICG error: unresolved call to '" + name + "'");
            return;
        }

        int argCount = 0;
        for (const auto &child : node.children)
        {
            if (child && labelIs(child->label, "Args"))
            {
                for (const auto &arg : child->children)
                {
                    if (!arg) continue;
                    const std::string argType = getAnnotation(*arg, "type");
                    if (argType == "record")
                    {
                        const int btabRef = getAnnotationInt(*arg, "ref");
                        if (btabRef > 0 && btabRef < static_cast<int>(semantic.btab.size()))
                        {
                            const int vsze = semantic.btab[btabRef].vsze;
                            for (int f = 0; f < vsze; ++f)
                            {
                                visitAddress(*arg);
                                emitLitInt(f);
                                emit("OPR", 0, opr::ADD);
                                emit("LDI", 0, 0);
                                ++argCount;
                            }
                        }
                        else
                        {
                            visitExpression(*arg);
                            ++argCount;
                        }
                    }
                    else
                    {
                        visitExpression(*arg);
                        ++argCount;
                    }
                }
            }
        }

        const TabEntry &entry = semantic.tab[tabIdx];
        int calIdx = emit("CAL", currentLevel - entry.lev, 0);
        auto found = funcAddresses.find(tabIdx);
        if (found != funcAddresses.end()) instructions[calIdx].operand = found->second;
        else pendingCalls.push_back({calIdx, tabIdx});
        instructions[calIdx].argCount = argCount;
        instructions[calIdx].returnsValue = (entry.obj == "function");
        instructions[calIdx].returnOffset = functionReturnOffset(tabIdx);
    }

    void visitWriteCall(const SemanticNode &node, bool newline)
    {
        const SemanticNode *argsNode = nullptr;
        for (const auto &child : node.children)
            if (child && labelIs(child->label, "Args")) { argsNode = child.get(); break; }

        if (!argsNode || argsNode->children.empty())
        {
            if (newline) emit("OPR", 0, opr::WRTLN);
            return;
        }

        const std::size_t argc = argsNode->children.size();
        for (std::size_t i = 0; i < argc; ++i)
        {
            visitExpression(*argsNode->children[i]);
            const bool isLast = (i + 1 == argc);
            emit("OPR", 0, (newline && isLast) ? opr::WRTLN : opr::WRT);
        }
    }

    void visitReadCall(const SemanticNode &node)
    {
        const SemanticNode *argsNode = nullptr;
        for (const auto &child : node.children)
            if (child && labelIs(child->label, "Args")) { argsNode = child.get(); break; }
        if (!argsNode) return;

        for (const auto &arg : argsNode->children)
        {
            if (!arg || !labelStartsWith(arg->label, "Var("))
            {
                errors.push_back("ICG error: readln argument must be a variable");
                continue;
            }
            visitAddress(*arg);
            int idx = emit("RED", 0, 0);
            instructions[idx].valueType = getAnnotation(*arg, "type");
            if (instructions[idx].valueType.empty()) instructions[idx].valueType = "integer";
        }
    }

    bool hasSelectors(const SemanticNode &node) const
    {
        for (const auto &child : node.children)
            if (child && (labelIs(child->label, "Index") || labelStartsWith(child->label, "Field(")))
                return true;
        return false;
    }

    void visitAddress(const SemanticNode &node)
    {
        const int tabIdx = getAnnotationInt(node, "tab_index");
        if (tabIdx <= 0 || tabIdx >= static_cast<int>(semantic.tab.size()))
        {
            errors.push_back("ICG error: unresolved address for " + node.label);
            emitLitInt(0);
            return;
        }
        const TabEntry &entry = semantic.tab[tabIdx];
        emit("LDA", currentLevel - entry.lev, entry.adr + FRAME_OVERHEAD);

        for (const auto &child : node.children)
        {
            if (!child) continue;
            if (labelIs(child->label, "Index"))
            {
                if (child->children.empty())
                {
                    errors.push_back("ICG error: array index has no expression");
                    continue;
                }
                visitExpression(*child->children[0]);
                int idx = emit("IDX", getAnnotationInt(*child, "low"), getAnnotationInt(*child, "high"));
                instructions[idx].argCount = getAnnotationInt(*child, "elsz", 1);
                emit("OPR", 0, opr::ADD);
            }
            else if (labelStartsWith(child->label, "Field("))
            {
                emitLitInt(getAnnotationInt(*child, "offset"));
                emit("OPR", 0, opr::ADD);
            }
        }
    }

    void visitExpression(const SemanticNode &node)
    {
        const std::string &lbl = node.label;
        const bool unaryNeg = hasAnnotation(node, "unary:-");

        if (labelStartsWith(lbl, "Int("))
        {
            int val = 0;
            try { val = std::stoi(labelValue(lbl)); }
            catch (...) { errors.push_back("ICG error: bad integer literal: " + lbl); }
            emitLitInt(val);
        }
        else if (labelStartsWith(lbl, "Real("))
        {
            double dval = 0.0;
            try { dval = std::stod(labelValue(lbl)); }
            catch (...) { errors.push_back("ICG error: bad real literal: " + lbl); }
            emitLitReal(dval);
        }
        else if (labelStartsWith(lbl, "Char("))
        {
            const std::string ch = labelValue(lbl);
            emitLitInt(ch.empty() ? 0 : static_cast<unsigned char>(ch[0]), "char");
        }
        else if (labelStartsWith(lbl, "String("))
        {
            emitLitString(unquote(labelValue(lbl)));
        }
        else if (labelStartsWith(lbl, "Var("))
        {
            const int tabIdx = getAnnotationInt(node, "tab_index");
            if (tabIdx <= 0 || tabIdx >= static_cast<int>(semantic.tab.size()))
            {
                errors.push_back("ICG error: unresolved identifier: " + lbl);
                emitLitInt(0);
            }
            else
            {
                const TabEntry &entry = semantic.tab[tabIdx];
                if (entry.obj == "constant")
                {
                    if (entry.type == "real")
                        emitLitReal(entry.realValue);
                    else
                        emitLitInt(entry.adr, entry.type);
                }
                else if (hasSelectors(node))
                {
                    visitAddress(node);
                    emit("LDI", 0, 0);
                }
                else
                {
                    emit("LOD", currentLevel - entry.lev, entry.adr + FRAME_OVERHEAD);
                }
            }
        }
        else if (labelStartsWith(lbl, "BinOp("))
        {
            if (node.children.size() < 2)
                errors.push_back("ICG error: BinOp missing operands: " + lbl);
            else
            {
                visitExpression(*node.children[0]);
                visitExpression(*node.children[1]);
                const std::string op = labelValue(lbl);
                if (op == "andsy")
                    emit("OPR", 0, opr::MUL);
                else if (op == "orsy")
                {
                    emit("OPR", 0, opr::ADD);
                    emitLitInt(0);
                    emit("OPR", 0, opr::GTR);
                }
                else
                {
                    const int code = binOpToOpr(op);
                    if (code < 0) errors.push_back("ICG error: unknown binary operator: " + op);
                    else emit("OPR", 0, code);
                }
            }
        }
        else if (labelIs(lbl, "Not"))
        {
            if (!node.children.empty()) visitExpression(*node.children[0]);
            emitLitInt(0);
            emit("OPR", 0, opr::EQL);
            return;
        }
        else if (labelStartsWith(lbl, "Call("))
        {
            visitCall(node);
            return;
        }
        else
        {
            errors.push_back("ICG error: unknown expression node: " + lbl);
            emitLitInt(0);
        }

        if (unaryNeg) emit("OPR", 0, opr::NEG);
    }
};
}

CodeGenResult generateCode(const SemanticResult &semantic)
{
    CodeGenerator gen(semantic);
    return gen.generate();
}

void printCodeGenResult(const CodeGenResult &result, std::ostream &out)
{
    if (!result.errors.empty())
    {
        out << "=== ICG DIAGNOSTICS ===\n";
        for (const auto &err : result.errors) out << "  " << err << '\n';
        out << '\n';
    }

    out << "=== INTERMEDIATE CODE ===\n";
    for (std::size_t i = 0; i < result.instructions.size(); ++i)
    {
        const Instruction &instr = result.instructions[i];
        out << i << " " << instr.mnemonic;
        if (instr.mnemonic != "RET")
            out << " " << instr.level << " " << instr.operand;
        if (instr.mnemonic == "LIT" && instr.valueType == "real")
            out << " ; real " << instr.realOperand;
        else if (instr.mnemonic == "LIT" && instr.valueType == "string")
            out << " ; string \"" << instr.stringOperand << "\"";
        else if (instr.mnemonic == "RED")
            out << " ; " << instr.valueType;
        else if (instr.mnemonic == "IDX")
            out << " ; elsz " << instr.argCount;
        out << '\n';
    }
}
