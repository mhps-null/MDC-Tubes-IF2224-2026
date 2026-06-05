#pragma once

#include "codegen.hpp"
#include <vector>
#include <string>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <unordered_map>

inline constexpr int MAX_STACK_SIZE = 2048;

class Interpreter
{
public:
    explicit Interpreter(std::ostream &outStream = std::cout) : out(outStream) {}
    void execute(const std::vector<Instruction> &code);

private:
    struct CallFrameInfo
    {
        bool returnsValue = false;
        int returnOffset = 0;
    };

    std::ostream &out;
    std::vector<int> stack;
    std::vector<int> frameLimits;
    std::unordered_map<int, int> frameLimitByBase;
    std::vector<CallFrameInfo> callFrames;
    int ip = 0;
    int top = -1;
    int base = 0;

    int getBase(int levelDiff, int b) const;
    void executeOPR(int opCode);
    void binaryOp(int opCode);
    void checkStackOverflow(int neededSlots = 1) const;
    void checkStackUnderflow(int neededValues = 1) const;
    void checkStackSmashing(int targetAddr, int targetBase) const;
    void checkAddress(int addr) const;
    void validateJumpTarget(int target, int codeSize) const;
};

void runInterpreter(const CodeGenResult &result, std::ostream &out = std::cout);