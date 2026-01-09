#include "raylib.h"
#include "font_data.h"
#include "icon_data.h"
#include <cstdint>
#include <vector>
#include <string>
#include <cctype>
#include <algorithm>
#include <cmath>

using namespace std;

// --- STRUKTURY DANYCH ---
struct EditorState { string text; int cursor; };
vector<EditorState> undoStack;

enum class TokType {
    Number, 
    VarT, 
    Op, 
    LParen,
    RParen,
    Fun,
    Quest, 
    Colon
};
enum class OpType {
    Add,
    Sub,
    Mul,
    Div, 
    Mod,
    And, 
    Or, 
    Xor,
    Shl,
    Shr,
    Neg,
    LT,
    GT,
    LE, 
    GE, 
    EQ, 
    NE, 
    Ternary
};
enum class FunType { 
    Sin, 
    Cos, 
    Abs, 
    Floor,
    Int 
};

struct Token {
    TokType type; 
    double value;
    OpType op; 
    FunType fun;
    int pos;

    static Token number(double v, int p) {
        Token t;
        t.type = TokType::Number; 
        t.value = v;
        t.pos = p;
        return t; 
    }
    static Token varT(int p) {
        Token t;
        t.type = TokType::VarT;
        t.pos = p;
        return t;
    }
    static Token makeOp(OpType o, int p) {
        Token t;
        t.type = TokType::Op;
        t.op = o;
        t.pos = p;
        return t;
    }
    static Token makeFun(FunType f, int p) {
        Token t;
        t.type = TokType::Fun;
        t.fun = f;
        t.pos = p;
        return t;
    }
    static Token lparen(int p) {
        Token t;
        t.type = TokType::LParen;
        t.pos = p;
        return t;
    }
    static Token rparen(int p) {
        Token t;
        t.type = TokType::RParen;
        t.pos = p;
        return t;
    }
};

