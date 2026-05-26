#include "codegen.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <stdexcept>
#include <unordered_map>

// OPR codes sesuai konvensi IC.
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
} // namespace opr

namespace
{

// annotation helpers

static std::string getAnnotation(const SemanticNode &node, const std::string &key)
{
    const std::string prefix = key + ":";
    for (const auto &ann : node.annotations)
        if (ann.rfind(prefix, 0) == 0)
            return ann.substr(prefix.size());
    return "";
}

static int getAnnotationInt(const SemanticNode &node, const std::string &key,
                             int defaultVal = 0)
{
    const std::string val = getAnnotation(node, key);
    if (val.empty())
        return defaultVal;
    try
    {
        return std::stoi(val);
    }
    catch (...)
    {
        return defaultVal;
    }
}

static bool hasAnnotation(const SemanticNode &node, const std::string &annotation)
{
    return std::find(node.annotations.begin(), node.annotations.end(), annotation) !=
           node.annotations.end();
}

// label helpers

static bool labelIs(const std::string &label, const std::string &exact)
{
    return label == exact;
}

static bool labelStartsWith(const std::string &label, const std::string &prefix)
{
    return label.rfind(prefix, 0) == 0;
}

// "BinOp(plus)" → "plus",  "Int(42)" → "42"
static std::string labelValue(const std::string &label)
{
    const auto l = label.find('(');
    const auto r = label.rfind(')');
    if (l == std::string::npos || r == std::string::npos || r <= l)
        return "";
    return label.substr(l + 1, r - l - 1);
}

static std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Relational ops juga di-map di sini supaya P2 bisa langsung pakai tanpa ubah tabel ini.
static int binOpToOpr(const std::string &op)
{
    if (op == "plus")  return opr::ADD;
    if (op == "minus") return opr::SUB;
    if (op == "times") return opr::MUL;
    if (op == "idiv")  return opr::DIV;
    if (op == "rdiv")  return opr::DIV;
    if (op == "imod")  return opr::MOD;
    // relational — dipakai P2
    if (op == "eql")   return opr::EQL;
    if (op == "neq")   return opr::NEQ;
    if (op == "lss")   return opr::LSS;
    if (op == "geq")   return opr::GEQ;
    if (op == "gtr")   return opr::GTR;
    if (op == "leq")   return opr::LEQ;
    return -1;
}

// CodeGenerator

class CodeGenerator
{
public:
    explicit CodeGenerator(const SemanticResult &sem) : semantic(sem) {}

    CodeGenResult generate()
    {
        if (!semantic.ast)
        {
            errors.push_back("ICG error: decorated AST is null");
        }
        else if (!labelStartsWith(semantic.ast->label, "ProgramNode("))
        {
            errors.push_back("ICG error: AST root is not a ProgramNode");
        }
        else
        {
            visitProgram(*semantic.ast);
        }
        return {std::move(instructions), std::move(errors)};
    }

private:
    const SemanticResult &semantic;
    std::vector<Instruction> instructions;
    std::vector<std::string> errors;
    
    std::unordered_map<int, int> funcAddresses;
    int currentLevel = 0; // Melacak eksekusi level

    void emit(const std::string &mnemonic, int level, int operand)
    {
        instructions.push_back({mnemonic, level, operand});
    }

    void emitRet()
    {
        instructions.push_back({"RET", 0, 0});
    }

    // IC address = tab[idx].adr + FRAME_OVERHEAD
    int varAddress(int tabIndex) const
    {
        if (tabIndex <= 0 || tabIndex >= static_cast<int>(semantic.tab.size()))
            return FRAME_OVERHEAD;
        return semantic.tab[tabIndex].adr + FRAME_OVERHEAD;
    }

    void visitProgram(const SemanticNode &node)
    {
        // Lompat ke blok utama (main) karena subprogram akan di-generate sebelum main
        int jmpMainIdx = instructions.size();
        emit("JMP", 0, 0);

        // Visit Declarations untuk generate subprograms
        for (const auto &child : node.children)
        {
            if (child && labelIs(child->label, "Declarations"))
                visitDeclarations(*child);
        }

        // Backpatch JMP ke main block
        if (jmpMainIdx < static_cast<int>(instructions.size()))
            instructions[jmpMainIdx].operand = instructions.size();

        // Deklarasi top-level ada di block 0, sebelum pushBlock compound.
        const int vsze = (!semantic.btab.empty()) ? semantic.btab[0].vsze : 0;
        emit("INT", 0, vsze + FRAME_OVERHEAD);

        for (const auto &child : node.children)
        {
            if (!child)
                continue;
            if (labelIs(child->label, "Compound"))
                visitCompound(*child);
        }

        emitRet();
    }

