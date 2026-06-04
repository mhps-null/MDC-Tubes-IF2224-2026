#include "interpreter.hpp"
#include <iostream>

void Interpreter::execute(const std::vector<Instruction> &code)
{
    // Inisialisasi
    stack.assign(MAX_STACK_SIZE, 0);
    ip = 0;
    top = -1;
    base = 0;

    while (true)
    {
        // fetch
        if (ip < 0 || ip >= (int)code.size())
            break;
        Instruction instr = code[ip];
        ip++;

        // decode and execute
        if (instr.mnemonic == "INT")
        {
            top = base + instr.operand - 1;
        }
        else if (instr.mnemonic == "LIT")
        {
            checkStackOverflow();
            stack[++top] = instr.operand;
        }
        else if (instr.mnemonic == "LOD")
        {
            checkStackOverflow();
            int addr = getBase(instr.level, base) + instr.operand;
            stack[++top] = stack[addr];
        }
        else if (instr.mnemonic == "STO")
        {
            int addr = getBase(instr.level, base) + instr.operand;
            checkStackSmashing(addr);
            checkStackUnderflow();
            stack[addr] = stack[top--];
        }
        else if (instr.mnemonic == "JMP")
        {
            validateJumpTarget(instr.operand, code.size());
            ip = instr.operand;
        }
        else if (instr.mnemonic == "JPC")
        {
            checkStackUnderflow();
            int cond = stack[top--];
            if (cond == 0)
            {
                validateJumpTarget(instr.operand, code.size());
                ip = instr.operand;
            }
        }
        else if (instr.mnemonic == "CAL")
        {
            if (top + 3 >= MAX_STACK_SIZE)
                throw std::runtime_error("Runtime Error: Stack Overflow");
            stack[top + 1] = getBase(instr.level, base);
            stack[top + 2] = base;
            stack[top + 3] = ip;
            base = top + 1;     
            ip = instr.operand; 
        }
        else if (instr.mnemonic == "RET")
        {
            checkStackCorruption(base);
            top = base - 1;
            ip = stack[base + 2];
            base = stack[base + 1];

            if (ip == 0)
                break;
        }
        else if (instr.mnemonic == "OPR")
        {
            executeOPR(instr.operand);
        }
        else
        {
            throw std::runtime_error(
                "Runtime Error: Unknown instruction '" + instr.mnemonic + "'");
        }
    }
}

int Interpreter::getBase(int levelDiff, int b) const
{
    int base = b;
    int i = levelDiff;
    while (i > 0)
    {
        base = stack[base]; 
        i--;
    }
    return base;
}

void Interpreter::executeOPR(int opCode)
{
    // Stub
    switch (opCode)
    {
    case 13: // WRT
        checkStackUnderflow();
        std::cout << stack[top--];
        break;
    case 14: // WRTLN
        checkStackUnderflow();
        std::cout << stack[top--] << '\n';
        break;
    default:
        throw std::runtime_error(
            "Runtime Error: OPR " + std::to_string(opCode) +
            " not yet implemented (P4)");
    }
}

void Interpreter::checkStackOverflow() const
{
    if (top >= MAX_STACK_SIZE - 1)
        throw std::runtime_error(
            "Runtime Error: Stack Overflow");
}

void Interpreter::checkStackUnderflow() const
{
    if (top < base)
        throw std::runtime_error(
            "Runtime Error: Stack Underflow");
}

void Interpreter::checkStackSmashing(int targetAddr) const
{
    if (targetAddr < base + FRAME_OVERHEAD)
        throw std::runtime_error(
            "Runtime Error: Stack Smashing — write to protected activation record"
            " at address " +
            std::to_string(targetAddr));

    if (targetAddr > top)
        throw std::runtime_error(
            "Runtime Error: Stack Smashing — write beyond allocated frame"
            " at address " +
            std::to_string(targetAddr));
}

void Interpreter::checkStackCorruption(int base) const
{
    int expectedTop = base - 1;

    if (top != expectedTop)
        {
            throw std::runtime_error(
                "Runtime Error: Stack Corruption"
                " Expected top=" +
                std::to_string(expectedTop) +
                " but got top=" + std::to_string(top));
        }
}

void Interpreter::validateJumpTarget(int target, int codeSize) const
{
    if (target < 0 || target >= codeSize)
        throw std::runtime_error(
            "Runtime Error: Invalid Jump Target — address " +
            std::to_string(target) + " is out of bounds");
}

void runInterpreter(const CodeGenResult &result)
{
    if (!result.errors.empty())
    {
        std::cout << "INTERPRETER: skipped\n";
        return;
    }

    std::cout << "INTERPRETER OUTPUT\n";

    Interpreter vm;
    try
    {
        vm.execute(result.instructions);
    }
    catch (const std::runtime_error &e)
    {
        std::cout << "\n[RUNTIME ERROR] " << e.what() << '\n';
    }
}