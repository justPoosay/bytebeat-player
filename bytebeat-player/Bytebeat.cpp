#include "Bytebeat.h"
#include <cmath>
#include <cctype>
#include <algorithm>
#include <string>

using namespace std;

Token Token::number(double v, int p) { Token t; t.type = TokType::Number; t.value = v; t.pos = p; return t; }
Token Token::varT(int p) { Token t; t.type = TokType::VarT; t.pos = p; return t; }
Token Token::makeOp(OpType o, int p) { Token t; t.type = TokType::Op; t.op = o; t.pos = p; return t; }
Token Token::makeFun(FunType f, int p) { Token t; t.type = TokType::Fun; t.fun = f; t.pos = p; return t; }
Token Token::lparen(int p) { Token t; t.type = TokType::LParen; t.pos = p; return t; }
Token Token::rparen(int p) { Token t; t.type = TokType::RParen; t.pos = p; return t; }

bool BytebeatExpression::Compile(const string& expr, string& error, int& errorPos) {
    error.clear(); errorPos = -1; m_rpn.clear();
    if (expr.empty()) return false;
    vector<Token> tokens;
    bool expectUnary = true;

    for (size_t i = 0; i < expr.size(); ) {
        if (isspace(expr[i])) { i++; continue; }
        int start = (int)i;

        if (isdigit(expr[i])) {
            double v = 0;
            while (i < expr.size() && isdigit(expr[i])) v = v * 10 + (expr[i++] - '0');
            tokens.push_back(Token::number(v, start));
            expectUnary = false;
        }
        else if (isalpha(expr[i])) {
            string name;
            while (i < expr.size() && isalpha(expr[i])) name += expr[i++];
            if (name == "t") tokens.push_back(Token::varT(start));
            else if (name == "sin") tokens.push_back(Token::makeFun(FunType::Sin, start));
            else if (name == "cos") tokens.push_back(Token::makeFun(FunType::Cos, start));
            else if (name == "abs") tokens.push_back(Token::makeFun(FunType::Abs, start));
            else if (name == "int" || name == "floor") tokens.push_back(Token::makeFun(FunType::Int, start));
            else { error = "Unknown function"; errorPos = start; return false; }
            expectUnary = false;
        }
        else if (expr[i] == '(') { tokens.push_back(Token::lparen(start)); i++; expectUnary = true; }
        else if (expr[i] == ')') { tokens.push_back(Token::rparen(start)); i++; expectUnary = false; }
        else if (expr[i] == '?') { tokens.push_back({ TokType::Quest, 0, OpType::Ternary, FunType::Sin, start }); i++; expectUnary = true; }
        else if (expr[i] == ':') { tokens.push_back({ TokType::Colon, 0, OpType::Ternary, FunType::Sin, start }); i++; expectUnary = true; }
        else if (expr[i] == '~') { tokens.push_back(Token::makeOp(OpType::BitNot, (int)i)); i++; expectUnary = true; }
        else {
            OpType ot;
            if (expr[i] == '-' && expectUnary) { ot = OpType::Neg; i++; }
            else {
                char c = expr[i];
                if (string("+-*/%&|^<>!=").find(c) == string::npos) { error = "Unknown char"; errorPos = start; return false; }

                if (i + 1 < expr.size()) {
                    string op2 = expr.substr(i, 2);
                    if (op2 == "<<") { ot = OpType::Shl; i += 2; }
                    else if (op2 == ">>") { ot = OpType::Shr; i += 2; }
                    else if (op2 == "<=") { ot = OpType::LE; i += 2; }
                    else if (op2 == ">=") { ot = OpType::GE; i += 2; }
                    else if (op2 == "==") { ot = OpType::EQ; i += 2; }
                    else if (op2 == "!=") { ot = OpType::NE; i += 2; }
                    else goto single_op;
                }
                else {
                single_op:
                    if (c == '<') ot = OpType::LT; else if (c == '>') ot = OpType::GT;
                    else if (c == '+') ot = OpType::Add; else if (c == '-') ot = OpType::Sub;
                    else if (c == '*') ot = OpType::Mul; else if (c == '/') ot = OpType::Div;
                    else if (c == '%') ot = OpType::Mod; else if (c == '&') ot = OpType::And;
                    else if (c == '|') ot = OpType::Or;  else if (c == '^') ot = OpType::Xor;
                    else { error = "Syntax error"; errorPos = start; return false; }
                    i++;
                }
            }
            tokens.push_back(Token::makeOp(ot, start));
            expectUnary = true;
        }
    }

    auto getPrec = [](const Token& t) {
        if (t.type == TokType::Quest || t.type == TokType::Colon) return 2;
        if (t.type == TokType::Fun || (t.type == TokType::Op && (t.op == OpType::Neg || t.op == OpType::BitNot))) return 12;
        switch (t.op) {
        case OpType::Mul: case OpType::Div: case OpType::Mod: return 10;
        case OpType::Add: case OpType::Sub: return 9;
        case OpType::Shl: case OpType::Shr: return 8;
        case OpType::LT: case OpType::GT: case OpType::LE: case OpType::GE: return 7;
        case OpType::EQ: case OpType::NE: return 6;
        case OpType::And: return 5; case OpType::Xor: return 4; case OpType::Or: return 3;
        default: return 0;
        }
        };

    vector<Token> s;
    for (const auto& t : tokens) {
        if (t.type == TokType::Number || t.type == TokType::VarT) m_rpn.push_back(t);
        else if (t.type == TokType::Fun || t.type == TokType::LParen) s.push_back(t);
        else if (t.type == TokType::RParen) {
            while (!s.empty() && s.back().type != TokType::LParen) { m_rpn.push_back(s.back()); s.pop_back(); }
            if (!s.empty()) s.pop_back();
            if (!s.empty() && s.back().type == TokType::Fun) { m_rpn.push_back(s.back()); s.pop_back(); }
        }
        else {
            while (!s.empty() && s.back().type != TokType::LParen && getPrec(s.back()) >= getPrec(t)) { m_rpn.push_back(s.back()); s.pop_back(); }
            s.push_back(t);
        }
    }
    while (!s.empty()) { m_rpn.push_back(s.back()); s.pop_back(); }
    return !m_rpn.empty();
}

