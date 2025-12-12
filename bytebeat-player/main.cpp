#include "raylib.h"

#include <cstdint>
#include <vector>
#include <string>
#include <cctype>
#include <cmath>

using namespace std;

// =============================
// Bytebeat expression parser
// =============================

enum class TokType
{
    Number,
    VarT,
    Op,
    Func,
    LParen,
    RParen
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
    Shr
};

enum class FuncType
{
    Sin,
    Cos,
    Tan,
    Sqrt,
    Abs,
    Log,
    Exp
};

struct Token
{
    TokType  type;
    int      value; // for Number
    OpType   op;    // for Op
    FuncType func;  // for Func

    Token()
        : type(TokType::Number),
        value(0),
        op(OpType::Add),
        func(FuncType::Sin)
    {
    }

    static Token number(int v)
    {
        Token t;
        t.type = TokType::Number;
        t.value = v;
        return t;
    }

    static Token varT()
    {
        Token t;
        t.type = TokType::VarT;
        return t;
    }

    static Token makeOp(OpType o)
    {
        Token t;
        t.type = TokType::Op;
        t.op = o;
        return t;
    }

    static Token makeFunc(FuncType f)
    {
        Token t;
        t.type = TokType::Func;
        t.func = f;
        return t;
    }

    static Token lparen()
    {
        Token t;
        t.type = TokType::LParen;
        return t;
    }

    static Token rparen()
    {
        Token t;
        t.type = TokType::RParen;
        return t;
    }
};

