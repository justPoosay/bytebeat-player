#include "raylib.h"
#include "font_data.h"
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

enum class TokType { Number, VarT, Op, LParen, RParen, Fun };
enum class OpType { Add, Sub, Mul, Div, Mod, And, Or, Xor, Shl, Shr, Neg };
enum class FunType { Sin, Cos, Abs, Floor, Int };

struct Token {
    TokType type; double value; OpType op; FunType fun; int pos;
    static Token number(double v, int p) { Token t; t.type = TokType::Number; t.value = v; t.pos = p; return t; }
    static Token varT(int p) { Token t; t.type = TokType::VarT; t.pos = p; return t; }
    static Token makeOp(OpType o, int p) { Token t; t.type = TokType::Op; t.op = o; t.pos = p; return t; }
    static Token makeFun(FunType f, int p) { Token t; t.type = TokType::Fun; t.fun = f; t.pos = p; return t; }
    static Token lparen(int p) { Token t; t.type = TokType::LParen; t.pos = p; return t; }
    static Token rparen(int p) { Token t; t.type = TokType::RParen; t.pos = p; return t; }
};

// --- KOMPILATOR / PARSER ---
class BytebeatExpression {
public:
    bool Compile(const string& expr, string& error, int& errorPos) {
        error.clear(); errorPos = -1; m_rpn.clear();
        if (expr.empty()) return false;

        vector<Token> tokens;
        bool expectUnary = true;

        for (size_t i = 0; i < expr.size(); ) {
            if (isspace(expr[i])) { i++; continue; }
            int start = (int)i;

            if (isdigit(expr[i])) {
                double v = 0; while (i < expr.size() && (isdigit(expr[i]) || expr[i] == '.')) {
                    if (expr[i] == '.') { i++; /* obsługa floatów w przyszłości */ }
                    else v = v * 10 + (expr[i++] - '0');
                }
                tokens.push_back(Token::number(v, start));
                expectUnary = false;
            }
            else if (isalpha(expr[i])) {
                string name; while (i < expr.size() && isalpha(expr[i])) name += expr[i++];
                if (name == "t") tokens.push_back(Token::varT(start));
                else if (name == "sin") tokens.push_back(Token::makeFun(FunType::Sin, start));
                else if (name == "cos") tokens.push_back(Token::makeFun(FunType::Cos, start));
                else if (name == "abs") tokens.push_back(Token::makeFun(FunType::Abs, start));
                else if (name == "int" || name == "floor") tokens.push_back(Token::makeFun(FunType::Int, start));
                else { error = "Błędna funkcja: " + name; errorPos = start; return false; }
                expectUnary = false;
            }
            else if (expr[i] == '(') { tokens.push_back(Token::lparen(start)); i++; expectUnary = true; }
            else if (expr[i] == ')') { tokens.push_back(Token::rparen(start)); i++; expectUnary = false; }
            else {
                OpType ot;
                if (expr[i] == '-' && expectUnary) { ot = OpType::Neg; i++; }
                else {
                    char c = expr[i];
                    if (string("+-*/%&|^<>").find(c) == string::npos) { error = "Znak nieznany"; errorPos = start; return false; }
                    if ((c == '<' || c == '>') && i + 1 < expr.size() && expr[i + 1] == c) {
                        ot = (c == '<' ? OpType::Shl : OpType::Shr); i += 2;
                    }
                    else {
                        ot = (c == '+') ? OpType::Add : (c == '-') ? OpType::Sub : (c == '*') ? OpType::Mul : (c == '/') ? OpType::Div : (c == '%') ? OpType::Mod : (c == '&') ? OpType::And : (c == '|') ? OpType::Or : OpType::Xor;
                        i++;
                    }
                }
                tokens.push_back(Token::makeOp(ot, start));
                expectUnary = true;
            }
        }

        vector<Token> s;
        auto getPrec = [](const Token& t) {
            if (t.type == TokType::Fun || t.op == OpType::Neg) return 12;
            switch (t.op) {
            case OpType::Mul: case OpType::Div: case OpType::Mod: return 10;
            case OpType::Add: case OpType::Sub: return 9;
            case OpType::Shl: case OpType::Shr: return 8;
            case OpType::And: return 7;
            case OpType::Xor: return 6;
            case OpType::Or:  return 5;
            default: return 0;
            }
            };

        for (const auto& t : tokens) {
            if (t.type == TokType::Number || t.type == TokType::VarT) m_rpn.push_back(t);
            else if (t.type == TokType::Fun || t.type == TokType::LParen) s.push_back(t);
            else if (t.type == TokType::RParen) {
                while (!s.empty() && s.back().type != TokType::LParen) { m_rpn.push_back(s.back()); s.pop_back(); }
                if (s.empty()) { error = "Błąd nawiasów"; return false; } s.pop_back();
                if (!s.empty() && s.back().type == TokType::Fun) { m_rpn.push_back(s.back()); s.pop_back(); }
            }
            else {
                while (!s.empty() && s.back().type != TokType::LParen && getPrec(s.back()) >= getPrec(t)) {
                    m_rpn.push_back(s.back()); s.pop_back();
                }
                s.push_back(t);
            }
        }
        while (!s.empty()) { m_rpn.push_back(s.back()); s.pop_back(); }

        int check = 0;
        for (auto& t : m_rpn) {
            if (t.type == TokType::Number || t.type == TokType::VarT) check++;
            else if (t.type == TokType::Op && t.op != OpType::Neg) check--;
            if (check < 1 && t.type != TokType::Fun) { error = "Błąd składni"; errorPos = t.pos; return false; }
        }
        return check == 1;
    }

