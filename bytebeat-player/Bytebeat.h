#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>

enum class TokType { Number, VarT, Op, LParen, RParen, Fun, Quest, Colon, Identifier };
enum class OpType { Add, Sub, Mul, Div, Mod, And, Or, Xor, Shl, Shr, Neg, BitNot, LT, GT, LE, GE, EQ, NE, Ternary, Assign, Coma };
enum class FunType { Sin, Cos, Abs, Floor, Int };

struct Token {
    TokType type;
    double value;
    OpType op;
    FunType fun;
    int pos;
    std::string name;

    static Token number(double v, int p);
    static Token varT(int p);
    static Token ident(std::string n, int p);
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

class ComplexEngine {
public:
    struct Instruction {
        enum class Type { AssignVar, EvalExpr };
        Type type;
        std::string varName;
        BytebeatExpression expr;
    };

    std::vector<Instruction> instructions;
    bool Compile(const std::string& code, std::string& err);
    int Eval(uint32_t t);
};