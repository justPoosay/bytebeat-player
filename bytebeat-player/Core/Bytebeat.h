#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>

enum class TokType { Number, VarT, Op, LParen, RParen, Fun, Quest, Colon, Identifier };
enum class OpType { Add, Sub, Mul, Div, Mod, And, Or, Xor, Shl, Shr, Neg, BitNot, LT, GT, LE, GE, EQ, NE, Ternary, Assign, Coma };
enum class FunType { Sin, Cos, Abs, Floor };

struct Token {
    TokType type;
    double value = 0.0;
    OpType op = OpType::Add; // Default value
    FunType fun = FunType::Sin; // Default value
    int pos = -1;

    int index = -1;

    // Constructors
    Token() = default;
    Token(double v, int p) : type(TokType::Number), value(v), pos(p) {}
    Token(OpType o, int p) : type(TokType::Op), op(o), pos(p) {}
    Token(FunType f, int p) : type(TokType::Fun), fun(f), pos(p) {}

    // Constructors for variables
    Token(int idx, int p) : type(TokType::Identifier), index(idx), pos(p) {}
    Token(TokType t, int p) : type(t), pos(p) {}
};

class BytebeatExpression {
public:
    bool Compile(const std::string& expr, std::string& error, int& errorPos);
    int Eval(uint32_t t) const;
private:
    std::vector<Token> m_rpn;
};

class ComplexEngine {
public:
    struct Instruction {
        enum class Type { AssignVar, EvalExpr };
        Type type;
        int targetVarIdx = -1;
        BytebeatExpression expr;
    };

    std::vector<Instruction> instructions;
    bool Compile(const std::string& code, std::string& err);
    int Eval(uint32_t t);
};