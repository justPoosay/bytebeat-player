#include "Bytebeat.h"
#include "GlobalState.h"
#include <cmath>
#include <cctype>
#include <algorithm>
#include <string>
#include <sstream>
#include <unordered_map>

using namespace std;

// const to differenciate Array ID from String ID
// Strings have ID 0..199999, Arrays have 200000+
static const int ARRAY_ID_OFFSET = 200000;

// Globals (move to other file later)
static std::vector<std::string> g_strings;
static std::vector<std::vector<double>> g_arrays;

static int getPrecedence(OpType op) {
    switch (op) {
    case OpType::Index: return 13;
    case OpType::CharCodeAt: return 12;
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
        {"sin", FunType::Sin}, {"cos", FunType::Cos},
        {"abs", FunType::Abs}, {"floor", FunType::Floor},
        {"pow", FunType::Pow}, {"tan", FunType::Tan}, {"random", FunType::Random}
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
        else if (isalpha(expr[i]) || expr[i] == '_') {
            string name;
            while (i < expr.size() && (isalnum(expr[i]) || expr[i] == '_')) name += expr[i++];

            if (name == "t") tokens.emplace_back(TokType::VarT, start);
            else if (funMap.count(name)) tokens.emplace_back(funMap.at(name), start);
            else {
                size_t j = i;
                while (j < expr.size() && isspace(expr[j])) j++;
                bool isAssign = false;
                if (j < expr.size() && expr[j] == '=' && (j + 1 >= expr.size() || expr[j + 1] != '=')) isAssign = true;
                int id = state.getVarId(name);
                tokens.emplace_back(id, start, isAssign);
            }
            expectUnary = false;
        }
        else if (expr[i] == '\'' || expr[i] == '"') {
            char quote = expr[i++];
            string s;
            while (i < expr.size() && expr[i] != quote) s += expr[i++];
            if (i < expr.size()) i++;

            g_strings.push_back(s);
            Token t(TokType::String, start);
            t.index = (int)g_strings.size() - 1;
            tokens.push_back(t);
            expectUnary = false;
        }
        else if (expr[i] == '[') {
            if (expectUnary) {
                i++;
                vector<double> arr;
                while (i < expr.size() && expr[i] != ']') {
                    if (isspace(expr[i]) || expr[i] == ',') { i++; continue; }
                    if (isdigit(expr[i]) || expr[i] == '-' || expr[i] == '.') {
                        size_t nextIdx;
                        try {
                            double val = stod(expr.substr(i), &nextIdx);
                            arr.push_back(val);
                            i += nextIdx;
                        }
                        catch (...) { i++; }
                    }
                    else { i++; }
                }
                if (i < expr.size()) i++;

                g_arrays.push_back(arr);
                Token t(TokType::ArrayLiteral, start);
                // offset for Eval to differenciate array (200000) from string (0)
                t.index = (int)g_arrays.size() - 1 + ARRAY_ID_OFFSET;
                tokens.push_back(t);
                expectUnary = false;
            }
            else {
                tokens.emplace_back(OpType::Index, start);
                tokens.emplace_back(TokType::LParen, start);
                i++;
                expectUnary = true;
            }
        }
        else if (expr[i] == ']') {
            tokens.emplace_back(TokType::RParen, start);
            i++;
            expectUnary = false;
        }
        else if (expr[i] == '(') { tokens.emplace_back(TokType::LParen, start); i++; expectUnary = true; }
        else if (expr[i] == ')') { tokens.emplace_back(TokType::RParen, start); i++; expectUnary = false; }
        else if (expr[i] == '?') { tokens.emplace_back(TokType::Quest, start); i++; expectUnary = true; }
        else if (expr[i] == ':') { tokens.emplace_back(TokType::Colon, start); i++; expectUnary = true; }
        else if (expr[i] == '~') { tokens.emplace_back(OpType::BitNot, start); i++; expectUnary = true; }
        else {
            if (expr[i] == '.' && (i + 10 < expr.size()) && expr.substr(i, 11) == ".charCodeAt") {
                tokens.emplace_back(OpType::CharCodeAt, start);
                i += 11;
                expectUnary = true;
                continue;
            }

            if ((expr[i] == '-' || expr[i] == '!') && expectUnary) {
                tokens.emplace_back(expr[i] == '-' ? OpType::Neg : OpType::BitNot, start);
                i++;
            }
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

    // Shunting-yard Algorithm
    for (const auto& t : tokens) {
        if (t.type == TokType::Number || t.type == TokType::VarT || t.type == TokType::Identifier || t.type == TokType::String || t.type == TokType::ArrayLiteral || t.type == TokType::VarPtr) {
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
            if (!stack.empty()) {
                if (stack.back().type == TokType::Fun || (stack.back().type == TokType::Op && (stack.back().op == OpType::Index || stack.back().op == OpType::CharCodeAt))) {
                    m_rpn.push_back(stack.back()); stack.pop_back();
                }
            }
        }
        else {
            // FIX: Ignore commas inside functions like pow(a, b)
            if (t.op == OpType::Coma) {
                while (!stack.empty() && stack.back().type != TokType::LParen) { m_rpn.push_back(stack.back()); stack.pop_back(); }
                bool isArgSeparator = false;
                if (!stack.empty() && stack.back().type == TokType::LParen) {
                    if (stack.size() >= 2 && stack[stack.size() - 2].type == TokType::Fun) isArgSeparator = true;
                }
                if (!isArgSeparator) stack.push_back(t);
            }
            else {
                int prec = (t.type == TokType::Quest || t.type == TokType::Colon) ? 2 :
                    (t.op == OpType::Neg || t.op == OpType::BitNot) ? 12 : getPrecedence(t.op);
                while (!stack.empty() && stack.back().type != TokType::LParen) {
                    int topPrec = (stack.back().type == TokType::Fun) ? 12 :
                        (stack.back().type == TokType::Op && (stack.back().op == OpType::Neg || stack.back().op == OpType::BitNot)) ? 12 :
                        getPrecedence(stack.back().op);
                    if (topPrec < prec) break;
                    m_rpn.push_back(stack.back()); stack.pop_back();
                }
                stack.push_back(t);
            }
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
        case TokType::Identifier: stack[++sp] = memory[tok.index]; break;
        case TokType::String: stack[++sp] = (double)tok.index; break;
        case TokType::ArrayLiteral: stack[++sp] = (double)tok.index; break;
        case TokType::VarPtr: stack[++sp] = (double)tok.index; break; // Throw variable ID to stack

        case TokType::Fun:
            if (sp >= 0) {
                if (tok.fun == FunType::Pow) {
                    if (sp >= 1) {
                        double v2 = stack[sp--]; double& v1 = stack[sp]; v1 = pow(v1, v2);
                    }
                }
                else if (tok.fun == FunType::Tan) {
                    stack[sp] = tan(stack[sp]);
                }
                else if (tok.fun == FunType::Random) {
                    stack[++sp] = (double)rand() / RAND_MAX;
                }
                else {
                    double& v = stack[sp];
                    if (tok.fun == FunType::Sin) v = sin(v);
                    else if (tok.fun == FunType::Cos) v = cos(v);
                    else if (tok.fun == FunType::Abs) v = fabs(v);
                    else if (tok.fun == FunType::Floor) v = floor(v);
                }
            }
            break;
        case TokType::Colon: // Ternary logic
            if (sp >= 2) {
                double f = stack[sp--]; double v = stack[sp--]; double cond = stack[sp];
                stack[sp] = (cond != 0) ? v : f;
            }
            break;
        case TokType::Quest: break; // Ignore '?'
        default:
            if (tok.op == OpType::Neg) { if (sp >= 0) stack[sp] = -stack[sp]; }
            else if (tok.op == OpType::BitNot) { if (sp >= 0) stack[sp] = (double)(~(int64_t)stack[sp]); }

            // --- FIX: Assign to VarPtr ---
            else if (tok.op == OpType::Assign) {
                if (sp >= 1) {
                    double val = stack[sp--];
                    double ptr = stack[sp];
                    int idx = (int)ptr;
                    if (idx >= 0 && idx < memory.size()) const_cast<std::vector<double>&>(memory)[idx] = val;
                    stack[sp] = val;
                }
            }
            else if (tok.op == OpType::Index || tok.op == OpType::CharCodeAt) {
                if (sp >= 1) {
                    double idxVal = stack[sp--]; double targetId = stack[sp]; int tId = (int)targetId;
                    if (tId >= ARRAY_ID_OFFSET) {
                        int arrIndex = tId - ARRAY_ID_OFFSET;
                        if (arrIndex >= 0 && arrIndex < g_arrays.size()) {
                            const vector<double>& arr = g_arrays[arrIndex]; int i = (int)idxVal;
                            if (i >= 0 && i < arr.size()) stack[sp] = arr[i]; else stack[sp] = 0;
                        }
                        else stack[sp] = 0;
                    }
                    else {
                        if (tId >= 0 && tId < g_strings.size()) {
                            const string& s = g_strings[tId]; int i = (int)idxVal;
                            if (i >= 0 && i < s.size()) stack[sp] = (double)s[i]; else stack[sp] = 0;
                        }
                        else stack[sp] = 0;
                    }
                }
            }
            else if (sp >= 1) {
                double b = stack[sp--]; double& a = stack[sp];
                int32_t ia = (int32_t)a; int32_t ib = (int32_t)b;
                switch (tok.op) {
                case OpType::Add: a += b; break;
                case OpType::Sub: a -= b; break;
                case OpType::Mul: a *= b; break;
                case OpType::Div: a = (b != 0) ? (a / b) : 0; break;
                case OpType::Mod: a = (b != 0) ? fmod(a, b) : 0; break;
                    
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

bool ComplexEngine::Compile(const std::string& code, std::string& err, int& errorPos) {
    instructions.clear();
    g_strings.clear();
    g_arrays.clear();

    errorPos = -1;

    struct SegmentData {
        string text;
        size_t offset;
    };
    vector<SegmentData> segments;

    string currentSeg;
    size_t currentSegStart = 0;
    int parenDepth = 0; int bracketDepth = 0; bool inQuote = false; char quoteChar = 0;

    // --- SMART SPLIT: Breaking down into instructions ---
    for (size_t i = 0; i < code.size(); i++) {
        char c = code[i];
        if (inQuote) {
            currentSeg += c; if (c == quoteChar) inQuote = false;
        }
        else {
            if (c == '"' || c == '\'') { inQuote = true; quoteChar = c; currentSeg += c; }
            else if (c == '(') { parenDepth++; currentSeg += c; }
            else if (c == ')') { if (parenDepth > 0) parenDepth--; currentSeg += c; }
            else if (c == '[') { bracketDepth++; currentSeg += c; }
            else if (c == ']') { if (bracketDepth > 0) bracketDepth--; currentSeg += c; }
            else if (c == ',') {
                if (parenDepth == 0 && bracketDepth == 0) {
                    if (!currentSeg.empty()) {
                        segments.push_back({ currentSeg, currentSegStart });
                    }
                    currentSeg.clear();
                    currentSegStart = i + 1;
                }
                else currentSeg += c;
            }
            else currentSeg += c;
        }
    }
    if (!currentSeg.empty()) segments.push_back({ currentSeg, currentSegStart });

    for (auto& segData : segments) {
        string& segment = segData.text;
        size_t segOffset = segData.offset;

        if (segment.find_first_not_of(" \t\n\r") == string::npos) continue;
        Instruction ins;

        // --- SMART ASSIGN DETECT: Ignore '=' inside () ---
        size_t assignPos = string::npos;
        bool isAssign = false;

        int pDepth = 0; bool q = false; char qc = 0;
        for (size_t i = 0; i < segment.size(); ++i) {
            char c = segment[i];
            if (q) { if (c == qc) q = false; }
            else {
                if (c == '"' || c == '\'') { q = true; qc = c; }
                else if (c == '(') pDepth++;
                else if (c == ')') { if (pDepth > 0) pDepth--; }
                else if (c == '=') {
                    if (pDepth == 0) {
                        bool logic = false;
                        if (i > 0 && (segment[i - 1] == '!' || segment[i - 1] == '<' || segment[i - 1] == '>')) logic = true;
                        if (i + 1 < segment.size() && segment[i + 1] == '=') logic = true;
                        if (!logic) { isAssign = true; assignPos = i; break; }
                    }
                }
            }
        }

        int localEp = -1;

        if (isAssign) {
            ins.type = Instruction::Type::AssignVar;
            string varName = segment.substr(0, assignPos);
            varName.erase(remove_if(varName.begin(), varName.end(), ::isspace), varName.end());

            // Get index for allocated variable
            ins.targetVarIdx = state.getVarId(varName);

            if (!ins.expr.Compile(segment.substr(assignPos + 1), err, localEp)) {
                errorPos = (int)(segOffset + assignPos + 1 + localEp);
                return false;
            }
        }
        else {
            ins.type = Instruction::Type::EvalExpr;
            if (!ins.expr.Compile(segment, err, localEp)) {
                errorPos = (int)(segOffset + localEp);
                return false;
            }
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