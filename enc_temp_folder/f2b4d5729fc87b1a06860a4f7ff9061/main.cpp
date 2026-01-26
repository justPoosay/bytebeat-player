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
#include <sstream>
#include <iomanip>
#include <vector>

using namespace std;

int main() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
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
        lang.mIdentifiers.insert(make_pair(string(k), id));
    }

    TextEditor::Identifier idT;
    idT.mDeclaration = "Time variable";
    lang.mIdentifiers.insert(make_pair("t", idT));
    state.editor.SetLanguageDefinition(lang);
    state.editor.SetText(state.inputBuf);

    // Init Compile
    state.valid = state.expr.Compile(state.inputBuf, state.errorMsg, state.errorPos);
    UpdateErrorMarkers();

    rlImGuiSetup(true);

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    static const ImWchar ranges[] = { 0x0020, 0x00FF, 0x0100, 0x017F, 0 };
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 16.0f, NULL, ranges);

    io.FontGlobalScale = 1.35f;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;

    bool firstFrame = true;
    int currentThemeIdx = 4;
    ApplyTheme(currentThemeIdx);
    const char* themeNames[] = { "Dark", "Light", "Classic", "Matrix Green", "Deep Ocean" };


    while (!WindowShouldClose()) {
        if (!io.WantCaptureKeyboard && IsKeyPressed(KEY_ENTER)) {
            state.playing = !state.playing;
        }
        if (IsKeyPressed(KEY_F11)) {
            ToggleFullscreen();
        }

        // DRAG & DROP
        if (IsFileDropped()) {
            FilePathList droppedFiles = LoadDroppedFiles();
            if (droppedFiles.count > 0) {
                const char* filePath = droppedFiles.paths[0];
                if (IsFileExtension(filePath, ".wav")) {
                    string fullCode = ConvertWavToBytebeat(filePath);
                    state.currentMode = AppState::BytebeatMode::JS_Compatible;
                    LoadCodeToEditor(fullCode);
                }
            }
            UnloadDroppedFiles(droppedFiles);
        }

        BeginDrawing();
        ClearBackground({ 15, 15, 20, 255 });
        rlImGuiBegin();

        // LIVE COMPILATION
        if (state.editor.IsTextChanged()) {
            string viewCode = state.editor.GetText(); // @HIDDEN

            // Expand @HIDDEN -> \x80\x80...
            string realCode = ExpandCode(viewCode);

            // Copy to inputBuf viewCode bc realCode causes buffer overflow
            strncpy(state.inputBuf, viewCode.c_str(), sizeof(state.inputBuf) - 1);
            state.inputBuf[sizeof(state.inputBuf) - 1] = '\0';

            state.errorMsg.clear();
            if (state.currentMode == AppState::BytebeatMode::C_Compatible)
                state.valid = state.expr.Compile(realCode, state.errorMsg, state.errorPos);
            else
                state.valid = state.complexEngine.Compile(realCode, state.errorMsg, state.errorPos);

            UpdateErrorMarkers();
        }

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(viewport->ID, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

        // Sprawdzamy: Pierwsza klatka LUB zmiana rozmiaru okna LUB wciśnięcie F11
        if (firstFrame || IsWindowResized() || IsKeyPressed(KEY_F11)) {
            firstFrame = false;

            // Czyścimy stary układ
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            // --- TWORZENIE UKŁADU 2x2 ---

            // 1. Dzielimy ekran na PÓŁ w pionie (Lewa / Prawa)
            ImGuiID dock_right_id;
            ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.5f, NULL, &dock_right_id);

            // 2. Dzielimy LEWĄ stronę na pół w poziomie (Góra: Editor / Dół: Settings)
            ImGuiID dock_settings_id; // Dół
            ImGuiID dock_editor_id = ImGui::DockBuilderSplitNode(dock_left_id, ImGuiDir_Up, 0.5f, NULL, &dock_settings_id);

            // 3. Dzielimy PRAWĄ stronę na pół w poziomie (Góra: Presets / Dół: Oscilloscope)
            ImGuiID dock_oscilloscope_id; // Dół
            ImGuiID dock_presets_id = ImGui::DockBuilderSplitNode(dock_right_id, ImGuiDir_Up, 0.5f, NULL, &dock_oscilloscope_id);

            // 4. Przypisujemy okna do konkretnych ćwiartek
            ImGui::DockBuilderDockWindow("Editor", dock_editor_id);           // Lewy Górny
            ImGui::DockBuilderDockWindow("Settings", dock_settings_id);       // Lewy Dolny
            ImGui::DockBuilderDockWindow("Presets", dock_presets_id);         // Prawy Górny
            ImGui::DockBuilderDockWindow("Oscilloscope", dock_oscilloscope_id); // Prawy Dolny

            ImGui::DockBuilderFinish(dockspace_id);
        }

        // --- EDITOR WINDOW ---
        ImGui::Begin("Editor");
        state.editor.Render("TextEditor", ImVec2(0, -60));

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

                    string viewCode = state.editor.GetText();
                    string realCode = ExpandCode(viewCode);

                    strncpy(state.inputBuf, viewCode.c_str(), sizeof(state.inputBuf) - 1);
                    state.inputBuf[sizeof(state.inputBuf) - 1] = '\0';

                    state.t = 0;
                    state.tAccum = 0.0;

                    if (state.currentMode == AppState::BytebeatMode::C_Compatible)
                        state.valid = state.expr.Compile(realCode, state.errorMsg, state.errorPos);
                    else
                        state.valid = state.complexEngine.Compile(realCode, state.errorMsg, state.errorPos);

                    UpdateErrorMarkers();
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();

        // FIT TO WINDOW
        if (ImGui::Button("Format", ImVec2(100, 40))) {
            string currentCode = state.editor.GetText();

            float availWidth = ImGui::GetContentRegionAvail().x;
            availWidth -= (50.0f + ImGui::GetStyle().ScrollbarSize);
            float charWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, "A").x;
            int maxChars = (int)(availWidth / charWidth);
            maxChars -= 1;

            string formatted = FormatCode(currentCode, maxChars);
            state.editor.SetText(formatted);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Wrap lines to fit window (minus scrollbar width)");
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

                // Text clipping
                string label = preset.code;
                if (label.length() > 40) {
                    label = label.substr(0, 40) + "...";
                }

                if (ImGui::Selectable(label.c_str())) {
                    if (preset.mode == PresetMode::Classic) {
                        state.currentMode = AppState::BytebeatMode::C_Compatible;
                    }
                    else {
                        state.currentMode = AppState::BytebeatMode::JS_Compatible;
                    }
                    LoadCodeToEditor(preset.code);

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
            string msg = state.errorMsg.empty() ? "Unknown Error" : state.errorMsg;
            string fullError = "ERROR: " + msg;

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

        // PLAY/PAUSE button
        const char* icon = state.playing ? "PAUSE" : "PLAY";
        ImVec2 btnSize(100, 64);
        ImVec2 btnCenter(p.x + sz.x / 2, p.y + sz.y / 2);
        ImVec2 btnMin(btnCenter.x - btnSize.x / 2, btnCenter.y - btnSize.y / 2);
        ImVec2 btnMax(btnCenter.x + btnSize.x / 2, btnCenter.y + btnSize.y / 2);

        bool oscHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
            ImGui::IsMouseHoveringRect(p, ImVec2(p.x + sz.x, p.y + sz.y));

        if (oscHovered) {
            dl->AddRectFilled(btnMin, btnMax, IM_COL32(60, 180, 255, 255), 16.0f);

            ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(28.0f, FLT_MAX, 0.0f, icon);
            ImVec2 textPos(btnCenter.x - textSize.x / 2, btnCenter.y - textSize.y / 2);
            textPos.y += 2.0f; // Correction in vertical axis
            dl->AddText(NULL, 28.0f, textPos, IM_COL32(255, 255, 255, 255), icon);

            // Tooltip
            ImGui::SetTooltip("LMB: Play/Pause\nRMB: Reset");

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                state.playing = !state.playing;
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                state.t = 0;
                state.tAccum = 0.0;
            }
        }
        ImGui::End();

        // --- SUCCESS POPUP ---
        if (state.successMsgTimer > 0) {
            state.successMsgTimer -= GetFrameTime();
            ImGui::SetNextWindowPos(ImVec2(GetScreenWidth() / 2, GetScreenHeight() / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
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