    int Eval(uint32_t t) const {
        if (m_rpn.empty()) return 0;
        static double stack[256]; int sp = -1;
        for (const auto& tok : m_rpn) {
            if (tok.type == TokType::Number) stack[++sp] = tok.value;
            else if (tok.type == TokType::VarT) stack[++sp] = (double)t;
            else if (tok.type == TokType::Fun) {
                if (tok.fun == FunType::Sin) stack[sp] = sin(stack[sp]);
                else if (tok.fun == FunType::Cos) stack[sp] = cos(stack[sp]);
                else if (tok.fun == FunType::Abs) stack[sp] = fabs(stack[sp]);
                else stack[sp] = floor(stack[sp]);
            }
            else {
                if (tok.op == OpType::Neg) stack[sp] = -stack[sp];
                else {
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
                    }
                }
            }
        }
        return (int)stack[0] & 0xFF;
    }
private:
    vector<Token> m_rpn;
};

// --- AUDIO & GLOBALS ---
BytebeatExpression g_expr;
bool g_playing = false, g_valid = false;
int g_errorPos = -1, g_rateIdx = 0, cursorIndex = 0, selectionAnchor = -1;
string inputBuf = "t*(42&t>>10)", errorMsg;
const int rates[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000 };
uint32_t g_t = 0; double g_tAccum = 0.0;
short renderedSamples[2048] = { 0 };
int sampleWriteIdx = 0;


