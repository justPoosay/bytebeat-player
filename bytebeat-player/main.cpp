#define _CRT_SECURE_NO_WARNINGS

#include "raylib.h"

// Rozwiązanie konfliktów nazw między Raylib a Windows API
#define RAYLIB_H
#ifndef NO_WINDESK
#define NO_WINDESK
#endif

#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include "TextEditor.h"

// TWOJE PLIKI Z DANYMI
#include "font_data_2.h" 
#include "icon_data.h" 

#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <string>

using namespace std;

// ==========================================================
// 1:1 LOGIKA Z TWOJEGO PLIKU (PARSER + EVAL)
// ==========================================================

enum class TokType { Number, VarT, Op, LParen, RParen, Fun, Quest, Colon };
enum class OpType { Add, Sub, Mul, Div, Mod, And, Or, Xor, Shl, Shr, Neg, BitNot, LT, GT, LE, GE, EQ, NE, Ternary };
enum class FunType { Sin, Cos, Abs, Floor, Int };

struct Token {
    TokType type;
    double value;
    OpType op;
    FunType fun;
    int pos;

    static Token number(double v, int p) { Token t; t.type = TokType::Number; t.value = v; t.pos = p; return t; }
    static Token varT(int p) { Token t; t.type = TokType::VarT; t.pos = p; return t; }
    static Token makeOp(OpType o, int p) { Token t; t.type = TokType::Op; t.op = o; t.pos = p; return t; }
    static Token makeFun(FunType f, int p) { Token t; t.type = TokType::Fun; t.fun = f; t.pos = p; return t; }
    static Token lparen(int p) { Token t; t.type = TokType::LParen; t.pos = p; return t; }
    static Token rparen(int p) { Token t; t.type = TokType::RParen; t.pos = p; return t; }
};

struct BytebeatPreset {
    string title;
    string code;
    int sampleRate;

    // Konstruktor domyślny i z parametrami
    BytebeatPreset(string t, string c, int sr = 8000)
        : title(t), code(c), sampleRate(sr) {
    }
};

vector<BytebeatPreset> g_presets = {
    {"The 42 melody", "t*(42&t>>10)"},
    {"Space Engine", "t*((t>>12|t>>8)&63&t>>4)"},
    {"Remix of Meowing Cat", "t*((t/2>>10|t%16*t>>8)&8*t>>12&18)|-(t/16)+64"},
    {"Neurofunk", "t*((t&4096?t%65536<59392?7:t&7:16)+(1&t>>14))>>(3&-t>>(t&2048?2:10))|t>>(t&16384?t&4096?10:3:2)"},
    {"Explosions", "(t>>2)*(t>>5)|t>>5"},
    {"Siren", "(t*5&t>>7)|(t*3&t>>10)"},
    {"Minimal sierpinsky", "t&t>>8"},
    {"Sierpinsky harmony", "5*t&t>>7|3*t&4*t>>10"},
    {"Remix of Lost in Space", "(t>>7|t|t>>6)*10+4*(t&t>>13|t>>6)"},
    {"Stimmer", "t*(4|t>>13&3)>>(~t>>11&1)&128|t*(t>>11&t>>13)*(~t>>9&3)&127"},
    {"Remix of Fractal melody", "(((t/10|0)^(t/10|0)-1280)%11*t/2&127)+(((t/640|0)^(t/640|0)-2)%13*t/2&127)", 11025},
    {"Fractalized Past", "(t>>10^t>>11)%5*((t>>14&3^t>>15&1)+1)*t%99+((3+(t>>14&3)-(t>>16&1))/3*t%99&64)"},
    {"Stress Signal", "(t/(t>>9&15)&32)*((-t>>16&t>>20|t>>21)&118518023>>(t>>11)&1)+(t/(t%152)&-t>>7&t>>10&t>>17&63)+(4*t/(3353657496>>(t>>18<<3)&255)&23)+(8*t/(3370500505>>(t>>18<<3)&255)&23)+(5*t/24&-t>>9&13)+(5*t/32&-t>>8&13)+(t/4-t/64*(-t>>17&1)&-t/3>>8&13)", 44100},
    {"Random Note Generator 2", "t/2*(6+((t^t/2^t/4&t/8)>>13&3))/6%128+t/4*(6+(t>>18&3))/6%128", 44100},
    {"Random Note Generator 3", "t*((-1-t*(6+((t^t/4&t/2)>>12&3))/6&255)>=(t/256&127))/128%256/3+t*(6+(t>>18&3))/12%256/4+((2<<17)/(t%(1<<15))&128)/2+((t-t/256)%256/4+(t+t/128)*7/6%256/4+(t*8/6+t/256)%256/4+(t*9/6-t/512)%256/4)*2/5", 44100},


    // Waves
    {"Triangle waves", "(t<<4)^-(t>>4&1)"},
    {"Sine waves", "(t&15)*(-t&15)*(((t&16)/8)-1)*128/65+128"}

};

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

    int Eval(uint32_t t) const {
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
private:
    vector<Token> m_rpn;
};

