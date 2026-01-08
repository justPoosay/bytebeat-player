#include "raylib.h"
#include <cstdint>
#include <vector>
#include <string>
#include <cctype>

using namespace std;

// =============================
// Bytebeat expression parser
// =============================

enum class TokType
{
    Number,
    VarT,
    Op,
    LParen,
    RParen,
    Question, // NOWE
    Colon     // NOWE
};

enum class OpType
{
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
    Less,    // NOWE
    Greater, // NOWE
    Neg,     // NOWE
    Question, // NOWE
    Colon     // NOWE
};

struct Token
{
    TokType type;
    int     value;
    OpType  op;

    Token() : type(TokType::Number), value(0), op(OpType::Add) {}

    static Token number(int v) { Token t; t.type = TokType::Number; t.value = v; return t; }
    static Token varT() { Token t; t.type = TokType::VarT; return t; }
    static Token makeOp(OpType o) { Token t; t.type = TokType::Op; t.op = o; return t; }
    static Token lparen() { Token t; t.type = TokType::LParen; return t; }
    static Token rparen() { Token t; t.type = TokType::RParen; return t; }
    static Token question() { Token t; t.type = TokType::Question; return t; } // NOWE
    static Token colon() { Token t; t.type = TokType::Colon; return t; }       // NOWE
};

static int Precedence(OpType op)
{
    switch (op)
    {
    case OpType::Neg:     return 11;

    case OpType::Mul:
    case OpType::Div:
    case OpType::Mod:     return 10;

    case OpType::Add:
    case OpType::Sub:     return 9;

    case OpType::Shl:
    case OpType::Shr:     return 8;

    case OpType::Less:
    case OpType::Greater: return 7;

    case OpType::And:     return 6;
    case OpType::Xor:     return 5;
    case OpType::Or:      return 4;

    case OpType::Question: return 3;
    case OpType::Colon:    return 2;
    }
    return 0;
}

class BytebeatExpression
{
public:
    bool Compile(const string& expr, string& error)
    {
        error.clear();
        vector<Token> tokens;
        if (!Tokenize(expr, tokens, error)) return false;
        if (!ToRPN(tokens, error)) return false;
        return true;
    }

