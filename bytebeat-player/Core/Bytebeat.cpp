#include "Bytebeat.h"
#include "GlobalState.h"
#include <cmath>
#include <cctype>
#include <algorithm>
#include <string>
#include <sstream>
#include <unordered_map>

using namespace std;

static int getPrecedence(OpType op) {
    switch (op) {
    case OpType::Mul: case OpType::Div: case OpType::Mod: return 10;
    case OpType::Add: case OpType::Sub: return 9;
    case OpType::Shl: case OpType::Shr: return 8;
    case OpType::LT: case OpType::GT: case OpType::LE: case OpType::GE: return 7;
    case OpType::EQ: case OpType::NE: return 6;
    case OpType::And: return 5; case OpType::Xor: return 4; case OpType::Or: return 3;
    case OpType::Assign: case OpType::Coma: return 1;
    default: return 0;
    }
}

bool BytebeatExpression::Compile(const string& expr, string& error, int& errorPos) {
    error.clear(); errorPos = -1; m_rpn.clear();
    if (expr.empty()) return false;

    vector<Token> tokens;
    vector<Token> stack;
    bool expectUnary = true;

    static const unordered_map<string, FunType> funMap = {
        {"sin", FunType::Sin}, {"cos", FunType::Cos}, {"abs", FunType::Abs}, {"floor", FunType::Floor}
    };
    static const unordered_map<string, OpType> doubleOps = {
        {"<<", OpType::Shl}, {">>", OpType::Shr}, {"<=", OpType::LE},
        {">=", OpType::GE}, {"==", OpType::EQ}, {"!=", OpType::NE}
    };
    static const unordered_map<char, OpType> singleOps = {
        {'+', OpType::Add}, {'-', OpType::Sub}, {'*', OpType::Mul}, {'/', OpType::Div},
        {'%', OpType::Mod}, {'&', OpType::And}, {'|', OpType::Or}, {'^', OpType::Xor},
        {'<', OpType::LT}, {'>', OpType::GT}, {'=', OpType::Assign}, {',', OpType::Coma}
    };

    for (size_t i = 0; i < expr.size(); ) {
        if (isspace(expr[i])) { i++; continue; }
        int start = (int)i;

        if (isdigit(expr[i])) {
            double v = 0;
            while (i < expr.size() && isdigit(expr[i])) v = v * 10 + (expr[i++] - '0');
            
            if (i < expr.size() && expr[i] == '.') {
                i++;
                double frac = 0.1;
                while (i < expr.size() && isdigit(expr[i])) {
                    v += (expr[i++] - '0') * frac;
                    frac *= 0.1;
                }
            }
            
            tokens.emplace_back(v, start);
            expectUnary = false;
        }
        else if (isalpha(expr[i])) {
            string name;
            while (i < expr.size() && isalpha(expr[i])) name += expr[i++];
            if (name == "t") tokens.emplace_back(TokType::VarT, start);
            else if (funMap.count(name)) tokens.emplace_back(funMap.at(name), start);
            else {
                int id = state.getVarId(name);
                tokens.emplace_back(id, start);
            }
            expectUnary = false;
        }
        else if (expr[i] == '(') { tokens.emplace_back(TokType::LParen, start); i++; expectUnary = true; }
        else if (expr[i] == ')') { tokens.emplace_back(TokType::RParen, start); i++; expectUnary = false; }
        else if (expr[i] == '?') { tokens.emplace_back(TokType::Quest, start); i++; expectUnary = true; }
        else if (expr[i] == ':') { tokens.emplace_back(TokType::Colon, start); i++; expectUnary = true; }
        else if (expr[i] == '~') { tokens.emplace_back(OpType::BitNot, start); i++; expectUnary = true; }
        else {
            OpType ot;
            if ((expr[i] == '-' || expr[i] == '!') && expectUnary) { tokens.emplace_back(expr[i] == '-' ? OpType::Neg : OpType::BitNot, start); i++; }
            else {
                OpType ot = OpType::Add;
                bool found = false;
                if (i + 1 < expr.size()) {
                    string two = expr.substr(i, 2);
                    if (doubleOps.count(two)) { ot = doubleOps.at(two); i += 2; found = true; }
                }
                if (!found) {
                    if (singleOps.count(expr[i])) { ot = singleOps.at(expr[i]); i++; }
                    else { error = "Unexpected token: '" + string(1, expr[i]) + "'"; errorPos = start; return false; }
                }
                tokens.emplace_back(ot, start);
                expectUnary = true;
            }
        }
    }

    // Shunting-yard Algorithm (conversion to RPN)
    for (const auto& t : tokens) {
        if (t.type == TokType::Number || t.type == TokType::VarT || t.type == TokType::Identifier) {
            m_rpn.push_back(t);
        }
        else if (t.type == TokType::Fun || t.type == TokType::LParen) {
            stack.push_back(t);
        }
        else if (t.type == TokType::RParen) {
            bool foundLParen = false;
            while (!stack.empty()) {
                if (stack.back().type == TokType::LParen) {
                    stack.pop_back();
                    foundLParen = true;
                    break;
                }
                m_rpn.push_back(stack.back());
                stack.pop_back();
            }
            // Detect missing LParen
            if (!foundLParen) {
                error = "Unmatched closing parenthesis ')'";
                errorPos = t.pos;
                return false;
            }
            if (!stack.empty() && stack.back().type == TokType::Fun) {
                m_rpn.push_back(stack.back()); 
                stack.pop_back();
            }
        }
        else {
            int prec = (t.type == TokType::Quest || t.type == TokType::Colon) ? 2 : 
                (t.op == OpType::Neg || t.op == OpType::BitNot) ? 12 : 
                getPrecedence(t.op);
            
            while (!stack.empty() && stack.back().type != TokType::LParen) {
                int topPrec = (stack.back().type == TokType::Fun) ? 12 :
                    (stack.back().type == TokType::Op && (stack.back().op == OpType::Neg || stack.back().op == OpType::BitNot)) ? 12 :
                    getPrecedence(stack.back().op);

                if (topPrec < prec) break;
                m_rpn.push_back(stack.back());
                stack.pop_back();
            }
            stack.push_back(t);
        }
    }

    // Detect missing RParen
    while (!stack.empty()) {
        if (stack.back().type == TokType::LParen) {
            error = "Unmatched opening parenthesis '('";
            errorPos = stack.back().pos;
            return false;
        }
        m_rpn.push_back(stack.back());
        stack.pop_back();
    }
    if (m_rpn.empty()) {
        error = "Empty expression";
        return false;
    }
    return true;
}