    void visitDeclarations(const SemanticNode &node)
    {
        for (const auto &child : node.children)
        {
            if (!child) continue;
            if (labelStartsWith(child->label, "ProcedureDecl") || labelStartsWith(child->label, "FunctionDecl"))
            {
                visitSubprogram(*child);
            }
        }
    }

    void visitSubprogram(const SemanticNode &node)
    {
        const int tabIdx = getAnnotationInt(node, "tab_index");
        if (tabIdx > 0 && tabIdx < static_cast<int>(semantic.tab.size()))
        {
            funcAddresses[tabIdx] = instructions.size();

            int btabIdx = getAnnotationInt(node, "block_index", -1);
            int vsze = (btabIdx >= 0 && btabIdx < static_cast<int>(semantic.btab.size())) ? semantic.btab[btabIdx].vsze : 0;
            int psze = (btabIdx >= 0 && btabIdx < static_cast<int>(semantic.btab.size())) ? semantic.btab[btabIdx].psze : 0;
            emit("INT", 0, vsze + psze + FRAME_OVERHEAD);

            int oldLevel = currentLevel;
            currentLevel = semantic.tab[tabIdx].lev + 1; // Level eksekusi adalah deklarasi + 1

            for (const auto &child : node.children)
            {
                if (child && labelIs(child->label, "Block"))
                {
                    // Opsional: Jika subprogram punya deklarasi (nested func), proses di sini
                    for (const auto &bChild : child->children)
                    {
                        if (bChild && labelIs(bChild->label, "Declarations"))
                            visitDeclarations(*bChild);
                    }
                    
                    // Eksekusi tubuh subprogram
                    for (const auto &bChild : child->children)
                    {
                        if (bChild && labelIs(bChild->label, "Compound"))
                            visitCompound(*bChild);
                    }
                }
            }
            emitRet();
            currentLevel = oldLevel; // Kembalikan level
        }
    }

    void visitCompound(const SemanticNode &node)
    {
        for (const auto &child : node.children)
        {
            if (!child)
                continue;
            if (labelIs(child->label, "Statements"))
                visitStatements(*child);
        }
    }

    void visitStatements(const SemanticNode &node)
    {
        for (const auto &child : node.children)
            if (child)
                visitStatement(*child);
    }

    void visitStatement(const SemanticNode &node)
    {
        const std::string &lbl = node.label;

        if (labelIs(lbl, "Assign"))
        {
            visitAssign(node);
            return;
        }
        if (labelStartsWith(lbl, "Call("))
        {
            visitCall(node);
            return;
        }
        // begin … end sebagai statement
        if (labelIs(lbl, "Compound"))
        {
            visitCompound(node);
            return;
        }
        if (labelIs(lbl, "Statements"))
        {
            visitStatements(node);
            return;
        }

        if (labelIs(lbl, "If")) { visitIf(node); return; }
        if (labelIs(lbl, "While")) { visitWhile(node); return; }
        if (labelIs(lbl, "Repeat")) { visitRepeat(node); return; }
        if (labelIs(lbl, "For")) { visitFor(node); return; }
        if (labelIs(lbl, "Case")) { visitCase(node); return; }

        errors.push_back(std::string("ICG error: unknown statement node '") + lbl + "'");
    }

    void visitIf(const SemanticNode &node)
    {
        if (node.children.empty()) return;
        visitExpression(*node.children[0]);
        int jpcIdx = instructions.size();
        emit("JPC", 0, 0);

        if (node.children.size() > 1 && node.children[1])
            visitStatement(*node.children[1]);

        if (node.children.size() > 2 && node.children[2])
        {
            int jmpIdx = instructions.size();
            emit("JMP", 0, 0);
            instructions[jpcIdx].operand = instructions.size();
            visitStatement(*node.children[2]);
            instructions[jmpIdx].operand = instructions.size();
        }
        else
        {
            instructions[jpcIdx].operand = instructions.size();
        }
    }

