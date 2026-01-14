#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class TokType { Number, VarT, Op, LParen, RParen, Fun, Quest, Colon };
enum class OpType { Add, Sub, Mul, Div, Mod, And, Or, Xor, Shl, Shr, Neg, BitNot, LT, GT, LE, GE, EQ, NE, Ternary };
enum class FunType { Sin, Cos, Abs, Floor, Int };

struct Token {
    TokType type;
    double value;
    OpType op;
    FunType fun;
    int pos;

    static Token number(double v, int p);
    static Token varT(int p);
    static Token makeOp(OpType o, int p);
    static Token makeFun(FunType f, int p);
    static Token lparen(int p);
    static Token rparen(int p);
};

class BytebeatExpression {
public:
    bool Compile(const std::string& expr, std::string& error, int& errorPos);
    int Eval(uint32_t t) const;
private:
    std::vector<Token> m_rpn;
};