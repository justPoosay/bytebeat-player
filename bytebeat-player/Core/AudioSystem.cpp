#include "AudioSystem.h"
#include "GlobalState.h"

void MyAudioCallback(void* buffer, unsigned int frames) {
    short* out = (short*)buffer;
    double tInc = (double)state.rates[state.rateIdx] / 44100.0;

    for (unsigned int i = 0; i < frames; i++) {
        if (state.playing && state.valid) {
            int v = 0;

            if (state.currentMode == AppState::BytebeatMode::C_Compatible) v = state.expr.Eval(state.t);
            else v = state.complexEngine.Eval(state.t);
            
            float sample = ((v & 0xFF) / 127.5f - 1.0f);
            out[i] = (short)(sample * 32767.0f * state.volume);

            if (i % 8 == 0) {
                state.scope[state.scopeIdx] = sample;
                state.scopeIdx = (state.scopeIdx + 1) % 256;
            }

            state.tAccum += tInc;
            if (state.tAccum >= 1.0) {
                uint32_t steps = (uint32_t)state.tAccum;
                state.t += steps;
                state.tAccum -= (double)steps;
            }
            if (state.tAccum > 100.0) state.tAccum = 0.0;
        }
        else out[i] = 0;
    }
}