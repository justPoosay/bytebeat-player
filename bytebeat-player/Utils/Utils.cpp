#define _CRT_SECURE_NO_WARNINGS
#include "Utils.h"
#include "GlobalState.h"
#include "imgui.h"
#include "raylib.h"
#include "TextEditor.h"

#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>

using namespace std;
namespace fs = filesystem;

void ApplyTheme(int themeIdx) {
    ImGuiStyle& style = ImGui::GetStyle();
    switch (themeIdx) {
    case 0:
        ImGui::StyleColorsDark();
        state.editor.SetPalette(TextEditor::GetDarkPalette());
        break;
    case 1: ImGui::StyleColorsLight();
        state.editor.SetPalette(TextEditor::GetLightPalette());
        break;
    case 2: ImGui::StyleColorsClassic();
        break;
    case 3:
        ImGui::StyleColorsDark();
        state.editor.SetPalette(TextEditor::GetRetroBluePalette());
        style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 1.00f, 0.00f, 1.00f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.40f, 0.10f, 1.00f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.60f, 0.10f, 1.00f);
        break;
    case 4:
        ImGui::StyleColorsDark();
        state.editor.SetPalette(TextEditor::GetDarkPalette());
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.05f, 0.10f, 1.00f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.30f, 0.60f, 1.00f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.15f, 0.40f, 0.70f, 1.00f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.15f, 0.30f, 1.00f);
        break;
    }
    style.WindowRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
}

void UpdateErrorMarkers() {
    TextEditor::ErrorMarkers markers;
    if (!state.valid && state.errorPos >= 0 && !state.errorMsg.empty()) {
        // Calculate line number based on errorPos
        int line = 1;
        string code = state.editor.GetText();
        for (int i = 0; i < state.errorPos && i < (int)code.size(); i++) {
            if (code[i] == '\n') line++;
        }
        markers[line] = state.errorMsg;
    }
    state.editor.SetErrorMarkers(markers);
}

void ExportToWav() {
    if (!state.valid) return;

    time_t now = time(0);
    tm* ltm = localtime(&now);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%d-%m-%Y_%H-%M", ltm);

    string fileName = "bytebeat_" + string(timeStr) + ".wav";
    state.fileName = fileName;

    const int exportRate = 44100;
    int targetRate = state.rates[state.rateIdx];
    uint32_t seconds = 30;
    uint32_t totalSamples = seconds * exportRate;

    FILE* f = fopen(fileName.c_str(), "wb");
    if (!f) return;

    // HEADER WAV
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

    // DATA
    double tAccumLocal = 0.0;
    uint32_t tLocal = 0;
    double tInc = (double)targetRate / (double)exportRate;

    for (uint32_t i = 0; i < totalSamples; i++) {
        int v = state.expr.Eval(tLocal);
        fputc((unsigned char)(v & 0xFF), f);

        tAccumLocal += tInc;
        if (tAccumLocal >= 1.0) {
            uint32_t steps = (uint32_t)tAccumLocal;
            tLocal += steps;
            tAccumLocal -= (double)steps;
        }

        if (i % 5000 == 0) state.exportProgress = (float)i / totalSamples;
    }

    fclose(f);
    state.exportProgress = -1.0f;
    state.successMsgTimer = 3.0f;
}

void LoadCodeToEditor(string fullCode) {
    string viewCode = CompressCode(fullCode);
    state.editor.SetText(viewCode);
    strncpy(state.inputBuf, viewCode.c_str(), sizeof(state.inputBuf) - 1);

    state.errorMsg.clear();
    state.valid = 
        (state.currentMode == AppState::BytebeatMode::C_Compatible)
        ? state.expr.Compile(fullCode, state.errorMsg, state.errorPos)
        : state.complexEngine.Compile(fullCode, state.errorMsg, state.errorPos);

    UpdateErrorMarkers();
    state.t = 0;
    state.tAccum = 0.0;
    state.rateIdx = 4;
    state.playing = true;
}

// Helper function to remove white characters (CR, LF, Spaces)
static string TrimRight(string s) {
    s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !isspace(ch);
        }).base(), s.end()
    );
    return s;
}

// Helper function to remove BOM (Byte Order Mark)
static string RemoveBOM(string s) {
    if (s.size() >= 3 && 
        (unsigned char)s[0] == 0xEF && 
        (unsigned char)s[1] == 0xBB && 
        (unsigned char)s[2] == 0xBF) {
        return s.substr(3);
    }
    return s;
}