    void visitWhile(const SemanticNode &node)
    {
        if (node.children.size() < 2) return;
        int conditionIdx = instructions.size();
        visitExpression(*node.children[0]);
        int jpcIdx = instructions.size();
        emit("JPC", 0, 0);

        if (node.children[1])
            visitStatement(*node.children[1]);

        emit("JMP", 0, conditionIdx);
        instructions[jpcIdx].operand = instructions.size();
    }

    void visitRepeat(const SemanticNode &node)
    {
        if (node.children.size() < 2) return;
        int loopIdx = instructions.size();

        if (node.children[0])
            visitStatement(*node.children[0]); // Body (bisa Statements)

        if (node.children[1])
            visitExpression(*node.children[1]); // Condition

        emit("JPC", 0, loopIdx); // Lompat kembali jika false (repeat UNTIL condition)
    }

    void visitFor(const SemanticNode &node)
    {
        // P2: ForNode(Assign, direction, end_expr, body)
        // Disederhanakan untuk ICG: Karena kita tidak memiliki node AST spesifik untuk pengkondisian di ICG,
        // For biasanya diterjemahkan ke While, tapi karena struktur node mungkin beda,
        // kita ambil Assign lalu JMP / JPC. 
        errors.push_back("ICG: 'For' is not fully translated in this milestone.");
    }

    void visitCase(const SemanticNode &node)
    {
        errors.push_back("ICG: 'Case' is not fully translated in this milestone.");
    }

    void visitAssign(const SemanticNode &node)
    {
        const SemanticNode *valueNode  = nullptr;
        const SemanticNode *targetNode = nullptr;

        for (const auto &child : node.children)
        {
            if (!child)
                continue;
            if (labelIs(child->label, "Value"))
                valueNode = child.get();
            else if (labelIs(child->label, "Target"))
                targetNode = child.get();
        }

        if (!valueNode || !targetNode)
        {
            errors.push_back("ICG error: malformed Assign node");
            return;
        }

        if (!valueNode->children.empty())
            visitExpression(*valueNode->children[0]);

        if (!targetNode->children.empty())
        {
            const SemanticNode &tv = *targetNode->children[0];
            const int tabIdx = getAnnotationInt(tv, "tab_index");

            if (tabIdx <= 0 || tabIdx >= static_cast<int>(semantic.tab.size()))
            {
                errors.push_back("ICG error: unresolved assignment target: " + tv.label);
                return;
            }

            const TabEntry &entry = semantic.tab[tabIdx];
            emit("STO", currentLevel - entry.lev, entry.adr + FRAME_OVERHEAD);
        }
    }

    void visitCall(const SemanticNode &node)
    {
        const std::string name = toLower(labelValue(node.label));

        if (name == "writeln")
        {
            visitWriteCall(node, /*newline=*/true);
            return;
        }
        if (name == "write")
        {
            visitWriteCall(node, /*newline=*/false);
            return;
        }
        if (name == "readln")
        {
            // readln tidak ada di spesifikasi OPR P1
            errors.push_back("ICG: 'readln' is not generated in P1 — skipped.");
            return;
        }

        // user-defined: butuh CAL/RET, baru di P2
        const int tabIdx = getAnnotationInt(node, "tab_index");
        if (tabIdx <= 0 || tabIdx >= static_cast<int>(semantic.tab.size()))
        {
            errors.push_back("ICG error: unresolved call to '" + name + "'");
            return;
        }
        
        for (const auto &child : node.children)
        {
            if (child && labelIs(child->label, "Args"))
            {
                for (const auto &arg : child->children)
                {
                    if (arg) visitExpression(*arg);
                }
            }
        }
        
        const TabEntry &entry = semantic.tab[tabIdx];
        
        // Pencarian nilai alamat pada funcAddresses
        if (funcAddresses.find(tabIdx) != funcAddresses.end())
        {
            emit("CAL", currentLevel - entry.lev, funcAddresses[tabIdx]);
        }
        else
        {
            errors.push_back("ICG error: unresolved function address for tab index " + std::to_string(tabIdx));
            emit("CAL", currentLevel - entry.lev, 0); // fallback dummy address
        }
    }