int BytebeatExpression::Eval(uint32_t t) const {
    if (m_rpn.empty()) return 0;
    double stack[1024];
    int sp = -1;

    // Cache index to memory
    const std::vector<double>& memory = state.vmMemory;

    for (const auto& tok : m_rpn) {
        if (sp >= 1023) break; // Security

        switch (tok.type) {
        case TokType::Number: stack[++sp] = tok.value; break;
        case TokType::VarT: stack[++sp] = (double)t; break;
        // KEY OPTIMIZATION
        // Replace searching string with getting index value
        case TokType::Identifier: {
            stack[++sp] = memory[tok.index];
            break;
        }
        case TokType::Fun:
            if (sp >= 0) {
                double& v = stack[sp];
                if (tok.fun == FunType::Sin) v = sin(v);
                else if (tok.fun == FunType::Cos) v = cos(v);
                else if (tok.fun == FunType::Abs) v = fabs(v);
                else if (tok.fun == FunType::Floor) v = floor(v);
            }
            break;
        case TokType::Colon: // Ternary logic
            if (sp >= 2) {
                double f = stack[sp--];
                double v = stack[sp--];
                double cond = stack[sp];
                stack[sp] = (cond != 0) ? v : f;
            } 
            break;
        case TokType::Quest: break; // Ignore '?'
        default:
            if (tok.op == OpType::Neg) { if (sp >= 0) stack[sp] = -stack[sp]; }
            else if (tok.op == OpType::BitNot) { if (sp >= 0) stack[sp] = (double)(~(int64_t)stack[sp]); }
            else if (sp >= 1) {
                double b = stack[sp--];
                double& a = stack[sp];

                int32_t ia = (int32_t)a;
                int32_t ib = (int32_t)b;

                switch (tok.op) {
                case OpType::Add: a += b; break;
                case OpType::Sub: a -= b; break;
                case OpType::Mul: a *= b; break;
                case OpType::Div: a = (b != 0) ? (a / b) : 0; break;
                case OpType::Mod: a = (b != 0) ? (double)((int64_t)a % (int64_t)b) : 0; break;

                // Binary operations
                case OpType::And: a = (double)(ia & ib); break;
                case OpType::Or:  a = (double)(ia | ib); break;
                case OpType::Xor: a = (double)(ia ^ ib); break;
                case OpType::Shl: a = (double)(ia << (ib & 0x1F)); break;
                case OpType::Shr: a = (double)(ia >> (ib & 0x1F)); break;

                // Logic operations
                case OpType::LT:  a = (a < b); break;
                case OpType::GT:  a = (a > b); break;
                case OpType::LE:  a = (a <= b); break;
                case OpType::GE:  a = (a >= b); break;
                case OpType::EQ:  a = (a == b); break;
                case OpType::NE:  a = (a != b); break;
                default: break;
                }
            }
            break;
        }
    }
    return (sp >= 0) ? ((int)stack[0] & 0xFF) : 0;
}

bool ComplexEngine::Compile(const std::string& code, std::string& err) {
    instructions.clear();
    std::stringstream ss(code);
    std::string segment;
    while (std::getline(ss, segment, ',')) {
        if (segment.find_first_not_of(" \t\n\r") == std::string::npos) continue;

        Instruction ins;
        size_t eq = segment.find('=');
        if (eq != std::string::npos) {
            ins.type = Instruction::Type::AssignVar;
            string varName = segment.substr(0, eq);
            varName.erase(remove_if(varName.begin(), varName.end(), ::isspace), varName.end());
            
            // Get index for allocated variable
            ins.targetVarIdx = state.getVarId(varName);

            int ep;
            if (!ins.expr.Compile(segment.substr(eq + 1), err, ep)) return false;
        }
        else {
            ins.type = Instruction::Type::EvalExpr;
            int ep;
            if (!ins.expr.Compile(segment, err, ep)) return false;
        }
        instructions.push_back(ins);
    }
    return !instructions.empty();
}

int ComplexEngine::Eval(uint32_t t) {
    double lastVal = 0;
    for (auto& ins : instructions) {
        lastVal = ins.expr.Eval(t);
        if (ins.type == Instruction::Type::AssignVar) state.vmMemory[ins.targetVarIdx] = lastVal;
    }

    return (int)((int32_t)lastVal & 0xFF);
}