static int Precedence(OpType op)
{
    switch (op)
    {
    case OpType::Or:  return 1;
    case OpType::Xor: return 2;
    case OpType::And: return 3;
    case OpType::Shl:
    case OpType::Shr: return 4;
    case OpType::Add:
    case OpType::Sub: return 5;
    case OpType::Mul:
    case OpType::Div:
    case OpType::Mod: return 6;
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
        if (!Tokenize(expr, tokens, error))
            return false;
        if (!ToRPN(tokens, error))
            return false;
        return true;
    }

    // Evaluate expression for integer time t, return 0..255
    int Eval(uint32_t t) const
    {
        if (m_rpn.empty())
            return 0;

        vector<int> stack;
        stack.reserve(32);

        for (const Token& tok : m_rpn)
        {
            switch (tok.type)
            {
            case TokType::Number:
                stack.push_back(tok.value);
                break;

            case TokType::VarT:
                stack.push_back(static_cast<int>(t));
                break;

            case TokType::Op:
            {
                if (stack.size() < 2)
                    return 0;
                int b = stack.back(); stack.pop_back();
                int a = stack.back(); stack.pop_back();
                int r = 0;

                switch (tok.op)
                {
                case OpType::Add: r = a + b; break;
                case OpType::Sub: r = a - b; break;
                case OpType::Mul: r = a * b; break;
                case OpType::Div: r = (b != 0) ? a / b : 0; break;
                case OpType::Mod: r = (b != 0) ? a % b : 0; break;
                case OpType::And: r = a & b; break;
                case OpType::Or:  r = a | b; break;
                case OpType::Xor: r = a ^ b; break;
                case OpType::Shl: r = a << b; break;
                case OpType::Shr: r = a >> b; break;
                }

                stack.push_back(r);
                break;
            }

            case TokType::Func:
            {
                if (stack.empty())
                    return 0;
                int a = stack.back();
                stack.pop_back();

                double x = static_cast<double>(a);
                double r = 0.0;

                switch (tok.func)
                {
                case FuncType::Sin:  r = sin(x);           break;
                case FuncType::Cos:  r = cos(x);           break;
                case FuncType::Tan:  r = tan(x);           break;
                case FuncType::Sqrt: r = (x >= 0.0) ? sqrt(x) : 0.0; break;
                case FuncType::Abs:  r = fabs(x);          break;
                case FuncType::Log:  r = (x > 0.0) ? log(x) : 0.0;    break;
                case FuncType::Exp:  r = exp(x);           break;
                }

                int ri = static_cast<int>(lround(r));
                stack.push_back(ri);
                break;
            }

            default:
                break;
            }
        }

        if (stack.empty())
            return 0;

        return stack.back() & 0xFF;
    }

private:
    bool Tokenize(const string& expr,
        vector<Token>& tokens,
        string& error)
    {
        size_t i = 0;
        while (i < expr.size())
        {
            char c = expr[i];

            if (isspace((unsigned char)c))
            {
                ++i;
                continue;
            }

            if (isdigit((unsigned char)c))
            {
                int value = 0;
                while (i < expr.size() && isdigit((unsigned char)expr[i]))
                {
                    value = value * 10 + (expr[i] - '0');
                    ++i;
                }
                tokens.push_back(Token::number(value));
                continue;
            }

            // identifiers: t, sin, cos, tan, sqrt, abs, log, exp
            if (isalpha((unsigned char)c))
            {
                string ident;
                while (i < expr.size() && isalpha((unsigned char)expr[i]))
                {
                    ident.push_back(expr[i]);
                    ++i;
                }

                if (ident == "t" || ident == "T")
                {
                    tokens.push_back(Token::varT());
                }
                else if (ident == "sin")
                {
                    tokens.push_back(Token::makeFunc(FuncType::Sin));
                }
                else if (ident == "cos")
                {
                    tokens.push_back(Token::makeFunc(FuncType::Cos));
                }
                else if (ident == "tan")
                {
                    tokens.push_back(Token::makeFunc(FuncType::Tan));
                }
                else if (ident == "sqrt")
                {
                    tokens.push_back(Token::makeFunc(FuncType::Sqrt));
                }
                else if (ident == "abs")
                {
                    tokens.push_back(Token::makeFunc(FuncType::Abs));
                }
                else if (ident == "log")
                {
                    tokens.push_back(Token::makeFunc(FuncType::Log));
                }
                else if (ident == "exp")
                {
                    tokens.push_back(Token::makeFunc(FuncType::Exp));
                }
                else
                {
                    error = "Unknown identifier: " + ident;
                    return false;
                }
                continue;
            }

            if (c == '(')
            {
                tokens.push_back(Token::lparen());
                ++i;
                continue;
            }
            if (c == ')')
            {
                tokens.push_back(Token::rparen());
                ++i;
                continue;
            }

            // Operators (including << and >>)
            if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
                c == '&' || c == '|' || c == '^' || c == '<' || c == '>')
            {
                if (c == '<' || c == '>')
                {
                    // << or >>
                    if (i + 1 < expr.size() && expr[i + 1] == c)
                    {
                        OpType op = (c == '<') ? OpType::Shl : OpType::Shr;
                        tokens.push_back(Token::makeOp(op));
                        i += 2;
                        continue;
                    }
                    else
                    {
                        error = "Expected << or >>";
                        return false;
                    }
                }

                OpType op;
                switch (c)
                {
                case '+': op = OpType::Add; break;
                case '-': op = OpType::Sub; break;
                case '*': op = OpType::Mul; break;
                case '/': op = OpType::Div; break;
                case '%': op = OpType::Mod; break;
                case '&': op = OpType::And; break;
                case '|': op = OpType::Or;  break;
                case '^': op = OpType::Xor; break;
                default:
                    error = "Unknown operator";
                    return false;
                }
                tokens.push_back(Token::makeOp(op));
                ++i;
                continue;
            }

            error = string("Unexpected char: '") + c + "'";
            return false;
        }

        if (tokens.empty())
        {
            error = "Empty expression";
            return false;
        }

        return true;
    }

    bool ToRPN(const vector<Token>& tokens,
        string& error)
    {
        m_rpn.clear();
        vector<Token> opStack;

        for (const Token& tok : tokens)
        {
            switch (tok.type)
            {
            case TokType::Number:
            case TokType::VarT:
                m_rpn.push_back(tok);
                break;

            case TokType::Func:
                // functions go on operator stack, popped after ')'
                opStack.push_back(tok);
                break;

            case TokType::Op:
            {
                while (!opStack.empty() &&
                    opStack.back().type == TokType::Op &&
                    Precedence(opStack.back().op) >= Precedence(tok.op))
                {
                    m_rpn.push_back(opStack.back());
                    opStack.pop_back();
                }
                opStack.push_back(tok);
                break;
            }

            case TokType::LParen:
                opStack.push_back(tok);
                break;

            case TokType::RParen:
            {
                bool foundL = false;
                while (!opStack.empty())
                {
                    if (opStack.back().type == TokType::LParen)
                    {
                        opStack.pop_back();
                        foundL = true;
                        break;
                    }
                    m_rpn.push_back(opStack.back());
                    opStack.pop_back();
                }
                if (!foundL)
                {
                    error = "Mismatched parentheses";
                    return false;
                }

                // If function is on top, pop it to output
                if (!opStack.empty() && opStack.back().type == TokType::Func)
                {
                    m_rpn.push_back(opStack.back());
                    opStack.pop_back();
                }
                break;
            }
            }
        }

        while (!opStack.empty())
        {
            if (opStack.back().type == TokType::LParen ||
                opStack.back().type == TokType::RParen)
            {
                error = "Mismatched parentheses";
                return false;
            }
            m_rpn.push_back(opStack.back());
            opStack.pop_back();
        }

        if (m_rpn.empty())
        {
            error = "Expression reduced to nothing";
            return false;
        }

        return true;
    }