    int Eval(uint64_t t) const
    {
        if (m_rpn.empty()) return 0;
        vector<int> stack;
        stack.reserve(32);

        for (const Token& tok : m_rpn)
        {
            if (tok.type == TokType::Number) stack.push_back(tok.value);
            else if (tok.type == TokType::VarT) stack.push_back((int)t);
            else if (tok.type == TokType::Op) {
                if (tok.op == OpType::Neg) { // NOWE
                    if (!stack.empty()) stack.back() = -stack.back();
                }
                else if (stack.size() >= 2) {
                    int b = stack.back(); stack.pop_back();
                    int a = stack.back(); stack.pop_back();
                    switch (tok.op) {
                    case OpType::Add: stack.push_back(a + b); break;
                    case OpType::Sub: stack.push_back(a - b); break;
                    case OpType::Mul: stack.push_back(a * b); break;
                    case OpType::Div: stack.push_back(b ? a / b : 0); break;
                    case OpType::Mod: stack.push_back(b ? a % b : 0); break;
                    case OpType::And: stack.push_back(a & b); break;
                    case OpType::Or:  stack.push_back(a | b); break;
                    case OpType::Xor: stack.push_back(a ^ b); break;
                    case OpType::Shl: stack.push_back(a << b); break;
                    case OpType::Shr: stack.push_back(a >> b); break;
                    case OpType::Less:    stack.push_back(a < b); break; // NOWE
                    case OpType::Greater: stack.push_back(a > b); break; // NOWE
                    }
                }
            }
            else if (tok.type == TokType::Question) { // NOWE (Ternary)
                if (stack.size() >= 3) {
                    int f = stack.back(); stack.pop_back();
                    int v = stack.back(); stack.pop_back();
                    int c = stack.back(); stack.pop_back();
                    stack.push_back(c ? v : f);
                }
            }
        }
        return stack.empty() ? 0 : (stack.back() & 0xFF);
    }

private:
    bool Tokenize(const string& expr, vector<Token>& tokens, string& error)
    {
        size_t i = 0;
        while (i < expr.size()) {
            char c = expr[i];
            if (isspace((unsigned char)c)) { i++; continue; }
            if (isdigit((unsigned char)c)) {
                int v = 0;
                while (i < expr.size() && isdigit((unsigned char)expr[i])) v = v * 10 + (expr[i++] - '0');
                tokens.push_back(Token::number(v)); continue;
            }
            if (c == 't' || c == 'T') { tokens.push_back(Token::varT()); i++; continue; }
            if (c == '(') { tokens.push_back(Token::lparen()); i++; continue; }
            if (c == ')') { tokens.push_back(Token::rparen()); i++; continue; }
            if (c == '?') { tokens.push_back(Token::question()); i++; continue; } // NOWE
            if (c == ':') { tokens.push_back(Token::colon()); i++; continue; } // NOWE

            if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '&' || c == '|' || c == '^' || c == '<' || c == '>') {
                // Obsługa <<, >> oraz <, >
                if (c == '<' || c == '>') {
                    if (i + 1 < expr.size() && expr[i + 1] == c) {
                        tokens.push_back(Token::makeOp(c == '<' ? OpType::Shl : OpType::Shr));
                        i += 2; continue;
                    }
                    else {
                        tokens.push_back(Token::makeOp(c == '<' ? OpType::Less : OpType::Greater));
                        i++; continue;
                    }
                }
                // Unarny minus
                if (c == '-') {
                    bool isUn = tokens.empty() || tokens.back().type == TokType::LParen || tokens.back().type == TokType::Op || tokens.back().type == TokType::Question || tokens.back().type == TokType::Colon;
                    tokens.push_back(Token::makeOp(isUn ? OpType::Neg : OpType::Sub));
                    i++; continue;
                }
                OpType op;
                switch (c) {
                case '+': op = OpType::Add; break; case '*': op = OpType::Mul; break; case '/': op = OpType::Div; break;
                case '%': op = OpType::Mod; break; case '&': op = OpType::And; break; case '|': op = OpType::Or;  break;
                case '^': op = OpType::Xor; break;
                }
                tokens.push_back(Token::makeOp(op)); i++; continue;
            }
            i++;
        }
        return !tokens.empty();
    }

    bool ToRPN(const vector<Token>& tokens, string& error)
    {
        m_rpn.clear(); vector<Token> s;
        for (const auto& t : tokens) {
            if (t.type == TokType::Number || t.type == TokType::VarT) m_rpn.push_back(t);
            else if (t.type == TokType::LParen) s.push_back(t);
            else if (t.type == TokType::RParen) {
                while (!s.empty() && s.back().type != TokType::LParen) { m_rpn.push_back(s.back()); s.pop_back(); }
                if (!s.empty()) s.pop_back();
            }
            else if (t.type == TokType::Op) {
                while (!s.empty() && s.back().type == TokType::Op && Precedence(s.back().op) >= Precedence(t.op)) { m_rpn.push_back(s.back()); s.pop_back(); }
                s.push_back(t);
            }
            else if (t.type == TokType::Question) { // NOWE
                while (!s.empty() && s.back().type == TokType::Op) { m_rpn.push_back(s.back()); s.pop_back(); }
                s.push_back(t);
            }
            else if (t.type == TokType::Colon) { // NOWE
                while (!s.empty() && s.back().type != TokType::Question) { m_rpn.push_back(s.back()); s.pop_back(); }
            }
        }
        while (!s.empty()) { m_rpn.push_back(s.back()); s.pop_back(); }
        return true;
    }
private:
    vector<Token> m_rpn;
};

// ... TUTAJ RESZTA TWOJEGO ORYGINALNEGO KODU (main, RecreateAudioStream, pętla rysowania) ...

// =============================
// Bytebeat render (mono), at fixed output rate
// =============================

static const int SAMPLE_RATE_OPTIONS[] = {
    8000, 11025, 16000, 22050, 32000, 44100, 48000
};
static const int SAMPLE_RATE_OPTION_COUNT =
sizeof(SAMPLE_RATE_OPTIONS) / sizeof(SAMPLE_RATE_OPTIONS[0]);

// Audio device / output rate (raylib mixer).
static const int OUTPUT_SAMPLE_RATE = 44100;