Font customFont;
// Funkcja pomocnicza do mierzenia szerokości tekstu naszą czcionką
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
            if (g_tAccum >= 1.0) { uint32_t steps = (uint32_t)g_tAccum; g_t += steps; g_tAccum -= steps; }
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
    SetConfigFlags(FLAG_MSAA_4X_HINT); // Włącza 4-krotny antyaliasing

    InitWindow(1000, 650, "C++ Bytebeat Composer");
    InitAudioDevice();

    // --- NOWA ZMIENNA SKALI ---
    float guiFontScale = 1.25f; // Zmień tutaj (np. 1.5f), aby przeskalować całe UI

    // Zamiast: customFont = LoadFontEx("SF-Pro-Rounded-Medium.otf", 64, 0, 250);
    // Używamy tego:
    customFont = LoadFontFromMemory(
        ".ttf",               // Rozszerzenie (żeby Raylib wiedział jak dekodować)
        SpaceMono_Regular_ttf,     // Wskaźnik do tablicy bajtów z font_data.h
        SpaceMono_Regular_ttf_len,     // Rozmiar tablicy
        64,                   // Rozmiar czcionki (fontSize)
        NULL,                 // Font chars (NULL = domyślne)
        250                   // Liczba znaków
    );
    SetTextureFilter(customFont.texture, TEXTURE_FILTER_BILINEAR);

    AudioStream stream = LoadAudioStream(44100, 16, 1);
    SetAudioStreamCallback(stream, MyAudioCallback);
    PlayAudioStream(stream);

    g_valid = g_expr.Compile(inputBuf, errorMsg, g_errorPos);
    cursorIndex = (int)inputBuf.size();
    bool editing = false;
    float timers[4] = { 0 };

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_UP)) g_rateIdx = (g_rateIdx + 1) % 7;
        if (IsKeyPressed(KEY_DOWN)) g_rateIdx = (g_rateIdx - 1 + 7) % 7;
        if (IsKeyPressed(KEY_ENTER)) { g_playing = !g_playing; if (!g_playing) g_t = 0; }

        // Skalowana wysokość pola inputu
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
                    cursorIndex = s; selectionAnchor = -1; return true;
                }
                return false;
                };

            if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT)) {
                timers[2] += GetFrameTime();
                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) || timers[2] > 0.4f) {
                    static float tick = 0; tick += GetFrameTime();
                    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) || tick > 0.03f) {
                        if (isShift && selectionAnchor == -1) selectionAnchor = cursorIndex;
                        else if (!isShift) selectionAnchor = -1;
                        int dir = (IsKeyDown(KEY_LEFT) ? -1 : 1);
                        if (isCtrl) {
                            if (dir == -1) { while (cursorIndex > 0 && !isalnum(inputBuf[cursorIndex - 1])) cursorIndex--; while (cursorIndex > 0 && isalnum(inputBuf[cursorIndex - 1])) cursorIndex--; }
                            else { while (cursorIndex < (int)inputBuf.size() && isalnum(inputBuf[cursorIndex])) cursorIndex++; while (cursorIndex < (int)inputBuf.size() && !isalnum(inputBuf[cursorIndex])) cursorIndex++; }
                        }
                        else cursorIndex = max(0, min((int)inputBuf.size(), cursorIndex + dir));
                        tick = 0; timers[3] = 0;
                    }
                }
            }
            else timers[2] = 0;

            if (isCtrl) {
                if (IsKeyPressed(KEY_A)) { selectionAnchor = 0; cursorIndex = (int)inputBuf.size(); }
                if (IsKeyPressed(KEY_C) && selectionAnchor != -1) {
                    int s = min(selectionAnchor, cursorIndex);
                    SetClipboardText(inputBuf.substr(s, abs(selectionAnchor - cursorIndex)).c_str());
                }
                if (IsKeyPressed(KEY_V)) {
                    SaveUndo(inputBuf, cursorIndex); DeleteSel();
                    string clip = GetClipboardText(); inputBuf.insert(cursorIndex, clip);
                    cursorIndex += (int)clip.size(); g_valid = g_expr.Compile(inputBuf, errorMsg, g_errorPos);
                }
                if (IsKeyPressed(KEY_Z) && !undoStack.empty()) {
                    inputBuf = undoStack.back().text; cursorIndex = undoStack.back().cursor;
                    undoStack.pop_back(); g_valid = g_expr.Compile(inputBuf, errorMsg, g_errorPos);
                }
            }
            else {
                int key = GetCharPressed();
                if (key >= 32 && key <= 125) {
                    SaveUndo(inputBuf, cursorIndex); DeleteSel();
                    inputBuf.insert(cursorIndex, 1, (char)key); cursorIndex++;
                    g_valid = g_expr.Compile(inputBuf, errorMsg, g_errorPos);
                }
            }

            auto HandleDel = [&](int k, float& t, bool bs) {
                if (IsKeyDown(k)) {
                    t += GetFrameTime();
                    if (IsKeyPressed(k) || t > 0.4f) {
                        static float ft = 0; ft += GetFrameTime();
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

        // Skalowane rozmiary czcionek
        float fMain = 22.0f * guiFontScale;
        float fSmall = 18.0f * guiFontScale;
        float fUI = 22.0f * guiFontScale;

        // Rysowanie zaznaczenia
        if (selectionAnchor != -1 && selectionAnchor != cursorIndex) {
            int s = min(selectionAnchor, cursorIndex), e = max(selectionAnchor, cursorIndex);
            float xOffset = MeasureWidth(inputBuf.substr(0, s).c_str(), fMain);
            float w = MeasureWidth(inputBuf.substr(s, e - s).c_str(), fMain);
            DrawRectangle(30 + xOffset, 60, w, fMain * 1.2f, { 0, 120, 255, 100 });
        }

        // Renderowanie tekstu formuły
        Vector2 textPos = { 30, 60 };
        if (g_valid || g_errorPos == -1) {
            DrawTextEx(customFont, inputBuf.c_str(), textPos, fMain, 1.0f, LIME);
        }
        else {
            string p1 = inputBuf.substr(0, g_errorPos), p2 = inputBuf.substr(g_errorPos, 1), p3 = (g_errorPos + 1 < (int)inputBuf.size()) ? inputBuf.substr(g_errorPos + 1) : "";
            float w1 = MeasureWidth(p1.c_str(), fMain), w2 = MeasureWidth(p2.c_str(), fMain);
            DrawTextEx(customFont, p1.c_str(), textPos, fMain, 1.0f, LIME);
            DrawTextEx(customFont, p2.c_str(), { textPos.x + w1, textPos.y }, fMain, 1.0f, RED);
            DrawTextEx(customFont, p3.c_str(), { textPos.x + w1 + w2, textPos.y }, fMain, 1.0f, GRAY);
            DrawTextEx(customFont, errorMsg.c_str(), { 20, 70 + (40 * guiFontScale) }, fSmall, 1.0f, MAROON);
        }

        // Kursor
        if (editing && (int)(timers[3] * 2) % 2 == 0) {
            float curX = MeasureWidth(inputBuf.substr(0, cursorIndex).c_str(), fMain);
            DrawRectangle(30 + curX, 60, 2, fMain * 1.1f, WHITE);
        }

        // Oscyloskop
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

            // Obliczamy kolor tęczy na podstawie pozycji X
            // x / 959.0f daje zakres 0.0 - 1.0, mnożymy przez 360 stopni
            float hue = (x / 959.0f) * 360.0f;
            Color rainbowColor = ColorFromHSV(hue, 0.8f, 1.0f);

            DrawLine(
                20 + x, 390 - (renderedSamples[i1] / 200),
                21 + x, 390 - (renderedSamples[i2] / 200),
                rainbowColor
            );
        }

        // Reszta UI ze skalowaniem pozycji Y
        DrawTextEx(customFont, TextFormat("Rate: %d Hz (UP/DOWN)", rates[g_rateIdx]), { 20, 110 + (30 * guiFontScale) }, fUI, 1.0f, LIGHTGRAY);
        DrawTextEx(customFont, TextFormat("t: %u", g_t), { 20, 80 + (30 * guiFontScale) }, fUI, 1.0f, GRAY);

        Color statusColor = (g_playing && g_valid) ? GOLD : (g_playing ? RED : GREEN);
        DrawTextEx(customFont, g_playing ? (g_valid ? "STOP" : "ERROR") : "PLAY (ENTER)", { 20, 610 }, fUI, 1.0f, statusColor);

        EndDrawing();
    }
    UnloadFont(customFont); UnloadAudioStream(stream); CloseAudioDevice(); CloseWindow();
    return 0;
}