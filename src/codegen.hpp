#pragma once

#include "semantic.hpp"
#include <ostream>
#include <string>
#include <vector>

// Slots 0-2 in every activation record are reserved:
//   0 = Static Link, 1 = Dynamic Link, 2 = Return Address.
// User-declared variables begin at offset FRAME_OVERHEAD.
inline constexpr int FRAME_OVERHEAD = 3;

struct Instruction
{
    std::string mnemonic;
    int level   = 0;
    int operand = 0;

    // Metadata internal VM. Tidak wajib terlihat di listing IC, tetapi dipakai
    // supaya printed IC tetap ringkas sementara VM tetap bisa menangani string,
    // real, readln, dan pemanggilan function/procedure.
    std::string valueType = "integer";
    double realOperand = 0.0;
    std::string stringOperand;

    int argCount = 0;
    bool returnsValue = false;
    int returnOffset = 0;
};

struct CodeGenResult
{
    std::vector<Instruction> instructions;
    std::vector<std::string> errors;
};

CodeGenResult generateCode(const SemanticResult &semantic);
void printCodeGenResult(const CodeGenResult &result, std::ostream &out);
