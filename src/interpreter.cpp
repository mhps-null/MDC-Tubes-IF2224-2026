#include "interpreter.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace
{
    int checkedInt(long long value, const std::string &context)
    {
        if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
            throw std::runtime_error("Runtime Error: Numerical Overflow during " + context);
        return static_cast<int>(value);
    }

}

void Interpreter::execute(const std::vector<Instruction> &code)
{
    stack.assign(MAX_STACK_SIZE, Value::fromInt(0));
    frameLimits.clear();
    frameLimitByBase.clear();
    callFrames.clear();

    ip = 0;
    top = -1;
    base = 0;
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
                throw std::runtime_error("Runtime Error: Invalid frame size " + std::to_string(instr.operand));
            const int newTop = base + instr.operand - 1;
            if (newTop >= MAX_STACK_SIZE)
                throw std::runtime_error("Runtime Error: Stack Overflow");
            top = newTop;
            if (frameLimits.empty()) frameLimits.push_back(top);
            else frameLimits.back() = top;
            frameLimitByBase[base] = top;
        }
        else if (instr.mnemonic == "LIT")
        {
            checkStackOverflow();
            if (instr.valueType == "real") stack[++top] = Value::fromReal(instr.realOperand);
            else if (instr.valueType == "string") stack[++top] = Value::fromString(instr.stringOperand);
            else stack[++top] = Value::fromInt(instr.operand);
        }
        else if (instr.mnemonic == "LDA")
        {
            checkStackOverflow();
            const int targetBase = getBase(instr.level, base);
            const int addr = targetBase + instr.operand;
            checkAddress(addr);
            stack[++top] = Value::fromInt(addr);
        }
        else if (instr.mnemonic == "LOD")
        {
            checkStackOverflow();
            const int targetBase = getBase(instr.level, base);
            const int addr = targetBase + instr.operand;
            checkAddress(addr);
            stack[++top] = stack[addr];
        }
        else if (instr.mnemonic == "LDI")
        {
            checkStackUnderflow();
            const int addr = asAddress(stack[top--]);
            checkAddress(addr);
            checkStackOverflow();
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
        else if (instr.mnemonic == "STI")
        {
            checkStackUnderflow(2);
            Value value = stack[top--];
            const int addr = asAddress(stack[top--]);
            checkAddress(addr);
            stack[addr] = value;
        }
        else if (instr.mnemonic == "IDX")
        {
            checkStackUnderflow();
            const int index = asInt(stack[top]);
            const int low = instr.level;
            const int high = instr.operand;
            const int elemSize = instr.argCount <= 0 ? 1 : instr.argCount;
            if (index < low || index > high)
            {
                throw std::runtime_error("Runtime Error: Out-of-Bounds Array Access — index " +
                                         std::to_string(index) + " not in [" + std::to_string(low) +
                                         ".." + std::to_string(high) + "]");
            }
            stack[top] = Value::fromInt((index - low) * elemSize);
        }
        else if (instr.mnemonic == "DUP")
        {
            checkStackUnderflow();
            checkStackOverflow();
            stack[top + 1] = stack[top];
            ++top;
        }
        else if (instr.mnemonic == "POP")
        {
            checkStackUnderflow();
            --top;
        }
        else if (instr.mnemonic == "RED")
        {
            readIntoAddress(instr);
        }
        else if (instr.mnemonic == "JMP")
        {
            validateJumpTarget(instr.operand, static_cast<int>(code.size()));
            ip = instr.operand;
        }
        else if (instr.mnemonic == "JPC")
        {
            checkStackUnderflow();
            const bool cond = isTrue(stack[top--]);
            if (!cond)
            {
                validateJumpTarget(instr.operand, static_cast<int>(code.size()));
                ip = instr.operand;
            }
        }
        else if (instr.mnemonic == "CAL")
        {
            validateJumpTarget(instr.operand, static_cast<int>(code.size()));
            const int argc = instr.argCount;
            if (argc < 0) throw std::runtime_error("Runtime Error: Invalid argument count");
            checkStackUnderflow(argc);

            std::vector<Value> args(argc);
            const int argStart = top - argc + 1;
            for (int i = 0; i < argc; ++i) args[i] = stack[argStart + i];

            const int newBase = (argc == 0) ? (top + 1) : argStart;
            if (newBase + FRAME_OVERHEAD + argc - 1 >= MAX_STACK_SIZE)
                throw std::runtime_error("Runtime Error: Stack Overflow");

            stack[newBase] = Value::fromInt(getBase(instr.level, base));
            stack[newBase + 1] = Value::fromInt(base);
            stack[newBase + 2] = Value::fromInt(ip);
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

            Value returnValue = Value::fromInt(0);
            if (returnsValue)
            {
                const int returnAddr = base + returnOffset;
                checkAddress(returnAddr);
                returnValue = stack[returnAddr];
            }

            if (base < 0 || base + 2 >= MAX_STACK_SIZE)
                throw std::runtime_error("Runtime Error: Stack Corruption");
            const int returnIp = asAddress(stack[base + 2]);
            const int dynamicLink = asAddress(stack[base + 1]);
            const int oldBase = base;

            frameLimitByBase.erase(oldBase);
            if (frameLimits.size() > 1) frameLimits.pop_back();

            top = oldBase - 1;
            base = dynamicLink;
            ip = returnIp;

            if (returnsValue)
            {
                checkStackOverflow();
                stack[++top] = returnValue;
            }

            if (ip == 0) break;
        }
        else if (instr.mnemonic == "OPR")
        {
            executeOPR(instr.operand);
        }
        else
        {
            throw std::runtime_error("Runtime Error: Unknown instruction '" + instr.mnemonic + "'");
        }
    }
}