// Render up to maxDurationSeconds, but stop earlier if we detect
// a long "silence" tail. Fade out at the end to avoid clicks.
static void RenderBytebeatOneShot(
    const BytebeatExpression& expr,
    int bytebeatRate,
    int outputRate,
    float maxDurationSeconds,
    vector<short>& outSamples)
{
    if (bytebeatRate <= 0) bytebeatRate = 8000;
    if (outputRate <= 0) outputRate = 44100;

    // 1. Obliczamy okres Bytebeatu w świecie "t"
    // Dla t&t>>8 i większości wzorów to 65536 (2^16)
    const uint32_t bytebeatPeriod = 65536;

    // 2. Obliczamy ile to próbek w wyjściowym sample rate (np. 44100)
    // Używamy double dla precyzji obliczeń klatki stopu
    uint32_t framesPerPeriod = (uint32_t)round(((double)bytebeatPeriod / bytebeatRate) * outputRate);

    uint32_t maxFrames = (uint32_t)round(maxDurationSeconds * outputRate);
    outSamples.clear();
    outSamples.reserve(framesPerPeriod + 100);

    // Licznik stałoprzecinkowy, aby uniknąć pływającego pitchu
    // t_accum trzyma czas pomnożony przez outputRate
    uint64_t t_accum = 0;

    for (uint32_t i = 0; i < maxFrames; ++i)
    {
        // t = (i * bytebeatRate) / outputRate
        // To jest najdokładniejsza metoda obliczania t dla każdej klatki audio
        uint32_t t = (uint32_t)((uint64_t)i * bytebeatRate / outputRate);

        int v = expr.Eval(t);
        short sample = (short)(((v & 0xFF) - 128) << 8);
        outSamples.push_back(sample);

        // DETEKCJA PĘTLI
        // Zamiast porównywać dżwięk, sprawdzamy czy t przekroczyło okres
        if (i >= framesPerPeriod)
        {
            // Sprawdzamy, czy t właśnie przeskoczyło przez wielokrotność 65536
            // albo po prostu ucinamy na obliczonym framesPerPeriod
            outSamples.resize(i);
            return; // Mamy idealną pętlę
        }
    }
}

// =============================
// Main
// =============================

