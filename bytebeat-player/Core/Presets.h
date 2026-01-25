#pragma once
#include <string>
#include <vector>

enum class PresetMode {
    Classic,
    Complex
};

struct BytebeatPreset {
    std::string title;
    std::string code;
    int sampleRate;
    PresetMode mode;

    BytebeatPreset(std::string t, std::string c, int sr = 8000, PresetMode m = PresetMode::Classic) : 
        title(t), code(c), sampleRate(sr), mode(m) { }

    BytebeatPreset(std::string t, std::string c, PresetMode m) : 
        title(t), code(c), sampleRate(8000), mode(m) { }
};

extern std::vector<BytebeatPreset> g_presets;