int Interpreter::getBase(int levelDiff, int b) const
{
    if (levelDiff < 0) throw std::runtime_error("Runtime Error: Invalid lexical level difference");
    int currentBase = b;
    for (int i = 0; i < levelDiff; ++i)
    {
        if (currentBase < 0 || currentBase >= MAX_STACK_SIZE)
            throw std::runtime_error("Runtime Error: Stack Corruption");
        currentBase = asAddress(stack[currentBase]);
    }
    return currentBase;
}

int Interpreter::asAddress(const Value &value) const
{
    if (value.kind != Value::Kind::Int)
        throw std::runtime_error("Runtime Error: Invalid memory address value");
    return value.i;
}

int Interpreter::asInt(const Value &value) const
{
    if (value.kind == Value::Kind::Int) return value.i;
    if (value.kind == Value::Kind::Real)
    {
        if (value.r < std::numeric_limits<int>::min() || value.r > std::numeric_limits<int>::max())
            throw std::runtime_error("Runtime Error: Numerical Overflow during real-to-integer conversion");
        return static_cast<int>(value.r);
    }
    try { return std::stoi(value.s); }
    catch (...) { throw std::runtime_error("Runtime Error: Cannot convert string to integer"); }
}

double Interpreter::asReal(const Value &value) const
{
    if (value.kind == Value::Kind::Real) return value.r;
    if (value.kind == Value::Kind::Int) return static_cast<double>(value.i);
    try { return std::stod(value.s); }
    catch (...) { throw std::runtime_error("Runtime Error: Cannot convert string to real"); }
}

bool Interpreter::isTrue(const Value &value) const
{
    if (value.kind == Value::Kind::String) return !value.s.empty();
    if (value.kind == Value::Kind::Real) return value.r != 0.0;
    return value.i != 0;
}