int main()
{
    const int screenWidth = 1024;
    const int screenHeight = 600;

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "Bytebeat Composer (Play/Stop + triggered scope)");
    SetTargetFPS(60);

    InitAudioDevice();

    // GUI state
    string exprText = "t*(42&t>>10)"; // default: 42 melody
    bool exprBoxEditMode = false;
    bool exprTextSelectedAll = false;
    Rectangle exprBox = { 20, 20, (float)screenWidth - 40.0f, 40 };

    int sampleRateIndex = 0; // 0 -> 8000 Hz (conceptual bytebeat rate)
    Rectangle srLeftButton = { 20, 80, 30, 30 };
    Rectangle srRightButton = { 260, 80, 30, 30 };

    Rectangle playStopButton = { (float)screenWidth - 160.0f, 60.0f, 120.0f, 30.0f };

    // Expression + error state
    BytebeatExpression expr;
    bool   exprValid = false;
    string errorText;

    // Audio data
    vector<short> renderedSamples;     // mono for analysis + scope
    vector<short> audioSamplesStereo;  // interleaved L/R for playback

    Sound currentSound = { 0 };
    bool  hasSound = false;

    const float MAX_RENDER_DURATION_SECONDS = 30.0f; // safety cap

    // Playback state
    double playStartTime = 0.0; // for mapping time -> sample index
    bool   userStopped = false;

    // Cache info to avoid unnecessary re-render
    string lastExprText;
    int    lastSampleRateIndex = -1;

    // Backspace repeat handling
    float backspaceHoldTime = 0.0f;
    float backspaceRepeatAccum = 0.0f;
    const float BACKSPACE_INITIAL_DELAY = 0.4f;
    const float BACKSPACE_REPEAT_INTERVAL = 0.03f;

    while (!WindowShouldClose())
    {
        float  dt = GetFrameTime();
        Vector2 mouse = GetMousePosition();

        // Check internal playing state from raylib
        bool soundPlayingInternal = hasSound && IsSoundPlaying(currentSound);
        bool isPlayingNow = hasSound && soundPlayingInternal && !userStopped;

        // Mouse clicks
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            // Focus text box
            if (CheckCollisionPointRec(mouse, exprBox))
                exprBoxEditMode = true;
            else
                exprBoxEditMode = false;

            if (!exprBoxEditMode)
                exprTextSelectedAll = false;

            // Sample rate change (conceptual bytebeat rate)
            if (CheckCollisionPointRec(mouse, srLeftButton))
            {
                sampleRateIndex--;
                if (sampleRateIndex < 0) sampleRateIndex = 0;
            }
            else if (CheckCollisionPointRec(mouse, srRightButton))
            {
                sampleRateIndex++;
                if (sampleRateIndex >= SAMPLE_RATE_OPTION_COUNT)
                    sampleRateIndex = SAMPLE_RATE_OPTION_COUNT - 1;
            }

            // Play/Stop (RESET)
            if (CheckCollisionPointRec(mouse, playStopButton))
            {
                if (!hasSound || !isPlayingNow)
                {
                    // --- PLAY ---

                    // Decide if we need to re-render, or can reuse buffer
                    bool shouldRender =
                        !hasSound ||
                        exprText != lastExprText ||
                        sampleRateIndex != lastSampleRateIndex;

                    if (shouldRender)
                    {
                        if (hasSound)
                        {
                            StopSound(currentSound);
                            UnloadSound(currentSound);
                            hasSound = false;
                        }

                        if (!expr.Compile(exprText, errorText))
                        {
                            exprValid = false;
                            renderedSamples.clear();
                            audioSamplesStereo.clear();
                        }
                        else
                        {
                            exprValid = true;
                            errorText.clear();

                            int bytebeatRate = SAMPLE_RATE_OPTIONS[sampleRateIndex];

                            RenderBytebeatOneShot(
                                expr,
                                bytebeatRate,
                                OUTPUT_SAMPLE_RATE,
                                MAX_RENDER_DURATION_SECONDS,
                                renderedSamples
                            );

                            if (renderedSamples.empty())
                            {
                                errorText = "Rendered buffer is empty (expression is silent?)";
                                exprValid = false;
                                hasSound = false;
                                audioSamplesStereo.clear();
                            }
                            else
                            {
                                size_t frames = renderedSamples.size();
                                audioSamplesStereo.resize(frames * 2);

                                for (size_t i = 0; i < frames; ++i)
                                {
                                    short s = renderedSamples[i];
                                    audioSamplesStereo[2 * i + 0] = s;
                                    audioSamplesStereo[2 * i + 1] = s;
                                }

                                Wave wave = { 0 };
                                wave.data = audioSamplesStereo.data();
                                wave.frameCount = (unsigned int)frames;
                                wave.sampleRate = (unsigned int)OUTPUT_SAMPLE_RATE;
                                wave.sampleSize = 16;
                                wave.channels = 2;

                                currentSound = LoadSoundFromWave(wave);
                                if (currentSound.frameCount == 0)
                                {
                                    errorText = "Failed to create sound from wave";
                                    exprValid = false;
                                    hasSound = false;
                                }
                                else
                                {
                                    hasSound = true;
                                    lastExprText = exprText;
                                    lastSampleRateIndex = sampleRateIndex;
                                }
                            }
                        }
                    }

                    if (hasSound)
                    {
                        userStopped = false;
                        StopSound(currentSound);
                        PlaySound(currentSound);
                        playStartTime = GetTime();
                    }
                }
                else
                {
                    // --- RESET ---
                    userStopped = true;
                    StopSound(currentSound);
                    playStartTime = GetTime();
                }
            }
        }

        // ==== Text input + shortcuts for expr box ====
        if (exprBoxEditMode)
        {
            bool ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

            if (ctrlDown && IsKeyPressed(KEY_A))
            {
                exprTextSelectedAll = true;
            }

            if (ctrlDown && IsKeyPressed(KEY_V))
            {
                const char* clip = GetClipboardText();
                if (clip)
                {
                    if (exprTextSelectedAll)
                    {
                        exprText.clear();
                        exprTextSelectedAll = false;
                    }
                    exprText += clip;
                }
            }

            if (!ctrlDown)
            {
                int key = GetCharPressed();
                while (key > 0)
                {
                    if (key >= 32 && key <= 126)
                    {
                        if (exprTextSelectedAll)
                        {
                            exprText.clear();
                            exprTextSelectedAll = false;
                        }
                        exprText.push_back((char)key);
                    }
                    key = GetCharPressed();
                }
            }

            bool backspacePressed = IsKeyPressed(KEY_BACKSPACE);
            bool backspaceDown = IsKeyDown(KEY_BACKSPACE);

            if (backspacePressed)
            {
                if (exprTextSelectedAll)
                {
                    exprText.clear();
                    exprTextSelectedAll = false;
                }
                else if (!exprText.empty())
                {
                    exprText.pop_back();
                }
                backspaceHoldTime = 0.0f;
                backspaceRepeatAccum = 0.0f;
            }
            else if (backspaceDown)
            {
                backspaceHoldTime += dt;
                if (backspaceHoldTime > BACKSPACE_INITIAL_DELAY)
                {
                    backspaceRepeatAccum += dt;
                    while (backspaceRepeatAccum >= BACKSPACE_REPEAT_INTERVAL)
                    {
                        backspaceRepeatAccum -= BACKSPACE_REPEAT_INTERVAL;

                        if (exprTextSelectedAll)
                        {
                            exprText.clear();
                            exprTextSelectedAll = false;
                            break;
                        }
                        else if (!exprText.empty())
                        {
                            exprText.pop_back();
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
            else
            {
                backspaceHoldTime = 0.0f;
                backspaceRepeatAccum = 0.0f;
            }
        }
        else
        {
            exprTextSelectedAll = false;
            backspaceHoldTime = 0.0f;
            backspaceRepeatAccum = 0.0f;
        }

        // Loop final render unless user explicitly stopped
        soundPlayingInternal = hasSound && IsSoundPlaying(currentSound);
        isPlayingNow = hasSound && soundPlayingInternal && !userStopped;

        if (hasSound && !soundPlayingInternal && !userStopped)
        {
            PlaySound(currentSound);
            playStartTime = GetTime();
            isPlayingNow = true;
        }

        // ==== Drawing ====
        BeginDrawing();

        Color bg = { 10, 10, 40, 255 };
        ClearBackground(bg);

        DrawText("Bytebeat expression:", 20, 0, 18, RAYWHITE);

        // Text box
        Color boxFill = exprBoxEditMode ? DARKBLUE : DARKGRAY;
        if (exprTextSelectedAll && exprBoxEditMode && !exprText.empty())
        {
            Color selectedFill = { 0, 40, 80, 255 };
            boxFill = selectedFill;
        }

        DrawRectangleRec(exprBox, boxFill);
        DrawRectangleLines((int)exprBox.x, (int)exprBox.y,
            (int)exprBox.width, (int)exprBox.height,
            exprBoxEditMode ? SKYBLUE : LIGHTGRAY);

        string exprToDraw = exprText;
        int maxChars = (int)(exprBox.width / 8) - 1;
        if ((int)exprToDraw.size() > maxChars)
            exprToDraw = exprToDraw.substr(exprToDraw.size() - maxChars);

        DrawText(exprToDraw.c_str(),
            (int)exprBox.x + 8,
            (int)exprBox.y + 10,
            20, RAYWHITE);

        // Sample rate selector
        DrawText("Sample rate:", 20, 60, 18, RAYWHITE);
        DrawRectangleRec(srLeftButton, DARKGRAY);
        DrawRectangleLines((int)srLeftButton.x, (int)srLeftButton.y,
            (int)srLeftButton.width, (int)srLeftButton.height,
            LIGHTGRAY);
        DrawText("<", (int)srLeftButton.x + 8, (int)srLeftButton.y + 5, 20, RAYWHITE);

        DrawRectangleRec(srRightButton, DARKGRAY);
        DrawRectangleLines((int)srRightButton.x, (int)srRightButton.y,
            (int)srRightButton.width, (int)srRightButton.height,
            LIGHTGRAY);
        DrawText(">", (int)srRightButton.x + 8, (int)srRightButton.y + 5, 20, RAYWHITE);

        int bytebeatRate = SAMPLE_RATE_OPTIONS[sampleRateIndex];
        string srText = to_string(bytebeatRate) + " Hz";
        DrawText(srText.c_str(), 70, 86, 20, YELLOW);

        // Play/Reset button
        const char* playLabel = isPlayingNow ? "RESET" : "PLAY";
        Color playColor = isPlayingNow ? Color{ 200, 40, 40, 255 }
        : Color{ 40, 140, 40, 255 };

        DrawRectangleRec(playStopButton, playColor);
        DrawRectangleLines((int)playStopButton.x, (int)playStopButton.y,
            (int)playStopButton.width, (int)playStopButton.height,
            RAYWHITE);
        int labelX = (int)playStopButton.x + 32;
        DrawText(playLabel,
            labelX,
            (int)playStopButton.y + 8, 18, RAYWHITE);

        if (!errorText.empty())
            DrawText(errorText.c_str(), 20, 130, 18, RED);

        // Scope
        int waveLeft = 20;
        int waveTop = 180;
        int waveWidth = screenWidth - 40;
        int waveHeight = screenHeight - waveTop - 20;

        DrawRectangleLines(waveLeft, waveTop, waveWidth, waveHeight, GRAY);

        if (exprValid && !renderedSamples.empty())
        {
            int totalFrames = (int)renderedSamples.size();
            if (totalFrames > 1)
            {
                int cur = 0;
                if (hasSound && isPlayingNow)
                {
                    double elapsed = GetTime() - playStartTime;
                    if (elapsed < 0.0) elapsed = 0.0;

                    double posFrames = elapsed * (double)OUTPUT_SAMPLE_RATE;
                    posFrames = fmod(posFrames, (double)totalFrames);
                    if (posFrames < 0.0) posFrames = 0.0;

                    cur = (int)posFrames;
                }

                if (cur < 0) cur = 0;
                if (cur >= totalFrames) cur = totalFrames - 1;

                int triggerIdx = cur;
                {
                    float maxSearch = OUTPUT_SAMPLE_RATE;
                    if (maxSearch > cur) maxSearch = cur;

                    for (int k = 1; k < maxSearch; ++k)
                    {
                        int idx = cur - k;
                        if (idx <= 0) break;
                        short s0 = renderedSamples[idx - 1];
                        short s1 = renderedSamples[idx];
                        if ((s0 <= 0 && s1 > 0) || (s0 >= 0 && s1 < 0))
                        {
                            triggerIdx = idx;
                            break;
                        }
                    }
                }

                int periodSamples = OUTPUT_SAMPLE_RATE / 440; // fallback

                {
                    int maxSearch = OUTPUT_SAMPLE_RATE / 5;
                    if (maxSearch > triggerIdx) maxSearch = triggerIdx;

                    int crossesFound = 0;
                    int lastCross = -1;
                    int prevCross = -1;

                    for (int k = 1; k < maxSearch; ++k)
                    {
                        int idx = triggerIdx - k;
                        if (idx <= 0) break;

                        short s0 = renderedSamples[idx - 1];
                        short s1 = renderedSamples[idx];
                        if ((s0 <= 0 && s1 > 0) || (s0 >= 0 && s1 < 0))
                        {
                            ++crossesFound;
                            if (crossesFound == 1)
                                lastCross = idx;
                            else if (crossesFound == 2)
                            {
                                prevCross = idx;
                                break;
                            }
                        }
                    }

                    if (prevCross != -1 && lastCross != -1)
                    {
                        int diff = lastCross - prevCross;
                        if (diff > 0) periodSamples = diff;
                    }
                }

                int windowFrames = periodSamples * 8;
                if (windowFrames < waveWidth)  windowFrames = waveWidth;
                if (windowFrames > totalFrames) windowFrames = totalFrames;

                int   prevX = waveLeft;
                float prevY = waveTop + waveHeight / 2.0f;
                float amplitudeScale = 1.0f;

                for (int x = 0; x < waveWidth; ++x)
                {
                    int offset = (x * windowFrames) / waveWidth;
                    int idx = triggerIdx + offset;
                    if (idx >= totalFrames) idx -= totalFrames;

                    float s = (float)renderedSamples[idx] / 32768.0f;
                    if (s > 1.0f) s = 1.0f;
                    if (s < -1.0f) s = -1.0f;
                    s *= amplitudeScale;

                    float normY = (s + 1.0f) * 0.5f;
                    float y = waveTop + waveHeight * (1.0f - normY);

                    int screenX = waveLeft + x;
                    if (x > 0)
                    {
                        Vector2 p0 = { (float)prevX, prevY };
                        Vector2 p1 = { (float)screenX, y };
                        DrawLineEx(p0, p1, 2.5f, LIME);
                    }

                    prevX = screenX;
                    prevY = y;
                }
            }
        }

        EndDrawing();
    }

    if (hasSound)
    {
        StopSound(currentSound);
        UnloadSound(currentSound);
    }

    CloseAudioDevice();
    CloseWindow();

    return 0;
}
