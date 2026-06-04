#pragma once

#include "codegen.hpp"
#include <vector>
#include <string>
#include <stdexcept>

inline constexpr int MAX_STACK_SIZE = 2048;

class Interpreter
{
public:
    void execute(const std::vector<Instruction> &code);

private:
    std::vector<int> stack; 
    int ip = 0;             
    int top = -1;           
    int base = 0;           

    int getBase(int levelDiff, int b) const;
    // stub
    void executeOPR(int opCode);
    void checkStackOverflow() const;
    void checkStackUnderflow() const;
    void checkStackSmashing(int targetAddr) const;
    void checkStackCorruption(int base) const;
    void validateJumpTarget(int target, int codeSize) const;
};

void runInterpreter(const CodeGenResult &result);