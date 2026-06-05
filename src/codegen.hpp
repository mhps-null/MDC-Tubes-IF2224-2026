#pragma once

#include "semantic.hpp"
#include <ostream>
#include <string>
#include <vector>

// Slots 0-2 in every activation record are reserved:
//   0 = Static Link, 1 = Dynamic Link, 2 = Return Address.
// User-declared variables begin at offset FRAME_OVERHEAD.
inline constexpr int FRAME_OVERHEAD = 3;

// A single flat instruction in the intermediate code.
// Format when printed: <idx> <mnemonic> <level> <operand>
// Exception: RET has no level/operand.
struct Instruction
{
    std::string mnemonic;
    int level   = 0;
    int operand = 0;

    // Metadata internal untuk VM. Metadata ini tidak dicetak sebagai bagian
    // dari intermediate code agar format output tetap mengikuti spesifikasi:
    // <idx> <mnemonic> <level> <operand>.
    int argCount = 0;
    bool returnsValue = false;
    int returnOffset = 0;
};

// Result of the code generation phase.
struct CodeGenResult
{
    std::vector<Instruction> instructions;
    std::vector<std::string> errors;
};

// Generate intermediate code from a decorated AST (SemanticResult).
// P1 scope: INT, LIT, LOD, STO, OPR (arithmetic ops), RET.
// P2 nodes (If, While, CAL, JMP, JPC) are detected but not generated.
CodeGenResult generateCode(const SemanticResult &semantic);

// Print the numbered instruction listing to the given stream.
void printCodeGenResult(const CodeGenResult &result, std::ostream &out);