    // Tiap argumen: push nilai → OPR WRT. Argumen terakhir writeln: OPR WRTLN.
    void visitWriteCall(const SemanticNode &node, bool newline)
    {
        const SemanticNode *argsNode = nullptr;
        for (const auto &child : node.children)
            if (child && labelIs(child->label, "Args"))
            {
                argsNode = child.get();
                break;
            }

        if (!argsNode || argsNode->children.empty())
        {
            // writeln() tanpa argumen
            if (newline)
                emit("OPR", 0, opr::WRTLN);
            return;
        }

        const std::size_t argc = argsNode->children.size();
        for (std::size_t i = 0; i < argc; ++i)
        {
            visitExpression(*argsNode->children[i]);

            const bool isLast = (i + 1 == argc);
            if (newline && isLast)
                emit("OPR", 0, opr::WRTLN);
            else
                emit("OPR", 0, opr::WRT);
        }
    }

    // TAC flattening: kunjungi ekspresi secara post-order DFS → hasilnya di atas stack.
    void visitExpression(const SemanticNode &node)
    {
        const std::string &lbl = node.label;

        // unary:- dari semantic analysis → OPR NEG setelah nilai
        const bool unaryNeg = hasAnnotation(node, "unary:-");

        // integer literal
        if (labelStartsWith(lbl, "Int("))
        {
            int val = 0;
            try { val = std::stoi(labelValue(lbl)); }
            catch (...) { errors.push_back("ICG error: bad integer literal: " + lbl); }
            emit("LIT", 0, val);
        }

        // real literal
        else if (labelStartsWith(lbl, "Real("))
        {
            double dval = 0.0;
            try { dval = std::stod(labelValue(lbl)); }
            catch (...) { errors.push_back("ICG error: bad real literal: " + lbl); }
            emit("LIT", 0, static_cast<int>(dval));
        }

        // char literal
        else if (labelStartsWith(lbl, "Char("))
        {
            const std::string ch = labelValue(lbl);
            const int ordinal = ch.empty() ? 0 : static_cast<unsigned char>(ch[0]);
            emit("LIT", 0, ordinal);
        }

        // string literal — tidak didukung stack integer P1, emit placeholder
        else if (labelStartsWith(lbl, "String("))
        {
            emit("LIT", 0, 0);
        }

        // var / constant
        else if (labelStartsWith(lbl, "Var("))
        {
            const int tabIdx = getAnnotationInt(node, "tab_index");

            if (tabIdx <= 0 || tabIdx >= static_cast<int>(semantic.tab.size()))
            {
                errors.push_back("ICG error: unresolved identifier: " + lbl);
                emit("LIT", 0, 0);
            }
            else
            {
                const TabEntry &entry = semantic.tab[tabIdx];

                if (entry.obj == "constant")
                {
                    // constant: inline nilai compile-time
                    emit("LIT", 0, entry.adr);
                }
                else
                {
                    emit("LOD", currentLevel - entry.lev, entry.adr + FRAME_OVERHEAD);
                }
            }
        }

        // binary op: kiri → kanan → OPR (post-order)
        else if (labelStartsWith(lbl, "BinOp("))
        {
            if (node.children.size() < 2)
            {
                errors.push_back("ICG error: BinOp missing operands: " + lbl);
            }
            else
            {
                visitExpression(*node.children[0]); // left operand → stack
                visitExpression(*node.children[1]); // right operand → stack
                const std::string op = labelValue(lbl);
                const int code = binOpToOpr(op);
                if (code < 0)
                    errors.push_back("ICG error: unknown binary operator: " + op);
                else
                    emit("OPR", 0, code);
            }
        }

        // not
        else if (labelIs(lbl, "Not"))
        {
            if (!node.children.empty())
                visitExpression(*node.children[0]);
            emit("OPR", 0, opr::NEG);
            return; // NEG sudah menangani negasi; skip unaryNeg di bawah
        }

        // function call di dalam ekspresi
        else if (labelStartsWith(lbl, "Call("))
        {
            visitCall(node);
            return;
        }

        else
        {
            errors.push_back("ICG error: unknown expression node: " + lbl);
            emit("LIT", 0, 0);
        }

        if (unaryNeg)
            emit("OPR", 0, opr::NEG);
    }
};

} // anonymous namespace

// Public API

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
        for (const auto &err : result.errors)
            out << "  " << err << '\n';
        out << '\n';
    }

    out << "=== INTERMEDIATE CODE ===\n";

    for (std::size_t i = 0; i < result.instructions.size(); ++i)
    {
        const Instruction &instr = result.instructions[i];
        out << i << " " << instr.mnemonic;

        if (instr.mnemonic != "RET")
            out << " " << instr.level << " " << instr.operand;

        out << '\n';
    }
}
