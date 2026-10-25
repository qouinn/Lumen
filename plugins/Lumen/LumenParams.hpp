// Lumen — parameter layout shared by DSP and UI
#pragma once

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace lm {

enum : uint32_t { F_INT = 1, F_LOG = 2, F_BOOL = 4, F_OUT = 8 };

// one timeline section: its sound, its four macro values, where it starts (fraction of the sample) and the fade into it
#define LUMEN_SLOT(X, n) \
    X(kT##n##Snd,  "Section " #n " Sound",   "t" #n "_sound", 0, 511, 0, F_INT) \
    X(kT##n##M1,   "Section " #n " Macro 1", "t" #n "_m1",    0, 1, 0.5f, 0) \
    X(kT##n##M2,   "Section " #n " Macro 2", "t" #n "_m2",    0, 1, 0.5f, 0) \
    X(kT##n##M3,   "Section " #n " Macro 3", "t" #n "_m3",    0, 1, 0.5f, 0) \
    X(kT##n##M4,   "Section " #n " Macro 4", "t" #n "_m4",    0, 1, 0.5f, 0) \
    X(kT##n##Start,"Section " #n " Start",   "t" #n "_start", 0, 1, (n - 1) / 6.0f, 0) \
    X(kT##n##Fade, "Section " #n " Fade In", "t" #n "_fade",  0, 1, 0.5f, 0)

// X(id, name, symbol, min, max, def, flags)
#define LUMEN_PARAMS(X) \
    /* player / global */ \
    X(kPlay,      "Play",              "play",        0, 1, 0, F_INT | F_BOOL) \
    X(kLoop,      "Loop",              "loop",        0, 1, 1, F_INT | F_BOOL) \
    X(kKeys,      "Keys",              "keys",        0, 1, 0, F_INT) \
    X(kRegStart,  "Start",             "reg_start",   0, 1, 0, 0) \
    X(kRegEnd,    "End",               "reg_end",     0, 1, 1, 0) \
    X(kOriginal,  "Original Sound",    "original",    0, 1, 0, F_INT | F_BOOL) \
    X(kPlayFrom,  "Play From",         "play_from",   0, 1, 0, 0) \
    X(kCue,       "Play Cue",          "play_cue",    0, 100000, 0, F_INT) \
    X(kDryWet,    "Dry/Wet",           "drywet",      0, 1, 1, 0) \
    X(kOut,       "Output",            "output",      -36, 12, 0, 0) \
    X(kTrim,      "Preset Level",      "trim",        -24, 18, 0, 0) \
    X(kMacroSet,  "Macro Set",         "macro_set",   0, 511, 0, F_INT) \
    X(kM1,        "Macro 1",           "macro1",      0, 1, 0.5f, 0) \
    X(kM2,        "Macro 2",           "macro2",      0, 1, 0.5f, 0) \
    X(kM3,        "Macro 3",           "macro3",      0, 1, 0.5f, 0) \
    X(kM4,        "Macro 4",           "macro4",      0, 1, 0.5f, 0) \
    /* 1 GRAIN */ \
    X(kGOn,       "Grain On",          "g_on",        0, 1, 0, F_INT | F_BOOL) \
    X(kGMix,      "Grain Mix",         "g_mix",       0, 1, 0.6f, 0) \
    X(kGSize,     "Grain Size",        "g_size",      10, 800, 120, F_LOG) \
    X(kGDens,     "Grain Density",     "g_dens",      1, 80, 16, F_LOG) \
    X(kGPitch,    "Grain Pitch",       "g_pitch",     -24, 24, 0, 0) \
    X(kGSpread,   "Grain Spread",      "g_spread",    0, 1, 0.3f, 0) \
    X(kGRev,      "Grain Reverse",     "g_rev",       0, 1, 0, 0) \
    X(kGJit,      "Grain Jitter",      "g_jit",       0, 1, 0.1f, 0) \
    X(kGFreeze,   "Grain Freeze",      "g_freeze",    0, 1, 0, F_INT | F_BOOL) \
    /* 2 PITCH */ \
    X(kPOn,       "Pitch On",          "p_on",        0, 1, 0, F_INT | F_BOOL) \
    X(kPA,        "Pitch Voice A",     "p_a",         -24, 24, 12, 0) \
    X(kPB,        "Pitch Voice B",     "p_b",         -24, 24, -12, 0) \
    X(kPMixA,     "Pitch Level A",     "p_mix_a",     0, 1, 0.6f, 0) \
    X(kPMixB,     "Pitch Level B",     "p_mix_b",     0, 1, 0, 0) \
    X(kPDry,      "Pitch Dry",         "p_dry",       0, 1, 1, 0) \
    /* 3 DRIVE */ \
    X(kDOn,       "Drive On",          "d_on",        0, 1, 0, F_INT | F_BOOL) \
    X(kDMode,     "Drive Mode",        "d_mode",      0, 4, 0, F_INT) \
    X(kDAmt,      "Drive Amount",      "d_amt",       0, 1, 0.4f, 0) \
    X(kDTone,     "Drive Tone",        "d_tone",      0, 1, 0.6f, 0) \
    X(kDMix,      "Drive Mix",         "d_mix",       0, 1, 1, 0) \
    /* 4 FILTER */ \
    X(kFOn,       "Filter On",         "f_on",        0, 1, 0, F_INT | F_BOOL) \
    X(kFType,     "Filter Type",       "f_type",      0, 3, 0, F_INT) \
    X(kFCut,      "Filter Cutoff",     "f_cut",       20, 20000, 2000, F_LOG) \
    X(kFRes,      "Filter Reso",       "f_res",       0, 1, 0.2f, 0) \
    X(kFRate,     "Filter LFO Rate",   "f_rate",      0.02f, 20, 0.5f, F_LOG) \
    X(kFDepth,    "Filter LFO Depth",  "f_depth",     0, 1, 0, 0) \
    X(kFEnv,      "Filter Envelope",   "f_env",       -1, 1, 0, 0) \
    /* 5 LOFI */ \
    X(kLOn,       "Lo-Fi On",          "l_on",        0, 1, 0, F_INT | F_BOOL) \
    X(kLBits,     "Lo-Fi Bits",        "l_bits",      2, 16, 16, 0) \
    X(kLRate,     "Lo-Fi Rate",        "l_rate",      500, 48000, 48000, F_LOG) \
    X(kLWow,      "Lo-Fi Wow",         "l_wow",       0, 1, 0, 0) \
    X(kLFlutter,  "Lo-Fi Flutter",     "l_flutter",   0, 1, 0, 0) \
    X(kLNoise,    "Lo-Fi Hiss",        "l_noise",     0, 1, 0, 0) \
    X(kLCrackle,  "Lo-Fi Crackle",     "l_crackle",   0, 1, 0, 0) \
    X(kLAge,      "Lo-Fi Age",         "l_age",       0, 1, 0, 0) \
    /* 6 MOD */ \
    X(kMOn,       "Mod On",            "m_on",        0, 1, 0, F_INT | F_BOOL) \
    X(kMType,     "Mod Type",          "m_type",      0, 6, 0, F_INT) \
    X(kMRate,     "Mod Rate",          "m_rate",      0.02f, 20, 0.6f, F_LOG) \
    X(kMDepth,    "Mod Depth",         "m_depth",     0, 1, 0.5f, 0) \
    X(kMFb,       "Mod Feedback",      "m_fb",        0, 0.95f, 0.3f, 0) \
    X(kMMix,      "Mod Mix",           "m_mix",       0, 1, 0.5f, 0) \
    /* 7 STUTTER */ \
    X(kSOn,       "Stutter On",        "s_on",        0, 1, 0, F_INT | F_BOOL) \
    X(kSMode,     "Stutter Mode",      "s_mode",      0, 4, 0, F_INT) \
    X(kSDiv,      "Stutter Division",  "s_div",       0, 6, 3, F_INT) \
    X(kSAmt,      "Stutter Amount",    "s_amt",       0, 1, 0.5f, 0) \
    X(kSPat,      "Stutter Pattern",   "s_pat",       0, 7, 0, F_INT) \
    X(kSMix,      "Stutter Mix",       "s_mix",       0, 1, 1, 0) \
    /* 8 DELAY */ \
    X(kYOn,       "Delay On",          "y_on",        0, 1, 0, F_INT | F_BOOL) \
    X(kYDiv,      "Delay Time",        "y_div",       0, 8, 4, F_INT) \
    X(kYFb,       "Delay Feedback",    "y_fb",        0, 0.95f, 0.4f, 0) \
    X(kYPing,     "Delay Ping-Pong",   "y_ping",      0, 1, 1, F_INT | F_BOOL) \
    X(kYTone,     "Delay Tone",        "y_tone",      0, 1, 0.6f, 0) \
    X(kYMod,      "Delay Wobble",      "y_mod",       0, 1, 0.1f, 0) \
    X(kYMix,      "Delay Mix",         "y_mix",       0, 1, 0.3f, 0) \
    /* 9 REVERB */ \
    X(kROn,       "Reverb On",         "r_on",        0, 1, 0, F_INT | F_BOOL) \
    X(kRSize,     "Reverb Size",       "r_size",      0, 1, 0.6f, 0) \
    X(kRDecay,    "Reverb Decay",      "r_decay",     0.3f, 30, 3, F_LOG) \
    X(kRDamp,     "Reverb Damping",    "r_damp",      0, 1, 0.4f, 0) \
    X(kRPre,      "Reverb Predelay",   "r_pre",       0, 250, 20, 0) \
    X(kRShim,     "Reverb Shimmer",    "r_shim",      0, 1, 0, 0) \
    X(kRMod,      "Reverb Motion",     "r_mod",       0, 1, 0.3f, 0) \
    X(kRMix,      "Reverb Mix",        "r_mix",       0, 1, 0.3f, 0) \
    /* TIMELINE: fade between sounds over the sample */ \
    X(kTlOn,      "Timeline On",       "tl_on",       0, 1, 0, F_INT | F_BOOL) \
    X(kTlCount,   "Timeline Sections", "tl_count",    1, 6, 1, F_INT) \
    X(kTlCurve,   "Timeline Curve",    "tl_curve",    0, 2, 0, F_INT) \
    X(kTlTails,   "Timeline Tails",    "tl_tails",    0, 1, 0, F_INT) \
    LUMEN_SLOT(X, 1) LUMEN_SLOT(X, 2) LUMEN_SLOT(X, 3) LUMEN_SLOT(X, 4) LUMEN_SLOT(X, 5) LUMEN_SLOT(X, 6) \
    /* 1.2.1: every change = hard stop (all voices, echoes and reverb tails). Appended so older projects keep their parameter numbers. */ \
    X(kStop,      "Stop All",          "stop_all",    0, 100000, 0, F_INT)

#define LUMEN_ENUM(id, n, s, mn, mx, d, f) id,
enum Param : uint32_t {
    LUMEN_PARAMS(LUMEN_ENUM)
    kOutPos,          // playhead 0..1 (of the whole file)
    kOutLevel,        // output level 0..1
    kOutPlaying,      // 1 while the sample is sounding
    kOutBpm,
    kOutSection,      // timeline section that is loudest right now
    kParamCount
};
#undef LUMEN_ENUM

constexpr uint32_t kNumIn = kOutPos;

struct ParamInfo {
    const char* name; const char* symbol;
    float min, max, def; uint32_t flags;
    bool isInt() const { return flags & F_INT; }
    bool isLog() const { return flags & F_LOG; }
    bool isBool() const { return flags & F_BOOL; }
    bool isOut() const { return flags & F_OUT; }
};

inline ParamInfo getParamInfo(uint32_t i)
{
#define LUMEN_INFO(id, n, s, mn, mx, d, f) case id: return { n, s, (float)(mn), (float)(mx), (float)(d), (uint32_t)(f) };
    switch (i) {
    LUMEN_PARAMS(LUMEN_INFO)
    case kOutPos:     return { "Playhead", "out_pos", 0, 1, 0, F_OUT };
    case kOutLevel:   return { "Level", "out_level", 0, 1, 0, F_OUT };
    case kOutPlaying: return { "Playing", "out_playing", 0, 1, 0, F_OUT | F_INT };
    case kOutBpm:     return { "Host Tempo", "out_bpm", 20, 400, 120, F_OUT };
    case kOutSection: return { "Playing Section", "out_section", 0, 5, 0, F_OUT | F_INT };
    default: break;
    }
#undef LUMEN_INFO
    return { "?", "unknown", 0, 1, 0, 0 };
}

// settings a preset never changes (your sample, how it plays, the output level)
inline bool presetKeeps(uint32_t i) { return i == kPlay || i == kLoop || i == kKeys || i == kRegStart || i == kRegEnd || i == kOriginal || i == kOut || i == kPlayFrom || i == kCue || i >= kTlOn; }

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

// ---- timeline
constexpr int kMaxSections = 6, kSlotParams = 7;
enum SlotField { kSfSound = 0, kSfM1, kSfM2, kSfM3, kSfM4, kSfStart, kSfFade };
inline uint32_t slotParam(int s, int field) { return kT1Snd + (uint32_t)(s * kSlotParams + field); }
inline constexpr const char* kCurveNames[] = { "SMOOTH", "LINEAR", "SHARP" };
inline constexpr const char* kTailNames[]  = { "TAILS RING", "TAILS CUT" };
// section colors (UI) live here so the manual and UI agree
inline constexpr unsigned char kSecCol[kMaxSections][3] = { { 255, 120, 150 }, { 120, 200, 255 }, { 255, 196, 110 }, { 170, 140, 255 }, { 110, 230, 170 }, { 255, 150, 220 } };

// Where the sections start (sorted, fractions of the sample region) and the fade width at each boundary (fraction of the
// shorter neighbour, centred on the line; 0 = hard cut, 1 = fades reach the middle of both neighbours).
inline int sectionCount(const float* p) { const int n = (int)p[kTlCount]; return n < 1 ? 1 : (n > kMaxSections ? kMaxSections : n); }
inline float sectionStart(const float* p, int s) { return s <= 0 ? 0.0f : clampf(p[slotParam(s, kSfStart)], 0.0f, 1.0f); }

// Weights of every section at position x (0..1 of the region). Returns amplitudes in amp[] (sum of squares or sum = 1).
inline void sectionWeights(const float* p, float x, float* amp)
{
    const int n = sectionCount(p);
    const int curve = (int)p[kTlCurve];
    float c[kMaxSections + 1];   // c[k] = how far we are "into" section k or later
    c[0] = 1.0f;
    for (int k = 1; k < n; ++k) {
        const float b = sectionStart(p, k);
        const float lenL = b - sectionStart(p, k - 1), lenR = (k + 1 < n ? sectionStart(p, k + 1) : 1.0f) - b;
        const float w = clampf(p[slotParam(k, kSfFade)], 0, 1) * std::max(0.0f, std::min(lenL, lenR));
        float t;
        if (w < 1e-5f) t = x >= b ? 1.0f : 0.0f;
        else t = clampf((x - (b - w * 0.5f)) / w, 0, 1);
        if (curve == 2) t = clampf((t - 0.35f) / 0.3f, 0, 1);          // SHARP: the change happens in the middle third
        if (curve != 1) t = t * t * (3 - 2 * t);                        // SMOOTH / SHARP: s-curve
        c[k] = std::min(c[k - 1], t);
    }
    c[n] = 0.0f;
    for (int k = 0; k < kMaxSections; ++k) amp[k] = 0.0f;
    for (int k = 0; k < n; ++k) {
        const float w = std::max(0.0f, c[k] - c[k + 1]);
        amp[k] = curve == 1 ? w : std::sqrt(w);                         // LINEAR = straight blend, others = constant power
    }
}

// ---- names
inline constexpr const char* kKeysNames[]    = { "PITCH", "TRIGGER" };
inline constexpr const char* kDriveNames[]   = { "SOFT", "HARD", "FOLD", "FUZZ", "TUBE" };
inline constexpr const char* kFilterNames[]  = { "LP", "HP", "BP", "NOTCH" };
inline constexpr const char* kModNames[]     = { "CHORUS", "FLANGER", "PHASER", "TREMOLO", "PAN", "RING", "VIBRATO" };
inline constexpr const char* kStutNames[]    = { "REPEAT", "GATE", "TAPE STOP", "REVERSE", "HALF" };
inline constexpr const char* kStutDivNames[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/8T" };
inline constexpr float       kStutDivBeats[] = { 4, 2, 1, 0.5f, 0.25f, 0.125f, 1.0f / 3.0f };
inline constexpr const char* kDelayNames[]   = { "1/32", "1/16", "1/16D", "1/8", "1/8D", "1/4", "1/4D", "1/2", "1/1" };
inline constexpr float       kDelayBeats[]   = { 0.125f, 0.25f, 0.375f, 0.5f, 0.75f, 1, 1.5f, 2, 4 };

// the 9 modules (order = signal chain), each: on-param, first and last param
struct ModuleDef { const char* name; uint32_t on, first, last; };
inline constexpr ModuleDef kModules[9] = {
    { "GRAIN",   kGOn, kGMix,  kGFreeze },
    { "PITCH",   kPOn, kPA,    kPDry },
    { "DRIVE",   kDOn, kDMode, kDMix },
    { "FILTER",  kFOn, kFType, kFEnv },
    { "LO-FI",   kLOn, kLBits, kLAge },
    { "MOD",     kMOn, kMType, kMMix },
    { "STUTTER", kSOn, kSMode, kSMix },
    { "DELAY",   kYOn, kYDiv,  kYMix },
    { "REVERB",  kROn, kRSize, kRMix },
};

inline float toNorm(uint32_t i, float v) {
    const ParamInfo in = getParamInfo(i);
    if (in.isLog() && in.min > 0) return std::log(clampf(v, in.min, in.max) / in.min) / std::log(in.max / in.min);
    return (clampf(v, in.min, in.max) - in.min) / (in.max - in.min);
}
inline float fromNorm(uint32_t i, float n) {
    const ParamInfo in = getParamInfo(i);
    n = clampf(n, 0, 1);
    float v = in.isLog() && in.min > 0 ? in.min * std::pow(in.max / in.min, n) : in.min + n * (in.max - in.min);
    if (in.isInt()) v = std::round(v);
    return v;
}

} // namespace lm
