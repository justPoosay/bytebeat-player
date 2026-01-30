#pragma once
#include "Bytebeat.h"
#include "TextEditor.h"
#include <string>
#include <vector>
#include <map>

enum class PresetMode {
    Classic,
    Complex
};

struct BytebeatPreset {
    std::string title;
    std::string code;
    int sampleRate;
    PresetMode mode;

    BytebeatPreset() : title(""), code(""), sampleRate(8000), mode(PresetMode::Classic) {}
    //BytebeatPreset(std::string t, std::string c, int sr = 8000, PresetMode m = PresetMode::Classic)
    //    : title(t), code(c), sampleRate(sr), mode(m) {
    //}
};

extern std::vector<BytebeatPreset> g_presets;

struct AppState {
    // Logic
    BytebeatExpression expr;
    ComplexEngine complexEngine;
    enum class BytebeatMode { C_Compatible, JS_Compatible };
    BytebeatMode currentMode = BytebeatMode::C_Compatible;

    // Quick access
    std::vector<double> vmMemory;
    std::map<std::string, int> varTable;

    // Helper: Reset VM state before compilation
    void resetVM() {
        vmMemory.clear();
        varTable.clear();
    }

    std::map<std::string, std::string> hiddenChunks;
    int hiddenCounter = 0;

    // Allocate index for variable
    int getVarId(const std::string& name) {
        if (varTable.find(name) == varTable.end()) {
            int id = (int)vmMemory.size();
            varTable[name] = id;
            vmMemory.push_back(0.0); // Init with 0
            return id;
        }
        return varTable[name];
    }

    std::map<std::string, double> jsVars; // (Optional/Legacy?)
    TextEditor editor;
    bool playing = false;
    bool valid = false;
    char inputBuf[2048];
    std::string errorMsg;
    int errorPos = -1;

    // Audio
    uint32_t t = 0;
    double tAccum = 0.0;
    float volume = 1.0f;
    float scope[256] = { 0 };
    int scopeIdx = 0;

    // Settings
    const int rates[7] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000 };
    const char* rateNames[7] = { "8000 Hz", "11025 Hz", "16000 Hz", "22050 Hz", "32000 Hz", "44100 Hz", "48000 Hz" };
    int rateIdx = 0;

    // View/Export
    float zoomFactors[4] = { 1.0f, 2.0f, 4.0f, 8.0f };
    int zoomIdx = 0;
    float exportProgress = -1.0f;
    float successMsgTimer = 0.0f;
    std::string fileName = "";

    AppState(); // Constructor
};

extern AppState state;