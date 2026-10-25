// Grainfield — parameter layout shared by DSP and UI (v1.1)
#pragma once

#include <cstdint>
#include <cstdio>
#include <cmath>

#include "AutoAdvance.hpp"

namespace gf {

constexpr int kMaxChops   = 8;
constexpr int kMaxSteps   = 16;
constexpr int kChopParams = 7;   // per-chop knobs

enum Mode { kModeHold = 0, kModeStep = 1, kModeKeys = 2 };
enum EnvShape { kEnvSquare = 0, kEnvTrapezoid, kEnvHann, kEnvGaussian, kEnvTriangle, kEnvCustom, kNumEnvShapes };
enum FilterType { kFilterOff = 0, kFilterLP, kFilterHP, kFilterBP };
enum ChopParam { kCSize = 0, kCRate, kCDensity, kCPosition, kCPitch, kCChaos, kCSpace };

enum Param : uint32_t {
    kMode = 0,
    kChops,
    kSelChop,
    kSync,
    kSnap,
    kEnvShapeP,
    kRegionStart,
    kRegionEnd,
    kFilterTypeP,
    kCutoff,
    kResonance,
    kAttack,
    kRelease,
    kDelayTime,
    kDelayFeedback,
    kDelayMix,
    kReverbSize,
    kReverbMix,
    kOutput,
    kStepCount,
    kStepRate,
    kStepGate,
    kStepSwing,
    kAudition,
    kStepFirst,                                    // 16 step chop slots (0 = rest)
    kStepPitchFirst = kStepFirst + kMaxSteps,      // 16 step pitch offsets
    kStepPosFirst = kStepPitchFirst + kMaxSteps,   // 16 step position offsets
    kChopFirst = kStepPosFirst + kMaxSteps,        // 8 chops x 7
    kAdvOn = kChopFirst + kMaxChops * kChopParams,  // auto-advance
    kAdvEvery,
    kAdvOrder,
    kAdvLen,                                        // LIST order: number of entries
    kAdvListFirst,                                  // LIST order: 16 chop numbers
    kAdvHoldFirst = kAdvListFirst + kAdvListMax,    // LIST order: 16 hold lengths (x EVERY)
    kOriginal = kAdvHoldFirst + kAdvListMax,        // ORIGINAL SOUND: plain sample playback, no grains / fx
    kOrigEnd,                                       // what ORIGINAL does at the end of a chop: loop / play on / stop
    kSplitCustom,                                   // chop cuts: 0 = even, 1 = your own cuts
    kSplitFirst,                                    // 7 cut positions (fraction of the region)
    kOutPlayStep = kSplitFirst + kMaxChops - 1,
    kOutActivity,
    kOutChop,
    kOutBpm,
    kOutAdvChop,
    kOutAdvProgress,
    kOutAdvPos,
    kParamCount
};

// performance / sample-layout settings that presets leave alone (auto-advance, ORIGINAL, your own chop cuts)
inline bool isAdvParam(uint32_t i) { return i >= kAdvOn && i < kOutPlayStep; }
// what a preset load keeps: auto-advance, ORIGINAL, the region crop, and your chop layout once you made your own cuts
inline bool presetKeeps(uint32_t i, const float* cur)
{
    if (isAdvParam(i) || i == kAudition || i == kSelChop || i == kRegionStart || i == kRegionEnd) return true;
    return cur[kSplitCustom] > 0.5f && i == kChops;
}
inline constexpr const char* kOrigEndNames[] = { "LOOP", "PLAY ON", "STOP" };

// chop c spans [a, b) as fractions of the region (even split, or your own cuts)
inline void chopBounds(const float* p, int c, double& a, double& b)
{
    int n = (int)p[kChops]; n = n < 1 ? 1 : (n > kMaxChops ? kMaxChops : n);
    c = c < 0 ? 0 : (c >= n ? n - 1 : c);
    if (p[kSplitCustom] < 0.5f || n == 1) { a = (double)c / n; b = (double)(c + 1) / n; return; }
    double cut[kMaxChops + 1]; cut[0] = 0; cut[n] = 1;
    for (int i = 1; i < n; ++i) {
        double v = p[kSplitFirst + i - 1];
        const double lo = cut[i - 1] + 0.002, hi = 1.0 - 0.002 * (n - i);
        cut[i] = v < lo ? lo : (v > hi ? hi : v);
    }
    a = cut[c]; b = cut[c + 1];
}
inline uint32_t chopParam(int chop, int which) { return kChopFirst + (uint32_t)(chop * kChopParams + which); }

struct ParamInfo {
    const char* name;
    const char* symbol;
    float min, max, def;
    bool integer, logarithmic, boolean, output;
};

inline constexpr float kStepRateBeats[] = { 1.0f, 0.5f, 0.25f, 0.125f, 1.0f / 3.0f, 1.0f / 6.0f };
inline constexpr const char* kStepRateNames[] = { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
constexpr int kNumStepRates = 6;

inline constexpr float kSyncDivBeats[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f };
inline constexpr const char* kSyncDivNames[] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128" };
constexpr int kNumSyncDivs = 8;

inline constexpr float kDelayBeats[] = { 0.25f, 0.375f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
inline constexpr const char* kDelayNames[] = { "1/16", "1/16D", "1/8", "1/8D", "1/4", "1/4D", "1/2" };
constexpr int kNumDelayTimes = 7;

inline constexpr const char* kModeNames[]   = { "HOLD", "STEP", "KEYS" };
inline constexpr const char* kEnvNames[]    = { "Square", "Trapezoid", "Hann", "Gaussian", "Triangle", "Custom" };
inline constexpr const char* kFilterNames[] = { "Off", "LP", "HP", "BP" };
inline constexpr const char* kChopParamNames[]   = { "Size", "Rate", "Density", "Position", "Pitch", "Chaos", "Space" };
inline constexpr const char* kChopParamSymbols[] = { "size", "rate", "density", "position", "pitch", "chaos", "space" };

inline constexpr float kChopMin[] = { 5.0f,    0.0f, 1.0f, 0.0f, -24.0f, 0.0f, 0.0f };
inline constexpr float kChopMax[] = { 2000.0f, 1.0f, 8.0f, 1.0f,  24.0f, 1.0f, 1.0f };
inline constexpr float kChopDef[] = { 120.0f,  0.55f, 2.0f, 0.3f,  0.0f, 0.15f, 0.35f };

inline ParamInfo getParamInfo(uint32_t i)
{
    static char nameBuf[kParamCount][32];
    static char symBuf[kParamCount][32];

    switch (i) {
    case kMode:          return { "Mode", "mode", 0, 2, 0, true, false, false, false };
    case kChops:         return { "Chops", "chops", 1, kMaxChops, 8, true, false, false, false };
    case kSelChop:       return { "Selected Chop", "sel_chop", 1, kMaxChops, 1, true, false, false, false };
    case kSync:          return { "Sync", "sync", 0, 1, 0, true, false, true, false };
    case kSnap:          return { "Pitch Snap", "snap", 0, 1, 1, true, false, true, false };
    case kEnvShapeP:     return { "Grain Envelope", "env_shape", 0, kNumEnvShapes - 1, 2, true, false, false, false };
    case kRegionStart:   return { "Region Start", "region_start", 0, 1, 0, false, false, false, false };
    case kRegionEnd:     return { "Region End", "region_end", 0, 1, 1, false, false, false, false };
    case kFilterTypeP:   return { "Filter Type", "filter_type", 0, 3, 0, true, false, false, false };
    case kCutoff:        return { "Cutoff", "cutoff", 20, 20000, 20000, false, true, false, false };
    case kResonance:     return { "Resonance", "resonance", 0, 1, 0.2f, false, false, false, false };
    case kAttack:        return { "Attack", "attack", 0.001f, 5.0f, 0.01f, false, true, false, false };
    case kRelease:       return { "Release", "release", 0.01f, 10.0f, 0.4f, false, true, false, false };
    case kDelayTime:     return { "Delay Time", "delay_time", 0, kNumDelayTimes - 1, 3, true, false, false, false };
    case kDelayFeedback: return { "Delay Feedback", "delay_fb", 0, 0.95f, 0.35f, false, false, false, false };
    case kDelayMix:      return { "Delay Mix", "delay_mix", 0, 1, 0.0f, false, false, false, false };
    case kReverbSize:    return { "Reverb Size", "reverb_size", 0, 1, 0.6f, false, false, false, false };
    case kReverbMix:     return { "Reverb Mix", "reverb_mix", 0, 1, 0.5f, false, false, false, false };
    case kOutput:        return { "Output", "output", -48.0f, 12.0f, 0.0f, false, false, false, false };
    case kStepCount:     return { "Steps", "step_count", 1, kMaxSteps, 16, true, false, false, false };
    case kStepRate:      return { "Step Rate", "step_rate", 0, kNumStepRates - 1, 2, true, false, false, false };
    case kStepGate:      return { "Step Gate", "step_gate", 0.05f, 1.0f, 0.9f, false, false, false, false };
    case kStepSwing:     return { "Swing", "swing", 0, 0.75f, 0.0f, false, false, false, false };
    case kAudition:      return { "Play", "play", 0, 1, 0, true, false, true, false };
    case kOutPlayStep:   return { "Playing Step", "play_step", -1, kMaxSteps - 1, -1, true, false, false, true };
    case kOutActivity:   return { "Activity", "activity", 0, 1, 0, false, false, false, true };
    case kOutChop:       return { "Last Chop", "last_chop", 0, kMaxChops - 1, 0, true, false, false, true };
    case kOutBpm:        return { "Host Tempo", "host_bpm", 20, 400, 120, false, false, false, true };
    case kAdvOn:         return { "Auto Advance", "adv_on", 0, 1, 0, true, false, true, false };
    case kAdvEvery:      return { "Advance Every", "adv_every", 0, 11, 2, true, false, false, false };
    case kAdvOrder:      return { "Advance Order", "adv_order", 0, kNumAdvOrders - 1, 0, true, false, false, false };
    case kOriginal:      return { "Original Sound", "original", 0, 1, 0, true, false, true, false };
    case kOrigEnd:       return { "Original Chop End", "orig_end", 0, 2, 0, true, false, false, false };
    case kSplitCustom:   return { "Own Chop Cuts", "split_custom", 0, 1, 0, true, false, true, false };
    case kAdvLen:        return { "Advance List Length", "adv_len", 1, kAdvListMax, 4, true, false, false, false };
    case kOutAdvPos:     return { "Advance List Position", "adv_pos", 0, kAdvListMax - 1, 0, true, false, false, true };
    case kOutAdvChop:    return { "Active Chop", "adv_chop", 0, kMaxChops - 1, 0, true, false, false, true };
    case kOutAdvProgress:return { "Advance Progress", "adv_progress", 0, 1, 0, false, false, false, true };
    default: break;
    }

    if (i >= kAdvListFirst && i < kAdvListFirst + kAdvListMax) {
        const int s = (int)(i - kAdvListFirst);
        std::snprintf(nameBuf[i], 32, "Advance List %d", s + 1);
        std::snprintf(symBuf[i], 32, "adv_list%d", s + 1);
        return { nameBuf[i], symBuf[i], 1, kMaxChops, (float)(s % kMaxChops + 1), true, false, false, false };
    }
    if (i >= kSplitFirst && i < kSplitFirst + kMaxChops - 1) {
        const int s = (int)(i - kSplitFirst);
        std::snprintf(nameBuf[i], 32, "Chop Cut %d", s + 1);
        std::snprintf(symBuf[i], 32, "cut%d", s + 1);
        return { nameBuf[i], symBuf[i], 0, 1, (float)(s + 1) / kMaxChops, false, false, false, false };
    }
    if (i >= kAdvHoldFirst && i < kAdvHoldFirst + kAdvListMax) {
        const int s = (int)(i - kAdvHoldFirst);
        std::snprintf(nameBuf[i], 32, "Advance Hold %d", s + 1);
        std::snprintf(symBuf[i], 32, "adv_hold%d", s + 1);
        return { nameBuf[i], symBuf[i], 1, kAdvListMax, 1, true, false, false, false };
    }
    if (i >= kStepFirst && i < kStepPitchFirst) {
        const int s = (int)(i - kStepFirst);
        std::snprintf(nameBuf[i], 32, "Step %d Chop", s + 1);
        std::snprintf(symBuf[i], 32, "step%d", s + 1);
        return { nameBuf[i], symBuf[i], 0, kMaxChops, (float)((s % kMaxChops) + 1), true, false, false, false };
    }
    if (i >= kStepPitchFirst && i < kStepPosFirst) {
        const int s = (int)(i - kStepPitchFirst);
        std::snprintf(nameBuf[i], 32, "Step %d Pitch", s + 1);
        std::snprintf(symBuf[i], 32, "step%d_pitch", s + 1);
        return { nameBuf[i], symBuf[i], -24, 24, 0, true, false, false, false };
    }
    if (i >= kStepPosFirst && i < kChopFirst) {
        const int s = (int)(i - kStepPosFirst);
        std::snprintf(nameBuf[i], 32, "Step %d Position", s + 1);
        std::snprintf(symBuf[i], 32, "step%d_pos", s + 1);
        return { nameBuf[i], symBuf[i], -0.5f, 0.5f, 0, false, false, false, false };
    }
    if (i >= kChopFirst && i < kAdvOn) {
        const int c = (int)((i - kChopFirst) / kChopParams);
        const int k = (int)((i - kChopFirst) % kChopParams);
        std::snprintf(nameBuf[i], 32, "Chop %d %s", c + 1, kChopParamNames[k]);
        std::snprintf(symBuf[i], 32, "c%d_%s", c + 1, kChopParamSymbols[k]);
        return { nameBuf[i], symBuf[i], kChopMin[k], kChopMax[k], kChopDef[k], k == kCDensity, k == kCSize, false, false };
    }
    return { "?", "unknown", 0, 1, 0, false, false, false, false };
}

// Rate knob -> free rate in grains per second (0.5..120, exponential)
inline float rateToHz(float r) { return 0.5f * std::pow(240.0f, r); }
inline int rateToSyncDiv(float r) { int d = (int)std::lround(r * (kNumSyncDivs - 1)); return d < 0 ? 0 : (d >= kNumSyncDivs ? kNumSyncDivs - 1 : d); }

} // namespace gf
