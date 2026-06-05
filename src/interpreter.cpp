#include "interpreter.hpp"

#include <iostream>
#include <limits>
#include <vector>

namespace
{
    int checkedInt(long long value, const std::string &context)
    {
        if (value < std::numeric_limits<int>::min() ||
            value > std::numeric_limits<int>::max())
        {
            throw std::runtime_error("Runtime Error: Numerical Overflow during " + context);
        }
        return static_cast<int>(value);
    }
}

void Interpreter::execute(const std::vector<Instruction> &code)
{
    stack.assign(MAX_STACK_SIZE, 0);
    frameLimits.clear();
    frameLimitByBase.clear();
    callFrames.clear();

    ip = 0;
    top = -1;
    base = 0;

    // Frame utama belum dialokasikan sampai instruksi INT pertama dieksekusi.
    frameLimits.push_back(-1);
    frameLimitByBase[0] = -1;

    while (true)
    {
        if (ip < 0 || ip >= static_cast<int>(code.size()))
            break;

        Instruction instr = code[ip];
        ip++;

        if (instr.mnemonic == "INT")
        {
            if (instr.operand < FRAME_OVERHEAD)
                throw std::runtime_error("Runtime Error: Invalid frame size " +
                                         std::to_string(instr.operand));

            const int newTop = base + instr.operand - 1;
            if (newTop >= MAX_STACK_SIZE)
                throw std::runtime_error("Runtime Error: Stack Overflow");

            top = newTop;
            if (frameLimits.empty())
                frameLimits.push_back(top);
            else
                frameLimits.back() = top;
            frameLimitByBase[base] = top;
        }
        else if (instr.mnemonic == "LIT")
        {
            checkStackOverflow();
            stack[++top] = instr.operand;
        }
        else if (instr.mnemonic == "LOD")
        {
            checkStackOverflow();
            const int targetBase = getBase(instr.level, base);
            const int addr = targetBase + instr.operand;
            checkAddress(addr);
            stack[++top] = stack[addr];
        }
        else if (instr.mnemonic == "STO")
        {
            checkStackUnderflow();
            const int targetBase = getBase(instr.level, base);
            const int addr = targetBase + instr.operand;
            checkStackSmashing(addr, targetBase);
            stack[addr] = stack[top--];
        }
        else if (instr.mnemonic == "JMP")
        {
            validateJumpTarget(instr.operand, static_cast<int>(code.size()));
            ip = instr.operand;
        }
        else if (instr.mnemonic == "JPC")
        {
            checkStackUnderflow();
            const int cond = stack[top--];
            if (cond == 0)
            {
                validateJumpTarget(instr.operand, static_cast<int>(code.size()));
                ip = instr.operand;
            }
        }
        else if (instr.mnemonic == "CAL")
        {
            validateJumpTarget(instr.operand, static_cast<int>(code.size()));

            const int argc = instr.argCount;
            if (argc < 0)
                throw std::runtime_error("Runtime Error: Invalid argument count");
            checkStackUnderflow(argc);

            std::vector<int> args(argc);
            const int argStart = top - argc + 1;
            for (int i = 0; i < argc; ++i)
                args[i] = stack[argStart + i];

            const int newBase = (argc == 0) ? (top + 1) : argStart;
            if (newBase + FRAME_OVERHEAD + argc - 1 >= MAX_STACK_SIZE)
                throw std::runtime_error("Runtime Error: Stack Overflow");

            stack[newBase] = getBase(instr.level, base); // Static link
            stack[newBase + 1] = base;                   // Dynamic link
            stack[newBase + 2] = ip;                     // Return address
            for (int i = 0; i < argc; ++i)
                stack[newBase + FRAME_OVERHEAD + i] = args[i];

            callFrames.push_back({instr.returnsValue, instr.returnOffset});
            frameLimits.push_back(newBase + FRAME_OVERHEAD + argc - 1);
            frameLimitByBase[newBase] = frameLimits.back();

            base = newBase;
            top = frameLimits.back();
            ip = instr.operand;
        }
        else if (instr.mnemonic == "RET")
        {
            bool returnsValue = false;
            int returnOffset = 0;
            if (!callFrames.empty())
            {
                returnsValue = callFrames.back().returnsValue;
                returnOffset = callFrames.back().returnOffset;
                callFrames.pop_back();
            }

            int returnValue = 0;
            if (returnsValue)
            {
                const int returnAddr = base + returnOffset;
                checkAddress(returnAddr);
                returnValue = stack[returnAddr];
            }

            if (base < 0 || base + 2 >= MAX_STACK_SIZE)
                throw std::runtime_error("Runtime Error: Stack Corruption");

            const int returnIp = stack[base + 2];
            const int dynamicLink = stack[base + 1];
            const int oldBase = base;

            frameLimitByBase.erase(oldBase);
            if (frameLimits.size() > 1)
                frameLimits.pop_back();

            top = oldBase - 1;
            base = dynamicLink;
            ip = returnIp;

            if (returnsValue)
            {
                checkStackOverflow();
                stack[++top] = returnValue;
            }

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
    if (levelDiff < 0)
        throw std::runtime_error("Runtime Error: Invalid lexical level difference");

    int currentBase = b;
    for (int i = 0; i < levelDiff; ++i)
    {
        if (currentBase < 0 || currentBase >= MAX_STACK_SIZE)
            throw std::runtime_error("Runtime Error: Stack Corruption");
        currentBase = stack[currentBase];
    }
    return currentBase;
}

void Interpreter::executeOPR(int opCode)
{
    switch (opCode)
    {
    case 1: // NEG
        checkStackUnderflow();
        if (stack[top] == std::numeric_limits<int>::min())
            throw std::runtime_error("Runtime Error: Numerical Overflow during NEG");
        stack[top] = -stack[top];
        break;
    case 2: // ADD
    case 3: // SUB
    case 4: // MUL
    case 5: // DIV
    case 6: // MOD
    case 7: // EQL
    case 8: // NEQ
    case 9: // LSS
    case 10: // GEQ
    case 11: // GTR
    case 12: // LEQ
        binaryOp(opCode);
        break;
    case 13: // WRT
        checkStackUnderflow();
        out << stack[top--];
        break;
    case 14: // WRTLN
        if (top - frameLimits.back() <= 0)
        {
            out << '\n';
        }
        else
        {
            out << stack[top--] << '\n';
        }
        break;
    default:
        throw std::runtime_error("Runtime Error: Unknown OPR code " + std::to_string(opCode));
    }
}

void Interpreter::binaryOp(int opCode)
{
    checkStackUnderflow(2);
    const int rhs = stack[top--];
    const int lhs = stack[top];

    switch (opCode)
    {
    case 2:
        stack[top] = checkedInt(static_cast<long long>(lhs) + rhs, "ADD");
        break;
    case 3:
        stack[top] = checkedInt(static_cast<long long>(lhs) - rhs, "SUB");
        break;
    case 4:
        stack[top] = checkedInt(static_cast<long long>(lhs) * rhs, "MUL");
        break;
    case 5:
        if (rhs == 0)
            throw std::runtime_error("Runtime Error: Division by Zero");
        if (lhs == std::numeric_limits<int>::min() && rhs == -1)
            throw std::runtime_error("Runtime Error: Numerical Overflow during DIV");
        stack[top] = lhs / rhs;
        break;
    case 6:
        if (rhs == 0)
            throw std::runtime_error("Runtime Error: Modulo by Zero");
        stack[top] = lhs % rhs;
        break;
    case 7:
        stack[top] = (lhs == rhs) ? 1 : 0;
        break;
    case 8:
        stack[top] = (lhs != rhs) ? 1 : 0;
        break;
    case 9:
        stack[top] = (lhs < rhs) ? 1 : 0;
        break;
    case 10:
        stack[top] = (lhs >= rhs) ? 1 : 0;
        break;
    case 11:
        stack[top] = (lhs > rhs) ? 1 : 0;
        break;
    case 12:
        stack[top] = (lhs <= rhs) ? 1 : 0;
        break;
    default:
        throw std::runtime_error("Runtime Error: Unknown binary OPR code " + std::to_string(opCode));
    }
}

void Interpreter::checkStackOverflow(int neededSlots) const
{
    if (neededSlots < 0 || top + neededSlots >= MAX_STACK_SIZE)
        throw std::runtime_error("Runtime Error: Stack Overflow");
}

void Interpreter::checkStackUnderflow(int neededValues) const
{
    if (neededValues <= 0)
        return;
    if (frameLimits.empty() || top - frameLimits.back() < neededValues)
        throw std::runtime_error("Runtime Error: Stack Underflow");
}

void Interpreter::checkStackSmashing(int targetAddr, int targetBase) const
{
    if (targetAddr < targetBase + FRAME_OVERHEAD)
        throw std::runtime_error(
            "Runtime Error: Stack Smashing — write to protected activation record at address " +
            std::to_string(targetAddr));

    auto limitIt = frameLimitByBase.find(targetBase);
    if (limitIt == frameLimitByBase.end() || targetAddr > limitIt->second)
        throw std::runtime_error(
            "Runtime Error: Stack Smashing — write beyond allocated frame at address " +
            std::to_string(targetAddr));
}

void Interpreter::checkAddress(int addr) const
{
    if (addr < 0 || addr >= MAX_STACK_SIZE)
        throw std::runtime_error("Runtime Error: Invalid memory address " + std::to_string(addr));

    auto exactFrame = frameLimitByBase.find(base);
    if (exactFrame != frameLimitByBase.end() && addr > top)
        throw std::runtime_error("Runtime Error: Out-of-Bounds Variable Access at address " +
                                 std::to_string(addr));
}

void Interpreter::validateJumpTarget(int target, int codeSize) const
{
    if (target < 0 || target >= codeSize)
        throw std::runtime_error(
            "Runtime Error: Invalid Jump Target — address " +
            std::to_string(target) + " is out of bounds");
}

void runInterpreter(const CodeGenResult &result, std::ostream &out)
{
    if (!result.errors.empty())
    {
        out << "INTERPRETER: skipped\n";
        return;
    }

    out << "INTERPRETER OUTPUT\n";

    Interpreter vm(out);
    try
    {
        vm.execute(result.instructions);
    }
    catch (const std::runtime_error &e)
    {
        out << "\n[RUNTIME ERROR] " << e.what() << '\n';
    }
}