std::string Interpreter::toString(const Value &value) const
{
    if (value.kind == Value::Kind::String) return value.s;
    if (value.kind == Value::Kind::Int) return std::to_string(value.i);
    std::ostringstream oss;
    oss << std::setprecision(12) << value.r;
    std::string s = oss.str();
    if (s.find('.') != std::string::npos)
    {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}

void Interpreter::executeOPR(int opCode)
{
    switch (opCode)
    {
    case 1:
        checkStackUnderflow();
        if (stack[top].kind == Value::Kind::Real)
            stack[top] = Value::fromReal(-stack[top].r);
        else
        {
            int v = asInt(stack[top]);
            if (v == std::numeric_limits<int>::min())
                throw std::runtime_error("Runtime Error: Numerical Overflow during NEG");
            stack[top] = Value::fromInt(-v);
        }
        break;
    case 2: case 3: case 4: case 5: case 6:
    case 7: case 8: case 9: case 10: case 11: case 12:
        binaryOp(opCode);
        break;
    case 13:
        checkStackUnderflow();
        out << toString(stack[top--]);
        break;
    case 14:
        if (frameLimits.empty() || top <= frameLimits.back())
            out << '\n';
        else
            out << toString(stack[top--]) << '\n';
        break;
    default:
        throw std::runtime_error("Runtime Error: Unknown OPR code " + std::to_string(opCode));
    }
}

void Interpreter::binaryOp(int opCode)
{
    checkStackUnderflow(2);
    Value rhs = stack[top--];
    Value lhs = stack[top];

    const bool realOp = lhs.kind == Value::Kind::Real || rhs.kind == Value::Kind::Real;

    switch (opCode)
    {
    case 2:
        if (lhs.kind == Value::Kind::String || rhs.kind == Value::Kind::String)
            stack[top] = Value::fromString(toString(lhs) + toString(rhs));
        else if (realOp)
            stack[top] = Value::fromReal(asReal(lhs) + asReal(rhs));
        else
            stack[top] = Value::fromInt(checkedInt(static_cast<long long>(asInt(lhs)) + asInt(rhs), "ADD"));
        break;
    case 3:
        if (realOp) stack[top] = Value::fromReal(asReal(lhs) - asReal(rhs));
        else stack[top] = Value::fromInt(checkedInt(static_cast<long long>(asInt(lhs)) - asInt(rhs), "SUB"));
        break;
    case 4:
        if (realOp) stack[top] = Value::fromReal(asReal(lhs) * asReal(rhs));
        else stack[top] = Value::fromInt(checkedInt(static_cast<long long>(asInt(lhs)) * asInt(rhs), "MUL"));
        break;
    case 5:
        if (realOp)
        {
            double r = asReal(rhs);
            if (r == 0.0) throw std::runtime_error("Runtime Error: Division by Zero");
            stack[top] = Value::fromReal(asReal(lhs) / r);
        }
        else
        {
            int r = asInt(rhs), l = asInt(lhs);
            if (r == 0) throw std::runtime_error("Runtime Error: Division by Zero");
            if (l == std::numeric_limits<int>::min() && r == -1)
                throw std::runtime_error("Runtime Error: Numerical Overflow during DIV");
            stack[top] = Value::fromInt(l / r);
        }
        break;
    case 6:
    {
        int r = asInt(rhs), l = asInt(lhs);
        if (r == 0) throw std::runtime_error("Runtime Error: Modulo by Zero");
        stack[top] = Value::fromInt(l % r);
        break;
    }
    case 7:
        stack[top] = Value::fromInt(toString(lhs) == toString(rhs));
        break;
    case 8:
        stack[top] = Value::fromInt(toString(lhs) != toString(rhs));
        break;
    case 9:
        if (lhs.kind == Value::Kind::String || rhs.kind == Value::Kind::String)
            stack[top] = Value::fromInt(toString(lhs) < toString(rhs));
        else stack[top] = Value::fromInt(asReal(lhs) < asReal(rhs));
        break;
    case 10:
        if (lhs.kind == Value::Kind::String || rhs.kind == Value::Kind::String)
            stack[top] = Value::fromInt(toString(lhs) >= toString(rhs));
        else stack[top] = Value::fromInt(asReal(lhs) >= asReal(rhs));
        break;
    case 11:
        if (lhs.kind == Value::Kind::String || rhs.kind == Value::Kind::String)
            stack[top] = Value::fromInt(toString(lhs) > toString(rhs));
        else stack[top] = Value::fromInt(asReal(lhs) > asReal(rhs));
        break;
    case 12:
        if (lhs.kind == Value::Kind::String || rhs.kind == Value::Kind::String)
            stack[top] = Value::fromInt(toString(lhs) <= toString(rhs));
        else stack[top] = Value::fromInt(asReal(lhs) <= asReal(rhs));
        break;
    default:
        throw std::runtime_error("Runtime Error: Unknown binary OPR code " + std::to_string(opCode));
    }

    if (stack[top].kind == Value::Kind::Real && !std::isfinite(stack[top].r))
        throw std::runtime_error("Runtime Error: Numerical Overflow during real operation");
}

void Interpreter::readIntoAddress(const Instruction &instr)
{
    checkStackUnderflow();
    const int addr = asAddress(stack[top--]);
    checkAddress(addr);

    std::string token;
    if (!(in >> token))
        throw std::runtime_error("Runtime Error: readln failed — no input available");

    try
    {
        if (instr.valueType == "real") stack[addr] = Value::fromReal(std::stod(token));
        else if (instr.valueType == "string") stack[addr] = Value::fromString(token);
        else if (instr.valueType == "char") stack[addr] = Value::fromInt(token.empty() ? 0 : static_cast<unsigned char>(token[0]));
        else stack[addr] = Value::fromInt(std::stoi(token));
    }
    catch (...)
    {
        throw std::runtime_error("Runtime Error: readln cannot parse input '" + token + "' as " + instr.valueType);
    }
}

void Interpreter::checkStackOverflow(int neededSlots) const
{
    if (neededSlots < 0 || top + neededSlots >= MAX_STACK_SIZE)
        throw std::runtime_error("Runtime Error: Stack Overflow");
}

void Interpreter::checkStackUnderflow(int neededValues) const
{
    if (neededValues <= 0) return;
    if (frameLimits.empty() || top - frameLimits.back() < neededValues)
        throw std::runtime_error("Runtime Error: Stack Underflow");
}

void Interpreter::checkStackSmashing(int targetAddr, int targetBase) const
{
    if (targetAddr < targetBase + FRAME_OVERHEAD)
        throw std::runtime_error("Runtime Error: Stack Smashing — write to protected activation record at address " + std::to_string(targetAddr));
    auto it = frameLimitByBase.find(targetBase);
    if (it == frameLimitByBase.end() || targetAddr > it->second)
        throw std::runtime_error("Runtime Error: Stack Smashing — write beyond allocated frame at address " + std::to_string(targetAddr));
}

void Interpreter::checkAddress(int addr) const
{
    if (addr < 0 || addr >= MAX_STACK_SIZE)
        throw std::runtime_error("Runtime Error: Invalid memory address " + std::to_string(addr));

    bool insideAllocatedFrame = false;
    for (const auto &frame : frameLimitByBase)
    {
        const int frameBase = frame.first;
        const int frameLimit = frame.second;
        if (frameLimit >= frameBase && addr >= frameBase && addr <= frameLimit)
        {
            insideAllocatedFrame = true;
            break;
        }
    }
    if (!insideAllocatedFrame)
        throw std::runtime_error("Runtime Error: Out-of-Bounds Variable Access at address " + std::to_string(addr));
}

void Interpreter::validateJumpTarget(int target, int codeSize) const
{
    if (target < 0 || target >= codeSize)
        throw std::runtime_error("Runtime Error: Invalid Jump Target — address " + std::to_string(target) + " is out of bounds");
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