// --- BYTEBEAT PARSER ---
class BytebeatExpression {
public:
    bool Compile(const string& expr, string& error, int& errorPos) {
        error.clear(); 
        errorPos = -1;
        m_rpn.clear();

        if (expr.empty()) return false;
        vector<Token> tokens;
        bool expectUnary = true;

        // --- 1. TOKENIZATION ---
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
                else { 
                    error = "Unknown function";
                    errorPos = start; 
                    return false; 
                }
                expectUnary = false;
            }
            else if (expr[i] == '(') { tokens.push_back(Token::lparen(start)); i++; expectUnary = true; }
            else if (expr[i] == ')') { tokens.push_back(Token::rparen(start)); i++; expectUnary = false; }
            else if (expr[i] == '?') { tokens.push_back({ TokType::Quest, 0, OpType::Ternary, FunType::Sin, start }); i++; expectUnary = true; }
            else if (expr[i] == ':') { tokens.push_back({ TokType::Colon, 0, OpType::Ternary, FunType::Sin, start }); i++; expectUnary = true; }
            else {
                OpType ot;
                if (expr[i] == '-' && expectUnary) { ot = OpType::Neg; i++; }
                else {
                    char c = expr[i];
                    if (string("+-*/%&|^<>=!").find(c) == string::npos) { 
                        error = "Unknown char"; 
                        errorPos = start;
                        return false; 
                    }
                    if (i + 1 < expr.size()) {
                        string op2 = expr.substr(i, 2);
                        if (op2 == "<<") { ot = OpType::Shl; i += 2; }
                        else if (op2 == ">>") { ot = OpType::Shr; i += 2; }
                        else if (op2 == "<=") { ot = OpType::LE; i += 2; }
                        else if (op2 == ">=") { ot = OpType::GE; i += 2; }
                        else if (op2 == "==") { ot = OpType::EQ; i += 2; }
                        else if (op2 == "!=") { ot = OpType::NE; i += 2; }
                        else goto single_char_val;
                    }
                    else {
                    single_char_val:
                        if (c == '<') ot = OpType::LT; else if (c == '>') ot = OpType::GT;
                        else if (c == '+') ot = OpType::Add; else if (c == '-') ot = OpType::Sub;
                        else if (c == '*') ot = OpType::Mul; else if (c == '/') ot = OpType::Div;
                        else if (c == '%') ot = OpType::Mod; else if (c == '&') ot = OpType::And;
                        else if (c == '|') ot = OpType::Or; else if (c == '^') ot = OpType::Xor;
                        else { 
                            error = "Operator error";
                            errorPos = start; 
                            return false;
                        }
                        i++;
                    }
                }
                tokens.push_back(Token::makeOp(ot, start)); 
                expectUnary = true;
            }
        }

        // --- 2. SHUNTING-YARD ---
        auto getPrec = [](const Token& t) {
            if (t.type == TokType::Quest || t.type == TokType::Colon) return 2;
            if (t.type == TokType::Fun || t.op == OpType::Neg) return 12;

            switch (t.op) {
                case OpType::Mul: 
                case OpType::Div: 
                case OpType::Mod:
                    return 10;

                case OpType::Add: 
                case OpType::Sub:
                    return 9;

                case OpType::Shl: 
                case OpType::Shr: 
                    return 8;

                case OpType::LT: 
                case OpType::GT:
                case OpType::LE:
                case OpType::GE: 
                    return 7;

                case OpType::EQ: 
                case OpType::NE:
                    return 6;

                case OpType::And: return 5;
                case OpType::Xor: return 4;
                case OpType::Or: return 3;

                default: return 0;
            }
        };

        vector<Token> s;
        for (const auto& t : tokens) {
            if (t.type == TokType::Number || t.type == TokType::VarT) m_rpn.push_back(t);
            else if (t.type == TokType::Fun || t.type == TokType::LParen) s.push_back(t);
            else if (t.type == TokType::RParen) {
                bool found = false;
                while (!s.empty()) {
                    if (s.back().type == TokType::LParen) { found = true; break; }
                    m_rpn.push_back(s.back());
                    s.pop_back();
                }
                if (!found) { 
                    error = "Unclosed parenthesis";
                    errorPos = t.pos; 
                    return false; 
                }
                s.pop_back();
                if (!s.empty() && s.back().type == TokType::Fun) {
                    m_rpn.push_back(s.back()); 
                    s.pop_back(); 
                }
            }
            else {
                while (!s.empty() && s.back().type != TokType::LParen && getPrec(s.back()) >= getPrec(t)) {
                    m_rpn.push_back(s.back());
                    s.pop_back();
                }
                s.push_back(t);
            }
        }
        while (!s.empty()) {
            if (s.back().type == TokType::LParen) { 
                error = "Unclosed parenthesis";
                errorPos = s.back().pos;
                return false; 
            }
            m_rpn.push_back(s.back());
            s.pop_back();
        }

        // --- 3. TERNARY OPERATION PREDICTION ---
        int stackSize = 0;
        for (auto& t : m_rpn) {
            if (t.type == TokType::Number || t.type == TokType::VarT) {
                stackSize++;
            }
            else if (t.type == TokType::Fun || (t.type == TokType::Op && t.op == OpType::Neg)) {
                if (stackSize < 1) { 
                    error = "Missing argument"; 
                    errorPos = t.pos;
                    return false; 
                } // 1 in, 1 out
            }
            else if (t.type == TokType::Quest) {
                continue; // Flag ? - awaits for :
            }
            else if (t.type == TokType::Colon) {
                // Ternary operator takes 3 values (condition, ifTrue, ifFalse) and returns 1
                if (stackSize < 3) { 
                    error = "Incomplete operator ?: (expecting 3 values)";
                    errorPos = t.pos;
                    return false; 
                }
                stackSize -= 2;
            }
            else if (t.type == TokType::Op) {
                // Binary operators take 2 values (leftOperand, rightOperand) and return 1
                if (stackSize < 2) {
                    error = "Missing arguments";
                    errorPos = t.pos; 
                    return false; 
                }
                stackSize -= 1;
            }
        }
        if (stackSize > 1) { 
            error = "Operator missing/too much values";
            errorPos = m_rpn.back().pos; 
            return false; 
        }
        if (stackSize < 1) { 
            error = "Empty formula"; 
            return false; 
        }
        return true;
    }

    int Eval(uint32_t t) const {
        if (m_rpn.empty()) return 0;
        static double stack[1024];
        int sp = -1;

        for (const auto& tok : m_rpn) {
            if (tok.type == TokType::Number) stack[++sp] = tok.value;
            else if (tok.type == TokType::VarT) stack[++sp] = (double)t;
            else if (tok.type == TokType::Fun) {
                if (tok.fun == FunType::Sin) stack[sp] = sin(stack[sp]);
                else if (tok.fun == FunType::Cos) stack[sp] = cos(stack[sp]);
                else if (tok.fun == FunType::Abs) stack[sp] = fabs(stack[sp]);
                else stack[sp] = floor(stack[sp]);
            }
            else if (tok.type == TokType::Quest) continue;
            else if (tok.type == TokType::Colon) {
                if (sp >= 2) {
                    double v_false = stack[sp--];
                    double v_true = stack[sp--];
                    double cond = stack[sp--];
                    stack[++sp] = (cond != 0) ? v_true : v_false;
                }
            }
            else {
                if (tok.op == OpType::Neg) {
                    if (sp >= 0) stack[sp] = -stack[sp];
                }
                else if (sp >= 1) {
                    double b = stack[sp--], a = stack[sp];
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
                        case OpType::LT:  stack[sp] = (a < b); break;
                        case OpType::GT:  stack[sp] = (a > b); break;
                        case OpType::LE:  stack[sp] = (a <= b); break;
                        case OpType::GE:  stack[sp] = (a >= b); break;
                        case OpType::EQ:  stack[sp] = (a == b); break;
                        case OpType::NE:  stack[sp] = (a != b); break;
                    }
                }
            }
        }
        return (sp >= 0) ? ((int)stack[0] & 0xFF) : 0;
    }

