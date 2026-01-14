#pragma once
#include <string>
#include <vector>

struct BytebeatPreset {
    std::string title;
    std::string code;
    int sampleRate;

    BytebeatPreset(std::string t, std::string c, int sr = 8000) : title(t), code(c), sampleRate(sr) { }
};

extern std::vector<BytebeatPreset> g_presets;