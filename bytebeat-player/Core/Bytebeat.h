#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>

enum class TokType { Number, VarT, Op, LParen, RParen, Fun, Quest, Colon, Identifier, String, ArrayLiteral, VarPtr };
enum class OpType { Add, Sub, Mul, Div, Mod, And, Or, Xor, Shl, Shr, Neg, BitNot, LT, GT, LE, GE, EQ, NE, Ternary, Assign, Coma, CharCodeAt, Index, Length };
enum class FunType { Sin, Cos, Abs, Floor, Tan, Pow, Random };

struct Token {
    TokType type;
    double value = 0.0;
    OpType op = OpType::Add;
    FunType fun = FunType::Sin;
    int pos = -1;
    int index = -1;

    Token() = default;
    Token(double v, int p) : type(TokType::Number), value(v), pos(p) {}
    Token(OpType o, int p) : type(TokType::Op), op(o), pos(p) {}
    Token(FunType f, int p) : type(TokType::Fun), fun(f), pos(p) {}
    Token(int idx, int p) : type(TokType::Identifier), index(idx), pos(p) {}
    Token(TokType t, int p) : type(t), pos(p) {}
    Token(int idx, int p, bool isPtr) : type(isPtr ? TokType::VarPtr : TokType::Identifier), index(idx), pos(p) {}
};

class BytebeatExpression {
public:
    bool Compile(const std::string& expr, std::string& error, int& errorPos);
    double Eval(uint32_t t) const;
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
    bool Compile(const std::string& code, std::string& err, int& errorPos);
    int Eval(uint32_t t);
};