private:
    vector<Token> m_rpn;
};

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
    if (maxDurationSeconds <= 0.0f) maxDurationSeconds = 5.0f;

    uint32_t maxFrames =
        (uint32_t)round(maxDurationSeconds * (float)outputRate);

    outSamples.clear();
    outSamples.reserve(maxFrames);

    const int      silenceThreshold = 300;                // amplitude threshold
    const uint32_t minFramesToRender = outputRate / 10;    // at least 0.1 s
    const uint32_t silenceRunFrames = outputRate / 5;     // 0.2 s of silence to stop

    uint32_t lastLoudIndex = 0;

    float tFloat = 0.0f;
    float tInc = (float)bytebeatRate / (float)outputRate;

    for (uint32_t i = 0; i < maxFrames; ++i)
    {
        uint32_t t = (uint32_t)tFloat;
        tFloat += tInc;

        int v = expr.Eval(t);           // 0..255

        // Map 0..255 -> [-1, 1]
        float s = (float)((v & 0xFF) / 127.5 - 1.0);
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;

        short sample = (short)lround(s * 32767.0f);
        outSamples.push_back(sample);

        if (abs((int)sample) > silenceThreshold)
            lastLoudIndex = i;

        if (i > minFramesToRender && (i - lastLoudIndex) > silenceRunFrames)
            break;
    }

    if (outSamples.empty())
        return;

    // Trim to last loud frame + a bit
    uint32_t used = (uint32_t)outSamples.size();
    uint32_t cutoff = lastLoudIndex + (outputRate / 50); // ~20 ms extra
    if (cutoff < used)
        used = cutoff;

    if (used == 0)
        used = (uint32_t)outSamples.size(); // fallback

    outSamples.resize(used);

    // Fade-out the last 256 frames to avoid click
    uint32_t fadeLen = 256;
    if (fadeLen > used) fadeLen = used;

    for (uint32_t i = 0; i < fadeLen; ++i)
    {
        uint32_t idx = used - 1 - i;
        float    g = 1.0f - (float)i / (float)fadeLen; // 1 -> 0
        outSamples[idx] = (short)lround((float)outSamples[idx] * g);
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
