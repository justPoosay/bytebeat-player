#pragma once
#include <cstdint>
#include <string>

void ApplyTheme(int themeIdx);
void UpdateErrorMarkers();
void ExportToWav();
void LoadCodeToEditor(std::string fullCode);
std::string ConvertWavToBytebeat(const char* filePath);
std::string CompressCode(const std::string& fullCode); // Ukrywa długie stringi
std::string ExpandCode(const std::string& shortCode);  // Przywraca długie stringi
uint32_t FindTrigger(uint32_t currentT);