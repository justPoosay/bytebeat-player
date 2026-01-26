#pragma once
#include <cstdint>
#include <string>

void ApplyTheme(int themeIdx);
void UpdateErrorMarkers();
void ExportToWav();
void LoadCodeToEditor(std::string fullCode);
std::string FormatCode(const std::string& code, int maxChars);
std::string ConvertWavToBytebeat(const char* filePath);
std::string CompressCode(const std::string& fullCode);
std::string ExpandCode(const std::string& shortCode);
uint32_t FindTrigger(uint32_t currentT);