int BytebeatExpression::Eval(uint32_t t) const {
    if (m_rpn.empty()) return 0;
    double stack[1024];
    int sp = -1;
    for (const auto& tok : m_rpn) {
        if (sp >= 1023) break;
        if (tok.type == TokType::Number) stack[++sp] = tok.value;
        else if (tok.type == TokType::VarT) stack[++sp] = (double)t;
        else if (tok.type == TokType::Fun) {
            if (sp >= 0) {
                if (tok.fun == FunType::Sin) stack[sp] = sin(stack[sp]);
                else if (tok.fun == FunType::Cos) stack[sp] = cos(stack[sp]);
                else if (tok.fun == FunType::Abs) stack[sp] = fabs(stack[sp]);
                else stack[sp] = floor(stack[sp]);
            }
        }
        else if (tok.type == TokType::Quest) continue;
        else if (tok.type == TokType::Colon) {
            if (sp >= 2) {
                double f = stack[sp--];
                double v = stack[sp--];
                double cond = stack[sp--];
                stack[++sp] = (cond != 0) ? v : f;
            }
        }
        else {
            if (tok.op == OpType::Neg) {
                if (sp >= 0) stack[sp] = -stack[sp];
            }
            else if (tok.op == OpType::BitNot) {
                if (sp >= 0) stack[sp] = (double)(~(int64_t)stack[sp]);
            }
            else if (sp >= 1) {
                double b = stack[sp--];
                double a = stack[sp];
                switch (tok.op) {
                case OpType::Add: stack[sp] = a + b; break;
                case OpType::Sub: stack[sp] = a - b; break;
                case OpType::Mul: stack[sp] = a * b; break;
                case OpType::Div: stack[sp] = b != 0 ? a / b : 0; break;
                case OpType::Mod: stack[sp] = b != 0 ? fmod(a, b) : 0; break;
                case OpType::And: stack[sp] = (int64_t)a & (int64_t)b; break;
                case OpType::Or:  stack[sp] = (int64_t)a | (int64_t)b; break;
                case OpType::Xor: stack[sp] = (int64_t)a ^ (int64_t)b; break;
                case OpType::Shl: stack[sp] = (int64_t)a << (int64_t)b; break;
                case OpType::Shr: stack[sp] = (uint32_t)a >> (uint32_t)b; break;
                case OpType::LT:  stack[sp] = a < b; break;
                case OpType::GT:  stack[sp] = a > b; break;
                case OpType::LE:  stack[sp] = a <= b; break;
                case OpType::GE:  stack[sp] = a >= b; break;
                case OpType::EQ:  stack[sp] = a == b; break;
                case OpType::NE:  stack[sp] = a != b; break;
                }
            }
        }
    }
    return (sp >= 0) ? ((int)stack[0] & 0xFF) : 0;
}