// ==========================================================
// RESZTA PROGRAMU (DOCKING + RAYLIB)
// ==========================================================

BytebeatExpression g_expr;
TextEditor g_editor; // Nowy obiekt edytora
bool g_playing = false, g_valid = false;
char g_inputBuf[2048] = "t*((-1-t*(6+((t^t/4&t/2)>>12&3))/6&255)>=(t/256&127))/128%256/3+t*(6+(t>>18&3))/12%256/4+((2<<17)/(t%(1<<15))&128)/2+((t-t/256)%256/4+(t+t/128)*7/6%256/4+(t*8/6+t/256)%256/4+(t*9/6-t/512)%256/4)*2/5";
string g_errorMsg;
int g_errorPos = -1;
uint32_t g_t = 0;
double g_tAccum = 0.0;
float g_volume = 1.0f;
float g_scope[256] = { 0 };
int g_scopeIdx = 0;
float g_zoomFactors[] = { 1.0f, 2.0f, 4.0f, 8.0f };
int g_zoomIdx = 0; // Zaczynamy od największego zbliżenia

const int g_rates[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000 };
const char* g_rateNames[] = { "8000 Hz", "11025 Hz", "16000 Hz", "22050 Hz", "32000 Hz", "44100 Hz", "48000 Hz" };
int g_rateIdx = 5;

float g_exportProgress = -1.0f; // -1 oznacza, że nie eksportujemy
float g_successMsgTimer = 0.0f; // Odlicza czas wyświetlania komunikatu
std::string g_fileName = "";

void MyAudioCallback(void* buffer, unsigned int frames) {
    short* out = (short*)buffer;
    double tInc = (double)g_rates[g_rateIdx] / 44100.0;
    for (unsigned int i = 0; i < frames; i++) {
        if (g_playing && g_valid) {
            int v = g_expr.Eval(g_t);
            float sample = ((v & 0xFF) / 127.5f - 1.0f);
            out[i] = (short)(sample * 32767.0f * g_volume);

            if (i % 8 == 0) {
                g_scope[g_scopeIdx] = sample;
                g_scopeIdx = (g_scopeIdx + 1) % 256;
            }

            g_tAccum += tInc;
            if (g_tAccum >= 1.0) {
                uint32_t steps = (uint32_t)g_tAccum;
                g_t += steps;
                g_tAccum -= (double)steps;
            }

            if (g_tAccum > 100.0) g_tAccum = 0.0;
        }
        else out[i] = 0;
    }
}


