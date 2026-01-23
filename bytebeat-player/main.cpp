#define _CRT_SECURE_NO_WARNINGS

#include "raylib.h"

#define RAYLIB_H
#ifndef NO_WINDESK
#define NO_WINDESK
#endif

// GUI libraries
#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"
#include "TextEditor.h"

// Modules
#include "Bytebeat.h"
#include "GlobalState.h"
#include "Presets.h"
#include "AudioSystem.h"
#include "Utils.h"

// Binary data
#include "icon_data.h" 

// Additional libraries
#include <string>
#include <cstdio>
#include <cstring>

using namespace std;

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Bytebeat Composer C++");
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

    // Init Editor
    auto lang = TextEditor::LanguageDefinition::C();

    static const char* const keywords[] = {
        "sin", "cos", "tan", "asin", "acos", "atan", "atan2", "sinh", "cosh", "tanh", 
        "exp", "log", "log10", "pow", "sqrt", 
        "ceil", "floor", "fmod", "round"
    };

    for (auto& k : keywords) {
        TextEditor::Identifier id;
        id.mDeclaration = "Built-in function";
        lang.mIdentifiers.insert(std::make_pair(std::string(k), id));
    }

    TextEditor::Identifier idT;
    idT.mDeclaration = "Time variable (counter)";
    lang.mIdentifiers.insert(std::make_pair("t", idT));
    state.editor.SetLanguageDefinition(lang);
    state.editor.SetText(state.inputBuf);

    // Init Compile
    state.valid = state.expr.Compile(state.inputBuf, state.errorMsg, state.errorPos);
    UpdateErrorMarkers();

    rlImGuiSetup(true);

    // --- POCZĄTEK MODYFIKACJI ---
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear(); // Usuwamy domyślną czcionkę bitmapową

    // Definiujemy zakres znaków do załadowania:
    // 0x0020 - 0x00FF: Basic Latin + Latin-1 Supplement (zawiera 0x91 i inne znaki specjalne)
    // 0x0100 - 0x017F: Latin Extended-A (polskie znaki ĄĘĆ...)
    static const ImWchar ranges[] = { 0x0020, 0x00FF, 0x0100, 0x017F, 0 };

    // Ładujemy czcionkę systemową (Consolas jest świetna do kodu)
    // Jeśli plik nie istnieje, ImGui użyje domyślnej, więc jest to bezpieczne.
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 16.0f, NULL, ranges);

    // Ważne: Przebudowujemy teksturę czcionek dla Rayliba/ImGui
    // --- KONIEC MODYFIKACJI ---

    io.FontGlobalScale = 1.35f;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;

    bool firstFrame = true;
    int currentThemeIdx = 4;
    ApplyTheme(currentThemeIdx);
    const char* themeNames[] = { "Dark", "Light", "Classic", "Matrix Green", "Deep Ocean" };

    while (!WindowShouldClose()) {
        if (!io.WantCaptureKeyboard && IsKeyPressed(KEY_ENTER)) state.playing = !state.playing;

        BeginDrawing();
        ClearBackground({ 15, 15, 20, 255 });
        rlImGuiBegin();

        // LIVE COMPILATION
        if (state.editor.IsTextChanged()) {
            string code = state.editor.GetText();
            // Copy to buffor
            strncpy(state.inputBuf, code.c_str(), sizeof(state.inputBuf) - 1);

            if (state.currentMode == AppState::BytebeatMode::C_Compatible) state.valid = state.expr.Compile(code, state.errorMsg, state.errorPos);
            else state.valid = state.complexEngine.Compile(code, state.errorMsg, state.errorPos); // Return errorPos

            UpdateErrorMarkers(); // Update red underlines
        }

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(viewport->ID, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

        if (firstFrame) {
            firstFrame = false;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            ImGuiID dock_id_bottom;
            ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.5f, NULL, &dock_id_bottom);
            ImGuiID dock_id_top_right;
            ImGuiID dock_id_top_left = ImGui::DockBuilderSplitNode(dock_id_top, ImGuiDir_Left, 0.5f, NULL, &dock_id_top_right);
            ImGuiID dock_id_bottom_right;
            ImGuiID dock_id_bottom_left = ImGui::DockBuilderSplitNode(dock_id_bottom, ImGuiDir_Left, 0.5f, NULL, &dock_id_bottom_right);

            ImGui::DockBuilderDockWindow("Editor", dock_id_top_left);
            ImGui::DockBuilderDockWindow("Presets", dock_id_top_right);
            ImGui::DockBuilderDockWindow("Settings", dock_id_bottom_left);
            ImGui::DockBuilderDockWindow("Oscilloscope", dock_id_bottom_right);

            ImGui::DockBuilderFinish(dockspace_id);
        }

        // --- EDITOR WINDOW ---
        ImGui::Begin("Editor");
        state.editor.Render("TextEditor", ImVec2(0, -60));

        string currentCode = state.editor.GetText();
        static string lastCode;
        if (currentCode != lastCode) {
            strncpy(state.inputBuf, currentCode.c_str(), sizeof(state.inputBuf) - 1);
            state.valid = state.expr.Compile(state.inputBuf, state.errorMsg, state.errorPos);
            lastCode = currentCode;
        }

        if (ImGui::Button(state.playing ? "STOP" : "PLAY", ImVec2(100, 40))) state.playing = !state.playing;
        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(100, 40))) { 
            state.t = 0; 
            state.tAccum = 0.0; 
        }
        ImGui::SameLine();

        char zoomBuf[32];
        if (state.zoomIdx == 0) sprintf(zoomBuf, "1x");
        else sprintf(zoomBuf, "1/%dx", (int)state.zoomFactors[state.zoomIdx]);

        if (ImGui::Button(zoomBuf, ImVec2(60, 40))) state.zoomIdx = (state.zoomIdx + 1) % 4;
        ImGui::SameLine();

        const char* modeNames[] = { "Classic (C)", "Javascript (JS)" };
        int currentModeIdx = (state.currentMode == AppState::BytebeatMode::C_Compatible) ? 0 : 1;
        ImGui::SetNextItemWidth(250.0f);
        if (ImGui::BeginCombo("##EngineMode", modeNames[currentModeIdx])) {
            for (int i = 0; i < 2; i++) {
                bool isSelected = (currentModeIdx == i);
                if (ImGui::Selectable(modeNames[i], isSelected)) {
                    state.currentMode = (i == 0) ? AppState::BytebeatMode::C_Compatible : AppState::BytebeatMode::JS_Compatible;

                    // Reset timer
                    state.t = 0;
                    state.tAccum = 0.0;

                    // Force recompilation immediately
                    string code = state.editor.GetText();

                    strncpy(state.inputBuf, code.c_str(), sizeof(state.inputBuf) - 1);
                    state.inputBuf[sizeof(state.inputBuf) - 1] = '\0';

                    if (state.currentMode == AppState::BytebeatMode::C_Compatible) state.valid = state.expr.Compile(code, state.errorMsg, state.errorPos);
                    else state.valid = state.complexEngine.Compile(code, state.errorMsg, state.errorPos);
                    
                    UpdateErrorMarkers();
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::End();

        // --- SETTINGS WINDOW ---
        ImGui::Begin("Settings");
        ImGui::Text("UI Appearance");
        if (ImGui::Combo("Theme", &currentThemeIdx, themeNames, 5)) ApplyTheme(currentThemeIdx);

        ImGui::Separator(); 
        ImGui::Spacing();

        ImGui::Text("Audio Engine");
        ImGui::SliderFloat("Volume", &state.volume, 0.0f, 1.0f, "%.2f");
        ImGui::Combo("Sample Rate", &state.rateIdx, state.rateNames, 7);

        ImGui::Spacing(); 
        ImGui::Separator(); 
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Playback Info");
        ImGui::Columns(2, "PlaybackLayout", false);
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.45f);

        ImGui::Text("Current t: %u", state.t);
        ImGui::Text("Time: %.2f s", (float)state.t / state.rates[state.rateIdx]);

        ImGui::NextColumn();
        float buttonWidth = ImGui::GetContentRegionAvail().x;

        if (ImGui::Button("Copy Formula", ImVec2(buttonWidth, 0))) ImGui::SetClipboardText(state.editor.GetText().c_str());
        if (!state.valid) ImGui::BeginDisabled();
        if (ImGui::Button("Quick Export (30s)", ImVec2(buttonWidth, 0))) ExportToWav();
        if (!state.valid) ImGui::EndDisabled();

        ImGui::Columns(1);
        ImGui::TextDisabled("Saves audio as .wav in project folder");
        ImGui::End();

        // --- PRESETS WINDOW ---
        ImGui::Begin("Presets");
        ImGui::TextDisabled("Click on a code to load it into the editor:");
        ImGui::Separator();

        if (ImGui::BeginChild("PresetList")) {
            for (const auto& preset : g_presets) {
                ImGui::PushID(preset.title.c_str());
                ImGui::TextColored({ 0.4f, 0.7f, 1.0f, 1.0f }, "%s", preset.title.c_str());

                if (ImGui::Selectable(preset.code.c_str(), false)) {
                    strncpy(state.inputBuf, preset.code.c_str(), sizeof(state.inputBuf) - 1);
                    state.inputBuf[sizeof(state.inputBuf) - 1] = '\0';
                    state.editor.SetText(preset.code); // This triggers IsTextChanged next frame

                    state.t = 0;
                    state.tAccum = 0.0;
                    state.playing = true;

                    // Auto-select sample rate
                    bool found = false;
                    for (int i = 0; i < 7; i++) {
                        if (state.rates[i] == preset.sampleRate) {
                            state.rateIdx = i;
                            found = true;
                            break;
                        }
                    }
                    if (!found) state.rateIdx = 0;
                }

                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to load preset");

                ImGui::Spacing(); 
                ImGui::Separator(); 
                ImGui::Spacing();
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::End();

        // --- OSCILLOSCOPE WINDOW ---
        ImGui::Begin("Oscilloscope");
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 sz = ImGui::GetContentRegionAvail();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (!state.valid) {
            std::string msg = state.errorMsg.empty() ? "Unknown Error" : state.errorMsg;
            std::string fullError = "ERROR: " + msg;

            // Center text
            ImVec2 textSize = ImGui::CalcTextSize(fullError.c_str());
            ImVec2 textPos = ImVec2(p.x + (sz.x - textSize.x) / 2, p.y + (sz.y - textSize.y) / 2);

            dl->AddText(textPos, IM_COL32(255, 80, 80, 255), fullError.c_str());
        }
        else {
            dl->AddRectFilled(p, { p.x + sz.x, p.y + sz.y }, IM_COL32(10, 10, 15, 255));
            dl->AddLine({ p.x, p.y + sz.y / 2 }, { p.x + sz.x, p.y + sz.y / 2 }, IM_COL32(100, 100, 120, 255), 1.0f);

            uint32_t triggeredT = FindTrigger(state.t);
            float numSamples = 512.0f * state.zoomFactors[state.zoomIdx];

            // Cache variables for loop performance
            const auto& expr = state.expr;
            auto& cxEngine = state.complexEngine;
            bool isJS = (state.currentMode == AppState::BytebeatMode::JS_Compatible);

            for (int n = 0; n < 255; n++) {
                float tIdx1 = ((float)n / 256.0f) * numSamples;
                float tIdx2 = ((float)(n + 1) / 256.0f) * numSamples;

                // Optimized call
                int v1 = isJS ? cxEngine.Eval(triggeredT + (uint32_t)tIdx1) : expr.Eval(triggeredT + (uint32_t)tIdx1);
                int v2 = isJS ? cxEngine.Eval(triggeredT + (uint32_t)tIdx2) : expr.Eval(triggeredT + (uint32_t)tIdx2);

                float x1 = p.x + (float)n / 256.0f * sz.x;
                float y1 = p.y + sz.y - (((v1 & 0xFF) / 255.0f) * sz.y);
                float x2 = p.x + (float)(n + 1) / 256.0f * sz.x;
                float y2 = p.y + sz.y - (((v2 & 0xFF) / 255.0f) * sz.y);

                dl->AddLine({ x1, y1 }, { x2, y2 }, ImColor::HSV(n / 256.0f, 0.8f, 1.0f), 2.5f);
            }
        }
        ImGui::End();

        // --- SUCCESS POPUP ---
        if (state.successMsgTimer > 0) {
            state.successMsgTimer -= GetFrameTime();
            ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::Begin("##Success", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "SUCCESS: '%s' saved!", state.fileName.c_str());
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