// Lumen — preset / macro data types and the macro mapping shared by DSP and UI
#pragma once

#include "LumenParams.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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
    "BREAKCORE & CHAOS", "DREAMY & NOSTALGIC", "CINEMATIC & DARK", "SCI-FI & EXPERIMENTAL", "BEAUTIFUL TRANSFORMS"
};
constexpr int kNumCats = 13;   // built-in categories (OWN PRESETS comes after them in the UI)

} // namespace lm

#include "LumenPresets.hpp"   // generated: kPresets[], kNumPresets

namespace lm {

// ---------------------------------------------------------------------------------------------
// OWN PRESETS: sounds you saved yourself. They use sound numbers kOwnBase.. (the built-in ones stay 0..kNumPresets-1),
// keep the four macros of the sound they started from, and live in Documents/Lumen/Own Presets.txt plus a copy in
// every project that uses them. The table is shared by the plugin and its editor (same process); the audio thread only
// reads numbers from it, and a slot is filled completely before it is marked as used.
constexpr int kOwnBase = 300, kOwnMax = 200, kOwnCat = kNumCats;   // category index used for OWN PRESETS in the UI
constexpr uint32_t kEditFirst = kGOn, kEditLast = kRMix;           // the sound's module settings
constexpr int kNumEdit = (int)(kEditLast - kEditFirst + 1);
constexpr float kNoEdit = 3.0e38f;                                 // "not edited" marker (fast-math safe, unlike NaN)
inline bool hasEdit(float v) { return v < 1.0e37f; }

struct OwnPreset {
    std::atomic<int> state { 0 };   // 0 free, 1 in the library, 2 removed from the library but still used this session
    char name[48] = { 0 }, desc[96] = { 0 };
    int base = 0;                   // the built-in sound it started from (its macros)
    float v[kNumEdit] = { 0 }, mdef[4] = { 0.5f, 0.5f, 0.5f, 0.5f }, trim = 0;
    Preset view {};
};
inline OwnPreset* ownTable() { static OwnPreset t[kOwnMax]; return t; }
inline bool isOwn(int idx) { return idx >= kOwnBase && idx < kOwnBase + kOwnMax; }
inline OwnPreset* ownAt(int idx) { if (!isOwn(idx)) return nullptr; OwnPreset* o = &ownTable()[idx - kOwnBase]; return o->state.load(std::memory_order_acquire) ? o : nullptr; }
inline bool soundExists(int idx) { return (idx >= 0 && idx < kNumPresets) || ownAt(idx) != nullptr; }

inline const Preset& presetAt(int i)
{
    if (const OwnPreset* o = ownAt(i)) return o->view;
    return kPresets[i < 0 || isOwn(i) ? 0 : (i >= kNumPresets ? kNumPresets - 1 : i)];
}

// Fill p[0..kNumIn) with a preset (all modules off unless the preset turns them on). Keeps nothing; callers decide.
inline void presetValues(int idx, float* p)
{
    const OwnPreset* o = ownAt(idx);
    const int src = o ? o->base : idx;
    for (uint32_t i = 0; i < kNumIn; ++i) p[i] = getParamInfo(i).def;
    const Preset& pr = presetAt(src);
    for (int k = 0; k < pr.nVals; ++k) p[pr.vals[k].param] = pr.vals[k].v;
    p[kMacroSet] = (float)(o ? idx : src);
    p[kM1] = pr.mdef[0]; p[kM2] = pr.mdef[1]; p[kM3] = pr.mdef[2]; p[kM4] = pr.mdef[3];
    p[kTrim] = pr.trim;
    if (o) {
        for (int k = 0; k < kNumEdit; ++k) p[kEditFirst + (uint32_t)k] = o->v[k];
        for (int m = 0; m < 4; ++m) p[kM1 + (uint32_t)m] = o->mdef[m];
        p[kTrim] = o->trim;
    }
}
// a timeline section's settings: its sound plus the edits made to it on the EDIT page
inline void sectionValues(int idx, const float* ov, float* p)
{
    presetValues(idx, p);
    if (ov) for (int k = 0; k < kNumEdit; ++k) if (hasEdit(ov[k])) p[kEditFirst + (uint32_t)k] = ov[k];
}

// ---- text formats (project state and the library file). Settings are stored by symbol, so they survive new versions.
inline uint32_t paramBySymbol(const char* sym, size_t n)
{
    for (uint32_t i = 0; i < kNumIn; ++i) { const char* s = getParamInfo(i).symbol; if (std::strlen(s) == n && !std::strncmp(s, sym, n)) return i; }
    return kParamCount;
}
// section edits:  "3:g_mix=0.5,r_on=1;7:..."  (section numbers from 0)
inline std::string editsToText(const float (*ov)[kNumEdit], int sections)
{
    std::string out; char b[64];
    for (int s = 0; s < sections; ++s) {
        bool any = false;
        for (int k = 0; k < kNumEdit; ++k) if (hasEdit(ov[s][k])) {
            if (!any) std::snprintf(b, sizeof b, "%s%d:%s=%.6g", out.empty() ? "" : ";", s, getParamInfo(kEditFirst + (uint32_t)k).symbol, (double)ov[s][k]);
            else std::snprintf(b, sizeof b, ",%s=%.6g", getParamInfo(kEditFirst + (uint32_t)k).symbol, (double)ov[s][k]);
            out += b; any = true;
        }
    }
    return out;
}
inline void editsFromText(const char* t, float (*ov)[kNumEdit], int sections)
{
    for (int s = 0; s < sections; ++s) for (int k = 0; k < kNumEdit; ++k) ov[s][k] = kNoEdit;
    if (!t) return;
    while (*t) {
        char* e = nullptr; const long s = std::strtol(t, &e, 10);
        if (e == t || *e != ':') break;
        t = e + 1;
        while (*t && *t != ';') {
            const char* eq = std::strchr(t, '='); if (!eq) return;
            const uint32_t pi = paramBySymbol(t, (size_t)(eq - t));
            const float v = std::strtof(eq + 1, &e);
            if (s >= 0 && s < sections && pi >= kEditFirst && pi <= kEditLast) ov[s][pi - kEditFirst] = v;
            t = e; if (*t == ',') ++t; else if (*t && *t != ';') return;
        }
        if (*t == ';') ++t;
    }
}
// one own preset per line:  slot|name|base|trim|m1,m2,m3,m4|g_on=1,g_mix=0.7,...
inline std::string ownToLine(int slot, const OwnPreset& o)
{
    std::string name = o.name; for (char& c : name) if (c == '|' || c == '\n' || c == '\r') c = ' ';
    char b[160]; std::snprintf(b, sizeof b, "%d|%s|%d|%.4g|%.4g,%.4g,%.4g,%.4g|", slot, name.c_str(), o.base, (double)o.trim, (double)o.mdef[0], (double)o.mdef[1], (double)o.mdef[2], (double)o.mdef[3]);
    std::string out = b;
    for (int k = 0; k < kNumEdit; ++k) { std::snprintf(b, sizeof b, "%s%s=%.6g", k ? "," : "", getParamInfo(kEditFirst + (uint32_t)k).symbol, (double)o.v[k]); out += b; }
    return out;
}
inline void ownFinish(OwnPreset& o)   // builds the view the rest of Lumen sees (name, category, the base sound's macros)
{
    const Preset& b = kPresets[o.base < 0 || o.base >= kNumPresets ? 0 : o.base];
    std::snprintf(o.desc, sizeof o.desc, "Your own sound, made from %s.", b.name);
    o.view = b;
    o.view.name = o.name; o.view.desc = o.desc; o.view.cat = (uint8_t)kOwnCat;
    for (int m = 0; m < 4; ++m) o.view.mdef[m] = o.mdef[m];
    o.view.trim = o.trim;
}
// parse one line into a slot (only when the slot is free, or `force`); returns the slot or -1
inline int ownFromLine(const std::string& line, int keepState, bool force)
{
    int slot = -1, base = 0; float trim = 0, md[4] = { 0.5f, 0.5f, 0.5f, 0.5f };
    const size_t a = line.find('|'), b = a == std::string::npos ? a : line.find('|', a + 1), c = b == std::string::npos ? b : line.find('|', b + 1),
                 d = c == std::string::npos ? c : line.find('|', c + 1), e = d == std::string::npos ? d : line.find('|', d + 1);
    if (e == std::string::npos) return -1;
    slot = std::atoi(line.substr(0, a).c_str());
    if (slot < 0 || slot >= kOwnMax) return -1;
    OwnPreset& o = ownTable()[slot];
    if (o.state.load() && !force) return -1;
    const std::string name = line.substr(a + 1, b - a - 1);
    base = std::atoi(line.substr(b + 1, c - b - 1).c_str());
    trim = std::strtof(line.substr(c + 1, d - c - 1).c_str(), nullptr);
    std::sscanf(line.substr(d + 1, e - d - 1).c_str(), "%f,%f,%f,%f", &md[0], &md[1], &md[2], &md[3]);
    float pv[kParamCount]; presetValues(base < 0 || base >= kNumPresets ? 0 : base, pv);
    o.state.store(0, std::memory_order_release);
    for (int k = 0; k < kNumEdit; ++k) o.v[k] = pv[kEditFirst + (uint32_t)k];
    const char* t = line.c_str() + e + 1;
    while (*t) {
        const char* eq = std::strchr(t, '='); if (!eq) break;
        const uint32_t pi = paramBySymbol(t, (size_t)(eq - t));
        char* end = nullptr; const float v = std::strtof(eq + 1, &end);
        if (pi >= kEditFirst && pi <= kEditLast) o.v[pi - kEditFirst] = v;
        t = end; if (*t == ',') ++t; else break;
    }
    std::snprintf(o.name, sizeof o.name, "%s", name.c_str());
    o.base = base < 0 || base >= kNumPresets ? 0 : base; o.trim = trim;
    for (int m = 0; m < 4; ++m) o.mdef[m] = clampf(md[m], 0, 1);
    ownFinish(o);
    o.state.store(keepState, std::memory_order_release);
    return slot;
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