private:
    vector<Token> m_rpn;
};

// --- GLOBALS ---
BytebeatExpression g_expr;
bool g_playing = false, g_valid = false;
int g_errorPos = -1, g_rateIdx = 0, cursorIndex = 0, selectionAnchor = -1;
string inputBuf = "t*((t&4096?t%65536<59392?7:t&7:16)+(1&t>>14))>>(3&-t>>(t&2048?2:10))|t>>(t&16384?t&4096?10:3:2)", errorMsg;
const int rates[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000 };
uint32_t g_t = 0; 
double g_tAccum = 0.0;
short renderedSamples[2048] = { 0 };
int sampleWriteIdx = 0;

// Init custom font
Font customFont;
float MeasureWidth(const char* text, float size) {
    Vector2 vec = MeasureTextEx(customFont, text, size, 1.0f);
    return vec.x;
}


void MyAudioCallback(void* buffer, unsigned int frames) {
    short* out = (short*)buffer;
    double tInc = (double)rates[g_rateIdx] / 44100.0;
    for (unsigned int i = 0; i < frames; i++) {
        if (g_playing && g_valid) {
            int v = g_expr.Eval(g_t);
            short s = (short)(((v & 0xFF) / 127.5f - 1.0f) * 32767.0f);
            out[i] = s;
            renderedSamples[sampleWriteIdx] = s;
            sampleWriteIdx = (sampleWriteIdx + 1) % 2048;
            g_tAccum += tInc;
            if (g_tAccum >= 1.0) { 
                uint32_t steps = (uint32_t)g_tAccum; 
                g_t += steps;
                g_tAccum -= steps;
            }
        }
        else out[i] = 0;
    }
}

void SaveUndo(const string& t, int c) {
    if (!undoStack.empty() && undoStack.back().text == t) return;
    undoStack.push_back({ t, c });
    if (undoStack.size() > 100) undoStack.erase(undoStack.begin());
}