void ApplyTheme(int themeIdx) {
    ImGuiStyle& style = ImGui::GetStyle();
    switch (themeIdx) {
    case 0:
        ImGui::StyleColorsDark(); 
        g_editor.SetPalette(TextEditor::GetDarkPalette());
        break;
    case 1: ImGui::StyleColorsLight();
        g_editor.SetPalette(TextEditor::GetLightPalette());
        break;
    case 2: ImGui::StyleColorsClassic(); 
        break;
    case 3: // Motyw "Cyberpunk/Matrix"
        ImGui::StyleColorsDark();
        g_editor.SetPalette(TextEditor::GetRetroBluePalette());
        style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 1.00f, 0.00f, 1.00f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.40f, 0.10f, 1.00f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.60f, 0.10f, 1.00f);
        break;
    case 4: // Motyw "Deep Ocean"
        ImGui::StyleColorsDark();
        g_editor.SetPalette(TextEditor::GetDarkPalette());
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.05f, 0.10f, 1.00f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.30f, 0.60f, 1.00f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.15f, 0.40f, 0.70f, 1.00f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.15f, 0.30f, 1.00f);
        break;
    }
    // Opcjonalnie: zaokrąglenie rogów dla nowoczesnego wyglądu
    style.WindowRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
}

void ExportToWav() {
    if (!g_valid) return;

    // --- GENEROWANIE NAZWY PLIKU Z DATĄ ---
    time_t now = time(0);
    tm* ltm = localtime(&now);
    char timeStr[64];
    // Format: Rok-Miesiac-Dzien_Godzina-Minuta
    strftime(timeStr, sizeof(timeStr), "%d-%m-%Y_%H-%M", ltm);

    std::string fileName = "bytebeat_" + std::string(timeStr) + ".wav";
    g_fileName = fileName;
    // --------------------------------------

    const int exportRate = 44100;
    int targetRate = g_rates[g_rateIdx];
    uint32_t seconds = 30;
    uint32_t totalSamples = seconds * exportRate;

    FILE* f = fopen(fileName.c_str(), "wb"); // Używamy fileName zamiast "export.wav"
    if (!f) return;

    // NAGŁÓWEK WAV
    fwrite("RIFF", 1, 4, f);
    uint32_t chunkSize = 36 + totalSamples; fwrite(&chunkSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    uint32_t s1 = 16; fwrite(&s1, 4, 1, f);
    uint16_t alg = 1; fwrite(&alg, 2, 1, f);
    uint16_t ch = 1; fwrite(&ch, 2, 1, f);
    fwrite(&exportRate, 4, 1, f);
    uint32_t br = exportRate; fwrite(&br, 4, 1, f);
    uint16_t ba = 1; fwrite(&ba, 2, 1, f);
    uint16_t bps = 8; fwrite(&bps, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&totalSamples, 4, 1, f);

    // DANE (Z izolacją tLocal)
    double tAccumLocal = 0.0;
    uint32_t tLocal = 0;
    double tInc = (double)targetRate / (double)exportRate;

    for (uint32_t i = 0; i < totalSamples; i++) {
        int v = g_expr.Eval(tLocal);
        fputc((unsigned char)(v & 0xFF), f);

        tAccumLocal += tInc;
        if (tAccumLocal >= 1.0) {
            uint32_t steps = (uint32_t)tAccumLocal;
            tLocal += steps;
            tAccumLocal -= (double)steps;
        }

        if (i % 5000 == 0) g_exportProgress = (float)i / totalSamples;
    }

    fclose(f);
    g_exportProgress = -1.0f;
    g_successMsgTimer = 3.0f;
}

// Ta funkcja szuka momentu, w którym fala przecina środek idąc w górę
uint32_t FindTrigger(uint32_t currentT) {
    const int searchRange = 1024;
    for (uint32_t i = 0; i < searchRange; i++) {
        int valNow = g_expr.Eval(currentT + i) & 0xFF;
        int valNext = g_expr.Eval(currentT + i + 1) & 0xFF;

        // Szukamy absolutnego dołu fali (blisko 0), żeby zaczynała od lewego dolnego rogu
        if (valNow <= 2 && valNext > 2) return currentT + i;
    }
    return currentT;
}

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Bytebeat Composer Pro - Fixed Logic");
    SetTargetFPS(144);
    InitAudioDevice();

    Image icon = LoadImageFromMemory(".png", icon_png, icon_png_len);
    if (icon.data != NULL) {
        ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        SetWindowIcon(icon);
        UnloadImage(icon);
    }

    AudioStream stream = LoadAudioStream(44100, 16, 1);
    SetAudioStreamCallback(stream, MyAudioCallback);
    PlayAudioStream(stream);

    // Konfiguracja kolorowania składni i domyślny tekst
    auto lang = TextEditor::LanguageDefinition::C();
    g_editor.SetLanguageDefinition(lang);
    g_editor.SetText(g_inputBuf);

    g_valid = g_expr.Compile(g_inputBuf, g_errorMsg, g_errorPos);

    rlImGuiSetup(true);
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.35f;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;

    // VERY VULNERABLE FONT
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF(SF_Pro_Rounded_Medium_otf, SF_Pro_Rounded_Medium_otf_len, 64.0f, &fontConfig);

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    bool firstFrame = true;

    int g_themeIdx = 4;
    ApplyTheme(g_themeIdx);
    const char* g_themeNames[] = { "Dark", "Light", "Classic", "Matrix Green", "Deep Ocean" };

    while (!WindowShouldClose()) {
        if (!io.WantCaptureKeyboard && IsKeyPressed(KEY_ENTER)) g_playing = !g_playing;

        BeginDrawing();
        ClearBackground({ 15, 15, 20, 255 });
        rlImGuiBegin();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(viewport->ID, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

        if (firstFrame) {
            firstFrame = false;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            // 1. Dzielimy główny obszar na GÓRĘ i DÓŁ (50/50)
            ImGuiID dock_id_bottom;
            ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.5f, NULL, &dock_id_bottom);

            // 2. Dzielimy GÓRĘ na LEWO (Editor) i PRAWO (Presets)
            ImGuiID dock_id_top_right;
            ImGuiID dock_id_top_left = ImGui::DockBuilderSplitNode(dock_id_top, ImGuiDir_Left, 0.5f, NULL, &dock_id_top_right);

            // 3. Dzielimy DÓŁ na LEWO (Settings) i PRAWO (Oscilloscope)
            ImGuiID dock_id_bottom_right;
            ImGuiID dock_id_bottom_left = ImGui::DockBuilderSplitNode(dock_id_bottom, ImGuiDir_Left, 0.5f, NULL, &dock_id_bottom_right);

            // 4. Przypisujemy okna do stworzonych węzłów
            ImGui::DockBuilderDockWindow("Editor", dock_id_top_left);
            ImGui::DockBuilderDockWindow("Presets", dock_id_top_right);
            ImGui::DockBuilderDockWindow("Settings", dock_id_bottom_left);
            ImGui::DockBuilderDockWindow("Oscilloscope", dock_id_bottom_right);

            ImGui::DockBuilderFinish(dockspace_id);
        }


        ImGui::Begin("Editor");
        {
            // Renderowanie profesjonalnego edytora (zamiast InputTextMultiline)
            // -60 zostawia miejsce na przyciski PLAY/Reset na dole
            g_editor.Render("TextEditor", ImVec2(0, -60));

            // Pobieramy tekst z edytora i sprawdzamy czy się zmienił
            string currentCode = g_editor.GetText();
            static string lastCode;

            if (currentCode != lastCode) {
                // Kopiujemy do bufora i kompilujemy
                strncpy(g_inputBuf, currentCode.c_str(), sizeof(g_inputBuf) - 1);
                g_valid = g_expr.Compile(g_inputBuf, g_errorMsg, g_errorPos);
                lastCode = currentCode;
            }

            if (ImGui::Button(g_playing ? "STOP" : "PLAY", ImVec2(100, 40))) g_playing = !g_playing;
            
            ImGui::SameLine();
            if (ImGui::Button("Reset", ImVec2(100, 40))) { g_t = 0; g_tAccum = 0.0; }

            char zoomBuf[32];
            // Mapowanie: index 0 -> "1", index 1 -> "1/2", index 2 -> "1/4", itd.
            if (g_zoomIdx == 0) sprintf(zoomBuf, "1x");
            else sprintf(zoomBuf, "1/%dx", (int)g_zoomFactors[g_zoomIdx]);

            ImGui::SameLine();
            if (ImGui::Button(zoomBuf, ImVec2(60, 40))) {
                g_zoomIdx = (g_zoomIdx + 1) % 4;
            }

            if (!g_valid) {
                string errStr = "Error: " + g_errorMsg + " (pos: " + to_string(g_errorPos) + ")";
                float errWidth = ImGui::CalcTextSize(errStr.c_str()).x;
                float targetPosX = ImGui::GetContentRegionMax().x - errWidth - ImGui::GetStyle().WindowPadding.x;
                ImGui::SameLine(targetPosX);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
                ImGui::TextColored({ 1,0,0,1 }, "%s", errStr.c_str());
            }
        }
        ImGui::End();



        ImGui::Begin("Settings");

        // Wybór motywu
        ImGui::Text("UI Appearance");
        if (ImGui::Combo("Theme", &g_themeIdx, g_themeNames, 5)) {
            ApplyTheme(g_themeIdx);
        }

        ImGui::Separator();
        ImGui::Spacing();

        // Sterowanie dźwiękiem
        ImGui::Text("Audio Engine");
        ImGui::SliderFloat("Volume", &g_volume, 0.0f, 1.0f, "%.2f");
        if (ImGui::Combo("Sample Rate", &g_rateIdx, g_rateNames, 7)) {
            // Re-inicjalizacja streamu jeśli zmienisz rate drastycznie, 
            // ale Twoja obecna logika tInc w callbacku radzi sobie z tym dynamicznie.
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Statystyki t
// --- Sekcja Playback Info i Przyciski obok siebie ---
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Playback Info");

        // Tworzymy 2 kolumny: lewa na tekst, prawa na przyciski
        ImGui::Columns(2, "PlaybackLayout", false); // false = brak linii rozdzielającej
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.45f); // 45% szerokości na tekst

        // KOLUMNA 1: Tekst
        ImGui::Text("Current t: %u", g_t);
        ImGui::Text("Time: %.2f s", (float)g_t / g_rates[g_rateIdx]);

        ImGui::NextColumn(); // Przejście do prawej kolumny

        // KOLUMNA 2: Przyciski (zmniejszona szerokość)
        float buttonWidth = ImGui::GetContentRegionAvail().x; // Przyciski zajmą resztę miejsca w kolumnie

        if (ImGui::Button("Copy Formula", ImVec2(buttonWidth, 0))) {
            ImGui::SetClipboardText(g_editor.GetText().c_str());
        }

        if (!g_valid) ImGui::BeginDisabled();
        if (ImGui::Button("Quick Export (30s)", ImVec2(buttonWidth, 0))) {
            ExportToWav();
        }
        if (!g_valid) ImGui::EndDisabled();

        ImGui::Columns(1); // Powrót do jednej kolumny dla reszty okna
        // ---------------------------------------------------

        ImGui::TextDisabled("Saves audio as .wav in project folder");
        ImGui::End();

        ImGui::Begin("Presets");
        ImGui::TextDisabled("Click on a code to load it into the editor:");
        ImGui::Separator();


        if (ImGui::BeginChild("PresetList")) { // Child window zapewnia niezależny scrollbar
            for (const auto& preset : g_presets) {
                ImGui::PushID(preset.title.c_str());

                // Tytuł presetu (pogrubiony/kolorowy)
                ImGui::TextColored({ 0.4f, 0.7f, 1.0f, 1.0f }, "%s", preset.title.c_str());

                // Kod jako przycisk tekstowy, który wygląda jak kod
                // Używamy Selectable, aby łatwo wykryć kliknięcie na całą linię kodu
                if (ImGui::Selectable(preset.code.c_str(), false)) {
                    // 1. Kopiowanie kodu (bezpieczne)
                    strncpy(g_inputBuf, preset.code.c_str(), sizeof(g_inputBuf) - 1);
                    g_inputBuf[sizeof(g_inputBuf) - 1] = '\0';

                    // 2. Re-kompilacja i reset czasu
                    g_valid = g_expr.Compile(g_inputBuf, g_errorMsg, g_errorPos);
                    g_t = 0;
                    g_tAccum = 0.0;

                    // 3. AUTOMATYCZNE USTAWIANIE SAMPLE RATE W SETTINGS
                    bool found = false;
                    for (int i = 0; i < 7; i++) {
                        if (g_rates[i] == preset.sampleRate) {
                            g_rateIdx = i; // Ustawiamy indeks dla Combo w Settings
                            found = true;
                            break;
                        }
                    }

                    // Jeśli z jakiegoś powodu w presecie wpiszesz rate, którego nie ma na liście g_rates
                    // domyślnie ustawiamy na pierwszy element (zazwyczaj 8000 Hz)
                    if (!found) g_rateIdx = 0;

                    g_editor.SetText(preset.code);
                    g_playing = true;
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Click to load this formula");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::End();


        ImGui::Begin("Oscilloscope");
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 sz = ImGui::GetContentRegionAvail();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // 1. Tło i siatka (zostawiasz tak jak miałeś)
        dl->AddRectFilled(p, { p.x + sz.x, p.y + sz.y }, IM_COL32(10, 10, 15, 255));
        dl->AddLine({ p.x, p.y + sz.y / 2 }, { p.x + sz.x, p.y + sz.y / 2 }, IM_COL32(100, 100, 120, 255), 1.0f);

        // 2. STABILIZACJA: Szukamy punktu startu
        uint32_t triggeredT = FindTrigger(g_t);
        float numSamples = 512.0f * g_zoomFactors[g_zoomIdx]; // Skalowanie ilości danych do okna

        // 3. Rysujemy 256 próbek (Twoja nowa rozdzielczość)
        for (int n = 0; n < 255; n++) {
            // Obliczamy wartości bezpośrednio z formuły dla idealnej płynności
            // Dzielimy n przez g_scopeZoom, aby "rozciagnac" probe na ekranie
            float tIdx1 = ((float)n / 256.0f) * numSamples;
            float tIdx2 = ((float)(n + 1) / 256.0f) * numSamples;

            int v1 = g_expr.Eval(triggeredT + (uint32_t)tIdx1);
            int v2 = g_expr.Eval(triggeredT + (uint32_t)tIdx2);

            // Mapowanie 0-255 na wysokość okna (-1.0 do 1.0)
            float sample1 = ((v1 & 0xFF) / 255.0f); // 0.0 to dół, 1.0 to góra
            float sample2 = ((v2 & 0xFF) / 255.0f);

            float x1 = p.x + (float)n / 256.0f * sz.x;
            float y1 = p.y + sz.y - (sample1 * sz.y); // Odwrócenie Y (w ImGui 0 jest na górze)
            float x2 = p.x + (float)(n + 1) / 256.0f * sz.x;
            float y2 = p.y + sz.y - (sample2 * sz.y);

            // Rysowanie linii z kolorem zależnym od pozycji (tęcza)
            dl->AddLine({ x1, y1 }, { x2, y2 }, ImColor::HSV(n / 256.0f, 0.8f, 1.0f), 2.5f);
        }
        ImGui::End();


        // Obsługa komunikatu o sukcesie
        if (g_successMsgTimer > 0) {
            g_successMsgTimer -= GetFrameTime();
            ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("##Success", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "SUCCESS: '%s' saved!", g_fileName.c_str());
            ImGui::End();
        }

        rlImGuiEnd();
        EndDrawing();
    }

    rlImGuiShutdown();

    UnloadAudioStream(stream);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}