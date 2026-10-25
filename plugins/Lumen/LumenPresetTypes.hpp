// Lumen — preset / macro data types and the macro mapping shared by DSP and UI
#pragma once

#include "LumenParams.hpp"

namespace lm {

struct MacroTarget { uint16_t param; float lo, hi; };
struct Macro { const char* label; uint8_t n; MacroTarget t[4]; };
struct PresetVal { uint16_t param; float v; };
struct Preset {
    const char* name;
    uint8_t cat;
    const char* desc;
    const PresetVal* vals; uint8_t nVals;
    Macro m[4];
    float mdef[4];
    float trim;
};

inline constexpr const char* kCatNames[] = {
    "GRANULAR", "SHIMMER & SPACE", "PADS & TEXTURES", "RHYTHM & GLITCH",
    "LO-FI & TAPE", "PITCH & HARMONY", "FILTER & MOTION", "DRIVE & CHARACTER",
    "BREAKCORE & CHAOS", "DREAMY & NOSTALGIC", "CINEMATIC & DARK", "SCI-FI & EXPERIMENTAL"
};
constexpr int kNumCats = 12;

} // namespace lm

#include "LumenPresets.hpp"   // generated: kPresets[], kNumPresets

namespace lm {

inline const Preset& presetAt(int i) { return kPresets[i < 0 ? 0 : (i >= kNumPresets ? kNumPresets - 1 : i)]; }

// Fill p[0..kNumIn) with a preset (all modules off unless the preset turns them on). Keeps nothing; callers decide.
inline void presetValues(int idx, float* p)
{
    for (uint32_t i = 0; i < kNumIn; ++i) p[i] = getParamInfo(i).def;
    const Preset& pr = presetAt(idx);
    for (int k = 0; k < pr.nVals; ++k) p[pr.vals[k].param] = pr.vals[k].v;
    p[kMacroSet] = (float)idx;
    p[kM1] = pr.mdef[0]; p[kM2] = pr.mdef[1]; p[kM3] = pr.mdef[2]; p[kM4] = pr.mdef[3];
    p[kTrim] = pr.trim;
}

// Effective values: the preset's macros drive their target parameters.
inline void applyMacros(const float* p, float* q)
{
    for (uint32_t i = 0; i < kNumIn; ++i) q[i] = p[i];
    const Preset& pr = presetAt((int)p[kMacroSet]);
    for (int m = 0; m < 4; ++m) {
        const float mv = clampf(p[kM1 + m], 0, 1);
        for (int t = 0; t < pr.m[m].n; ++t) {
            const MacroTarget& mt = pr.m[m].t[t];
            const float a = toNorm(mt.param, mt.lo), b = toNorm(mt.param, mt.hi);
            q[mt.param] = fromNorm(mt.param, a + (b - a) * mv);
        }
    }
}

// which macro (0..3) drives a parameter in the current set, or -1
inline int macroFor(const float* p, uint32_t param)
{
    const Preset& pr = presetAt((int)p[kMacroSet]);
    for (int m = 0; m < 4; ++m) for (int t = 0; t < pr.m[m].n; ++t) if (pr.m[m].t[t].param == param) return m;
    return -1;
}

} // namespace lm