// --- MAIN ---
int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1000, 650, "C++ Bytebeat Composer");
    InitAudioDevice();

    // Load image internally
    Image appIcon = LoadImageFromMemory(".png", icon_png, icon_png_len);
    ImageFormat(&appIcon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    SetWindowIcon(appIcon);

    SetTargetFPS(60);


    // Load font internally
    customFont = LoadFontFromMemory(
        ".ttf",                     // File extention
        SpaceMono_Regular_ttf,      // Byte array
        SpaceMono_Regular_ttf_len,  // Array len
        64, NULL, 250               // Font size, Font chars, String len
    );
    SetTextureFilter(customFont.texture, TEXTURE_FILTER_BILINEAR);
    float guiFontScale = 1.25f;


    // Load audio stream
    AudioStream stream = LoadAudioStream(44100, 16, 1);
    SetAudioStreamCallback(stream, MyAudioCallback);
    PlayAudioStream(stream);

    g_valid = g_expr.Compile(inputBuf, errorMsg, g_errorPos);
    cursorIndex = (int)inputBuf.size();
    bool editing = false;
    float timers[4] = { 0 };

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_UP)) g_rateIdx = (g_rateIdx + 1) % 7;
        if (IsKeyPressed(KEY_DOWN)) g_rateIdx = (g_rateIdx - 1 + 7) % 7;
        if (IsKeyPressed(KEY_ENTER)) { g_playing = !g_playing; if (!g_playing) g_t = 0; }

        // Input box
        Rectangle inputBox = { 20, 50, 960, 40 * guiFontScale };
        if (CheckCollisionPointRec(GetMousePosition(), inputBox) && IsMouseButtonPressed(0)) editing = true;
        else if (IsMouseButtonPressed(0)) editing = false;

        if (editing) {
            timers[3] += GetFrameTime();
            bool isShift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            bool isCtrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

            auto DeleteSel = [&]() {
                if (selectionAnchor != -1 && selectionAnchor != cursorIndex) {
                    int s = min(selectionAnchor, cursorIndex);
                    inputBuf.erase(s, abs(selectionAnchor - cursorIndex));
                    cursorIndex = s; 
                    selectionAnchor = -1;
                    return true;
                }
                return false;
            };

            if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT)) {
                timers[2] += GetFrameTime();
                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) || timers[2] > 0.4f) {
                    static float tick = 0; 
                    tick += GetFrameTime();
                    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) || tick > 0.03f) {
                        if (isShift && selectionAnchor == -1) selectionAnchor = cursorIndex;
                        else if (!isShift) selectionAnchor = -1;
                        int dir = (IsKeyDown(KEY_LEFT) ? -1 : 1);
                        if (isCtrl) {
                            if (dir == -1) { 
                                while (cursorIndex > 0 && !isalnum(inputBuf[cursorIndex - 1])) cursorIndex--;
                                while (cursorIndex > 0 && isalnum(inputBuf[cursorIndex - 1])) cursorIndex--; 
                            }
                            else { 
                                while (cursorIndex < (int)inputBuf.size() && isalnum(inputBuf[cursorIndex])) cursorIndex++; 
                                while (cursorIndex < (int)inputBuf.size() && !isalnum(inputBuf[cursorIndex])) cursorIndex++; 
                            }
                        }
                        else cursorIndex = max(0, min((int)inputBuf.size(), cursorIndex + dir));
                        tick = 0; 
                        timers[3] = 0;
                    }
                }
            }
            else timers[2] = 0;

            if (isCtrl) {
                if (IsKeyPressed(KEY_A)) { 
                    selectionAnchor = 0;
                    cursorIndex = (int)inputBuf.size();
                }
                if (IsKeyPressed(KEY_C) && selectionAnchor != -1) {
                    int s = min(selectionAnchor, cursorIndex);
                    SetClipboardText(inputBuf.substr(s, abs(selectionAnchor - cursorIndex)).c_str());
                }
                if (IsKeyPressed(KEY_V)) {
                    SaveUndo(inputBuf, cursorIndex);
                    DeleteSel();
                    string clip = GetClipboardText(); 
                    inputBuf.insert(cursorIndex, clip);
                    cursorIndex += (int)clip.size();
                    g_valid = g_expr.Compile(inputBuf, errorMsg, g_errorPos);
                }
                if (IsKeyPressed(KEY_Z) && !undoStack.empty()) {
                    inputBuf = undoStack.back().text;
                    cursorIndex = undoStack.back().cursor;
                    undoStack.pop_back(); 
                    g_valid = g_expr.Compile(inputBuf, errorMsg, g_errorPos);
                }
            }
            else {
                int key = GetCharPressed();
                if (key >= 32 && key <= 125) {
                    SaveUndo(inputBuf, cursorIndex);
                    DeleteSel();
                    inputBuf.insert(cursorIndex, 1, (char)key);
                    cursorIndex++;
                    g_valid = g_expr.Compile(inputBuf, errorMsg, g_errorPos);
                }
            }

            auto HandleDel = [&](int k, float& t, bool bs) {
                if (IsKeyDown(k)) {
                    t += GetFrameTime();
                    if (IsKeyPressed(k) || t > 0.4f) {
                        static float ft = 0; 
                        ft += GetFrameTime();
                        if (IsKeyPressed(k) || ft > 0.04f) {
                            SaveUndo(inputBuf, cursorIndex);
                            if (!DeleteSel()) {
                                if (bs && cursorIndex > 0) inputBuf.erase(--cursorIndex, 1);
                                else if (!bs && cursorIndex < (int)inputBuf.size()) inputBuf.erase(cursorIndex, 1);
                            }
                            g_valid = g_expr.Compile(inputBuf, errorMsg, g_errorPos);
                            ft = 0;
                        }
                    }
                }
                else t = 0;
            };
            HandleDel(KEY_BACKSPACE, timers[0], true);
            HandleDel(KEY_DELETE, timers[1], false);
        }

        BeginDrawing();
        ClearBackground({ 25, 25, 30, 255 });
        DrawRectangleRec(inputBox, editing ? DARKGRAY : BLACK);

        // Scalable font sizes
        float fMain = 22.0f * guiFontScale;
        float fSmall = 18.0f * guiFontScale;
        float fUI = 22.0f * guiFontScale;

        // Draw input text selection
        if (selectionAnchor != -1 && selectionAnchor != cursorIndex) {
            int s = min(selectionAnchor, cursorIndex), e = max(selectionAnchor, cursorIndex);
            float xOffset = MeasureWidth(inputBuf.substr(0, s).c_str(), fMain);
            float w = MeasureWidth(inputBuf.substr(s, e - s).c_str(), fMain);
            DrawRectangle(30 + xOffset, 60, w, fMain * 1.2f, { 0, 120, 255, 100 });
        }

        // Draw bytebeat formula
        Vector2 textPos = { 30, 60 };
        if (g_valid || g_errorPos == -1) DrawTextEx(customFont, inputBuf.c_str(), textPos, fMain, 1.0f, LIME);
        else {
            string p1 = inputBuf.substr(0, g_errorPos), p2 = inputBuf.substr(g_errorPos, 1), p3 = (g_errorPos + 1 < (int)inputBuf.size()) ? inputBuf.substr(g_errorPos + 1) : "";
            float w1 = MeasureWidth(p1.c_str(), fMain), w2 = MeasureWidth(p2.c_str(), fMain);
            DrawTextEx(customFont, p1.c_str(), textPos, fMain, 1.0f, LIME);
            DrawTextEx(customFont, p2.c_str(), { textPos.x + w1, textPos.y }, fMain, 1.0f, RED);
            DrawTextEx(customFont, p3.c_str(), { textPos.x + w1 + w2, textPos.y }, fMain, 1.0f, GRAY);
        }

        // Draw input cursor
        if (editing && (int)(timers[3] * 2) % 2 == 0) {
            float curX = MeasureWidth(inputBuf.substr(0, cursorIndex).c_str(), fMain);
            DrawRectangle(30 + curX, 60, 2, fMain * 1.1f, WHITE);
        }

        // --- Timeline + Error message ---
        float lineY = 80 + (30 * guiFontScale);
        float containerStartX = 20;
        float containerEndX = 980; // 20 + 960 (oscilloscope width)

        // Draw timeline counter
        const char* tText = TextFormat("t: %u", g_t);
        DrawTextEx(customFont, tText, { containerStartX, lineY }, fUI, 1.0f, GRAY);

        // Draw error message
        if (!g_valid && !errorMsg.empty()) {
            Vector2 errSize = MeasureTextEx(customFont, errorMsg.c_str(), fSmall, 1.0f);
            float errX = containerEndX - errSize.x;
            float tWidth = MeasureTextEx(customFont, tText, fUI, 1.0f).x;
            if (errX < containerStartX + tWidth + 20) errX = containerStartX + tWidth + 20;
            DrawTextEx(customFont, errorMsg.c_str(), { errX, lineY }, fSmall, 1.0f, MAROON);
        }

        // Draw sample rate
        DrawTextEx(customFont, TextFormat("Rate: %d Hz (UP/DOWN)", rates[g_rateIdx]), { 20, 110 + (30 * guiFontScale) }, fUI, 1.0f, LIGHTGRAY);

        // Draw oscilloscope view
        DrawRectangleLinesEx({ 20, 200, 960, 380 }, 1, DARKGRAY);
        int tr = 0;
        for (int i = 0; i < 1024; i++) {
            if (renderedSamples[i] < 0 && renderedSamples[i + 1] >= 0) {
                tr = i;
                break;
            }
        }
        for (int x = 0; x < 959; x++) {
            int i1 = (tr + x) % 2048;
            int i2 = (tr + x + 1) % 2048;

            // Calculate rainbow gradient based on X
            // x/959.0f gives range 0.0 - 1.0, then multiply by 360 degrees
            float hue = (x / 959.0f) * 360.0f;
            Color rainbowColor = ColorFromHSV(hue, 0.8f, 1.0f);

            DrawLine(20 + x, 390 - (renderedSamples[i1] / 200), 21 + x, 390 - (renderedSamples[i2] / 200), rainbowColor );
        }

        // Draw playback button
        Color statusColor = (g_playing && g_valid) ? GOLD : (g_playing ? RED : GREEN);
        DrawTextEx(customFont, g_playing ? (g_valid ? "STOP" : "ERROR") : "PLAY (ENTER)", { 20, 610 }, fUI, 1.0f, statusColor);

        EndDrawing();
    }
    UnloadFont(customFont);
    UnloadImage(appIcon);
    UnloadAudioStream(stream);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}