void LoadPresets(const string& folderPath) {
    g_presets.clear();

    if (!fs::exists(folderPath)) {
        TraceLog(LOG_ERROR, "PRESETS: Folder '%s' not found!", folderPath.c_str());
        return;
    }
    TraceLog(LOG_INFO, "PRESETS: Scanning folder: %s", folderPath.c_str());

    for (const auto& entry : fs::directory_iterator(folderPath)) {
        string ext = entry.path().extension().string();
        transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".txt") {
            ifstream file(entry.path());
            if (!file.is_open()) continue;

            BytebeatPreset preset;
            string line;
            string accumulatedCode;
            bool readingCode = false;
            bool firstLine = true;

            while (getline(file, line)) {
                // Remove trash from endline
                line = TrimRight(line);

                // BOM fix for first line
                if (firstLine) {
                    line = RemoveBOM(line);
                    firstLine = false;
                }

                if (readingCode) {
                    accumulatedCode += line + "\n";
                    continue;
                }
                if (line.find("Code=") == 0) {
                    readingCode = true;
                    if (line.length() > 5) accumulatedCode += line.substr(5) + "\n";
                }
                else if (line.find("Title=") == 0) preset.title = line.substr(6);
                else if (line.find("Rate=") == 0) {
                    try {
                        preset.sampleRate = stoi(line.substr(5));
                    }
                    catch (...) { 
                        preset.sampleRate = 8000; 
                    }
                }
                else if (line.find("Mode=") == 0) {
                    string m = line.substr(5);
                    if (m == "Complex") preset.mode = PresetMode::Complex;
                    else preset.mode = PresetMode::Classic;
                }
            }

            preset.code = accumulatedCode;
            if (!preset.code.empty() && preset.code.back() == '\n') preset.code.pop_back();

            if (!preset.title.empty() && !preset.code.empty()) {
                g_presets.push_back(preset);
                TraceLog(LOG_INFO, "PRESETS: Loaded '%s'", preset.title.c_str());
            }
            else TraceLog(LOG_WARNING, "PRESETS: Skipped file '%s' (Missing Title or Code)", entry.path().filename().string().c_str());
        }
    }
}

uint32_t FindTrigger(uint32_t currentT) {
    const int searchRange = 1024;
    for (uint32_t i = 0; i < searchRange; i++) {
        int valNow = state.expr.Eval(currentT + i) & 0xFF;
        int valNext = state.expr.Eval(currentT + i + 1) & 0xFF;

        if (valNow <= 2 && valNext > 2) return currentT + i;
    }
    return currentT;
}

string FormatCode(const string& code, int maxChars) {
    maxChars = max(maxChars, 20);

    stringstream ss(code);
    string originalLine;
    stringstream out;

    auto isBreakChar = [](char c) {
        return
            c == ',' ||
            c == ';' ||
            c == '{' ||
            c == '}' ||
            c == '+' ||
            c == '-' ||
            c == '*' ||
            c == '/' ||
            c == '&' ||
            c == '|' ||
            c == '^' ||
            c == '?' ||
            c == ':';
        };
    bool firstLine = true;

    while (getline(ss, originalLine)) {
        if (!firstLine) out << "\n";
        firstLine = false;

        while ((int)originalLine.length() > maxChars) {
            int splitIdx = -1;

            int searchStart = (int)(maxChars * 0.6);
            for (int k = maxChars - 1; k >= searchStart; k--) {
                if (isBreakChar(originalLine[k])) {
                    splitIdx = k;
                    break;
                }
            }

            auto n = 
                splitIdx != -1
                ? splitIdx + 1
                : maxChars;

            out << originalLine.substr(0, n) << "\n";
            originalLine = originalLine.substr(n);
        }
        out << originalLine;
    }
    return out.str();
}

string ConvertWavToBytebeat(const char* filePath) {
    Wave wave = LoadWave(filePath);
    if (wave.data == nullptr) TraceLog(LOG_ERROR, "Failed to load .wav file");

    // Format audio (32000Hz/8-bit/mono)
    WaveFormat(&wave, 32000, 8, 1);

    // Generate HEX
    stringstream ss;
    ss << "data='";

    unsigned char* waveData = (unsigned char*)wave.data;
    char hexBuf[5];
    for (unsigned int i = 0; i < wave.frameCount; i++) {
        // Format \xNN
        snprintf(hexBuf, sizeof(hexBuf), "\\x%02X", waveData[i]);
        ss << hexBuf;
    }
    ss << "',\n(data.charCodeAt(t%data.length))";

    UnloadWave(wave);
    return ss.str();
}

string CompressCode(const string& code) {
    state.hiddenChunks.clear();
    state.hiddenCounter = 0;

    stringstream out;
    size_t len = code.length();

    for (size_t i = 0; i < len; ++i) {
        char c = code[i];

        if (c == '\'' || c == '"') {
            char quote = c;
            size_t start = i;
            i++;

            while (i < len) {
                if (code[i] == quote &&
                    (i == 0 || code[i - 1] != '\\' ||
                        (i >= 2 && code[i - 2] == '\\'))) 
                    break;
                i++;
            }


            size_t strLen = i - start + 1;
            string content = code.substr(start, strLen);

            if (strLen > 1000) {
                // Generate unique key
                string key = "@HIDDEN_DATA_" + to_string(state.hiddenCounter++) + "@";
                state.hiddenChunks[key] = content;
                out << key;
            }
            else out << content;

        }
        else out << c;
    }
    return out.str();
}

string ExpandCode(const string& code) {
    string result = code;

    for (auto const& pair : state.hiddenChunks) {
        string key = pair.first;
        string value = pair.second;

        size_t pos = 0;
        while ((pos = result.find(key, pos)) != string::npos) {
            result.replace(pos, key.length(), value);
            pos += value.length();
        }
    }
    return result;
}