#pragma once

#include "codegen.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

inline constexpr int MAX_STACK_SIZE = 2048;

class Interpreter
{
public:
    explicit Interpreter(std::ostream &outStream = std::cout, std::istream &inStream = std::cin)
        : out(outStream), in(inStream) {}
    void execute(const std::vector<Instruction> &code);

private:
    struct Value
    {
        enum class Kind { Int, Real, String } kind = Kind::Int;
        int i = 0;
        double r = 0.0;
        std::string s;

        static Value fromInt(int v) { Value x; x.kind = Kind::Int; x.i = v; x.r = v; return x; }
        static Value fromReal(double v) { Value x; x.kind = Kind::Real; x.r = v; x.i = static_cast<int>(v); return x; }
        static Value fromString(std::string v) { Value x; x.kind = Kind::String; x.s = std::move(v); return x; }
    };

    struct CallFrameInfo
    {
        bool returnsValue = false;
        int returnOffset = 0;
    };

    std::ostream &out;
    std::istream &in;
    std::vector<Value> stack;
    std::vector<int> frameLimits;
    std::unordered_map<int, int> frameLimitByBase;
    std::vector<CallFrameInfo> callFrames;
    int ip = 0;
    int top = -1;
    int base = 0;

    int getBase(int levelDiff, int b) const;
    int asAddress(const Value &value) const;
    int asInt(const Value &value) const;
    double asReal(const Value &value) const;
    bool isTrue(const Value &value) const;
    std::string toString(const Value &value) const;

    void executeOPR(int opCode);
    void binaryOp(int opCode);
    void readIntoAddress(const Instruction &instr);

    void checkStackOverflow(int neededSlots = 1) const;
    void checkStackUnderflow(int neededValues = 1) const;
    void checkStackSmashing(int targetAddr, int targetBase) const;
    void checkAddress(int addr) const;
    void validateJumpTarget(int target, int codeSize) const;
};

void runInterpreter(const CodeGenResult &result, std::ostream &out = std::cout);
