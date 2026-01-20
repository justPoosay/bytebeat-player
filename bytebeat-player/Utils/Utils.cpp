#define _CRT_SECURE_NO_WARNINGS
#include "Utils.h"
#include "GlobalState.h"
#include "imgui.h"
#include "TextEditor.h"

#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

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
        std::string code = state.editor.GetText();
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

    std::string fileName = "bytebeat_" + std::string(timeStr) + ".wav";
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

uint32_t FindTrigger(uint32_t currentT) {
    const int searchRange = 1024;
    for (uint32_t i = 0; i < searchRange; i++) {
        int valNow = state.expr.Eval(currentT + i) & 0xFF;
        int valNext = state.expr.Eval(currentT + i + 1) & 0xFF;

        if (valNow <= 2 && valNext > 2) return currentT + i;
    }
    return currentT;
}