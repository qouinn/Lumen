// Grainfield — granular engine (framework independent), v1.1
#pragma once

#include "Params.hpp"
#include "AutoAdvance.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace gf {

constexpr int kMaxVoices    = 16;
constexpr int kMaxGrains    = 64;
constexpr int kWinSize      = 1024;
constexpr int kCustomPoints = 32;
constexpr float kPi = 3.14159265358979f;

struct SampleData {
    std::vector<float> L, R;
    double rate = 44100.0;
    size_t frames() const { return L.size(); }
    bool empty() const { return L.size() < 16; }
};

struct Rng {
    uint32_t s = 0x9E3779B9u;
    inline float u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float)(s >> 8) * (1.0f / 16777216.0f); }
    inline float bi() { return u() * 2.0f - 1.0f; }
};

struct MidiMsg { uint32_t frame; uint8_t status, d1, d2; };

struct TimeInfo {
    bool playing = false;
    bool valid = false;
    double ppq = 0.0;   // beat position at block start
    double bpm = 120.0;
    double beatsPerBar = 4.0;
};

// ------------------------------------------------------------------------------------------------
// Freeverb-style stereo reverb

class Reverb {
    struct Comb {
        std::vector<float> buf; size_t idx = 0; float store = 0;
        inline float process(float in, float fb, float damp) {
            float out = buf[idx];
            store = out * (1.0f - damp) + store * damp;
            buf[idx] = in + store * fb;
            if (++idx >= buf.size()) idx = 0;
            return out;
        }
    };
    struct Allpass {
        std::vector<float> buf; size_t idx = 0;
        inline float process(float in) {
            float b = buf[idx];
            float out = -in + b;
            buf[idx] = in + b * 0.5f;
            if (++idx >= buf.size()) idx = 0;
            return out;
        }
    };
    Comb combL[8], combR[8];
    Allpass apL[4], apR[4];
    float fb = 0.84f, damp = 0.3f;
public:
    void init(double sr) {
        static const int combT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int apT[4]   = { 556, 441, 341, 225 };
        const double sc = sr / 44100.0;
        for (int i = 0; i < 8; ++i) {
            combL[i].buf.assign((size_t)std::max(8.0, combT[i] * sc), 0.0f); combL[i].idx = 0; combL[i].store = 0;
            combR[i].buf.assign((size_t)std::max(8.0, (combT[i] + 23) * sc), 0.0f); combR[i].idx = 0; combR[i].store = 0;
        }
        for (int i = 0; i < 4; ++i) {
            apL[i].buf.assign((size_t)std::max(8.0, apT[i] * sc), 0.0f); apL[i].idx = 0;
            apR[i].buf.assign((size_t)std::max(8.0, (apT[i] + 23) * sc), 0.0f); apR[i].idx = 0;
        }
    }
    void setSize(float s) { fb = 0.70f + 0.27f * s; damp = 0.45f - 0.25f * s; }
    inline void process(float inL, float inR, float& outL, float& outR) {
        const float in = (inL + inR) * 0.015f;
        float l = 0, r = 0;
        for (int i = 0; i < 8; ++i) { l += combL[i].process(in, fb, damp); r += combR[i].process(in, fb, damp); }
        for (int i = 0; i < 4; ++i) { l = apL[i].process(l); r = apR[i].process(r); }
        outL = l * 2.2f; outR = r * 2.2f;
    }
};

// ------------------------------------------------------------------------------------------------
// Tempo-synced stereo ping-pong delay

class Delay {
    std::vector<float> bl, br;
    size_t w = 0;
    double cur = 1000.0;
    float lpL = 0, lpR = 0;
public:
    void init(double sr) { const size_t n = (size_t)(sr * 4.5) + 4; bl.assign(n, 0.0f); br.assign(n, 0.0f); w = 0; lpL = lpR = 0; }
    inline float readAt(const std::vector<float>& b, double d) const {
        const size_t n = b.size();
        double rp = (double)w - d; while (rp < 0) rp += (double)n;
        const size_t i0 = (size_t)rp % n, i1 = (i0 + 1) % n; const float f = (float)(rp - std::floor(rp));
        return b[i0] * (1 - f) + b[i1] * f;
    }
    inline void process(float& l, float& r, double targetSamples, float fb, float mix) {
        if (bl.empty()) return;
        targetSamples = std::min((double)bl.size() - 4.0, std::max(16.0, targetSamples));
        cur += (targetSamples - cur) * 0.0005;  // glide on time changes
        const float dl = readAt(bl, cur), dr = readAt(br, cur);
        lpL += (dr - lpL) * 0.45f; lpR += (dl - lpR) * 0.45f; // damped cross-feedback (ping-pong)
        bl[w] = l + lpL * fb;
        br[w] = r * 0.6f + lpR * fb;
        if (++w >= bl.size()) w = 0;
        l += dl * mix; r += dr * mix;
    }
};

// ------------------------------------------------------------------------------------------------

class GrainEngine {
public:
    float params[kParamCount];

    GrainEngine() {
        for (uint32_t i = 0; i < kParamCount; ++i) params[i] = getParamInfo(i).def;
        for (int i = 0; i < kCustomPoints; ++i) custom[i] = std::sin(kPi * (float)i / (kCustomPoints - 1));
        buildWindows();
        for (auto& v : voices) v.active = false;
    }

    void setSampleRate(double sr) {
        sampleRate = sr > 1000.0 ? sr : 44100.0;
        reverb.init(sampleRate);
        delay.init(sampleRate);
        reset();
    }

    void reset() {
        for (auto& v : voices) v.active = false;
        heldCount = 0; seqRunning = false; lastStepIdx = INT64_MIN; playStep = -1;
        activity = 0.0f; auditionOn = false; auditionNote = -1; adv.restart();
    }

    void setSample(const SampleData* s) { sample = s; for (auto& v : voices) v.active = false; }

    void setCustomEnvelope(const float* pts) {
        for (int i = 0; i < kCustomPoints; ++i) custom[i] = std::min(1.0f, std::max(0.0f, pts[i]));
        buildCustomWindow();
    }
    const float* getCustomEnvelope() const { return custom; }

    int getPlayStep() const { return playStep; }
    float getActivity() const { return activity; }
    int getLastChop() const { return lastChop; }
    double getBpm() const { return curBpm; }
    int getAdvChop() const { return (selChop() + advOffset()) % numChops(); }
    int getAdvPos() const { return adv.listPos; }
    int advOffset() const { return params[kAdvOn] > 0.5f ? adv.offsetFor(selChop(), numChops()) : 0; }
    float getAdvProgress() const { return adv.progress; }

    void process(float* outL, float* outR, uint32_t frames, const MidiMsg* midi, uint32_t midiCount, const TimeInfo& ti)
    {
        std::memset(outL, 0, sizeof(float) * frames);
        std::memset(outR, 0, sizeof(float) * frames);

        const int mode = (int)params[kMode];
        if (mode != lastMode) {
            releaseAll(); heldCount = 0; seqRunning = false; playStep = -1; lastMode = mode;
            if (auditionOn) { auditionOn = false; auditionNote = -1; } // re-armed below
        }
        updateAudition(mode);

        const double bpm = (ti.valid && ti.bpm > 1.0) ? ti.bpm : 120.0;
        const double beatsPerSample = bpm / 60.0 / sampleRate;
        updateBlockParams(bpm);

        uint32_t mi = 0;
        float peak = 0.0f;
        const bool haveSample = sample && !sample->empty();
        const double delaySamples = kDelayBeats[std::min(kNumDelayTimes - 1, std::max(0, (int)params[kDelayTime]))] * 60.0 / bpm * sampleRate;
        const float dfb = params[kDelayFeedback], dmix = params[kDelayMix], rmix = params[kReverbMix];
        reverb.setSize(params[kReverbSize]);

        for (uint32_t i = 0; i < frames; ++i) {
            while (mi < midiCount && midi[mi].frame <= i) { handleMidi(midi[mi], mode); ++mi; }
            if ((i & 31) == 0) tickAdvance(ti, bpm, i, std::min<uint32_t>(32, frames - i));

            if (mode == kModeStep && seqRunning)
                runSequencer(ti.playing && ti.valid ? ti.ppq + (double)i * beatsPerSample : internalBeat);
            internalBeat += beatsPerSample;

            float dryL = 0, dryR = 0, sendL = 0, sendR = 0;
            if (haveSample) {
                for (auto& v : voices) {
                    if (!v.active) continue;
                    float l, r;
                    renderVoice(v, l, r);
                    dryL += l; dryR += r;
                    const float send = chopVal(v.chop, kCSpace);
                    sendL += l * send; sendR += r * send;
                }
            }
            float L, R;
            if (original) {
                // ORIGINAL SOUND: no delay / reverb, unity level at OUTPUT 0 dB
                smoothGain += (outGain / 2.2f - smoothGain) * 0.002f;
                L = dryL * smoothGain; R = dryR * smoothGain;
            } else {
                if (dmix > 0.0005f || dfb > 0.0f) delay.process(dryL, dryR, delaySamples, dfb, dmix);
                float wl, wr;
                reverb.process(sendL, sendR, wl, wr);
                smoothGain += (outGain - smoothGain) * 0.002f;
                L = (dryL + wl * rmix * 1.6f) * smoothGain;
                R = (dryR + wr * rmix * 1.6f) * smoothGain;
            }
            if (!std::isfinite(L)) L = 0;
            if (!std::isfinite(R)) R = 0;
            outL[i] = L; outR[i] = R;
            const float a = std::max(std::fabs(L), std::fabs(R));
            if (a > peak) peak = a;
        }
        while (mi < midiCount) { handleMidi(midi[mi], mode); ++mi; }

        const float target = std::min(1.0f, peak);
        activity = target > activity ? target : activity * 0.9f + target * 0.1f;
    }

private:
    struct Grain {
        bool on = false;
        double pos = 0, inc = 0;
        int age = 0, len = 1;
        float gl = 0, gr = 0;
    };
    struct Voice {
        bool active = false, released = false, fromStep = false;
        int note = 0, chop = 0, baseChop = 0;
        float transpose = 0, velocity = 1, posOffset = 0;
        double grainTimer = 0;
        float env = 0;
        int gateRemaining = -1;
        uint64_t order = 0;
        float s1[2] = { 0, 0 }, s2[2] = { 0, 0 };
        Grain grains[kMaxGrains];
        // ORIGINAL SOUND: plain playback head (+ an old head that fades out on jumps / loops)
        bool cOn = false, fOn = false; double cPos = 0, fPos = 0; int fLeft = 0; int cChop = -1;
    };

    double sampleRate = 44100.0;
    const SampleData* sample = nullptr;
    Voice voices[kMaxVoices];
    uint64_t voiceCounter = 0;
    Rng rng;
    Reverb reverb;
    Delay delay;
    AutoAdvance adv;

    float windows[kNumEnvShapes][kWinSize + 1];
    float custom[kCustomPoints];

    float outGain = 1.0f, smoothGain = 2.2f, attackInc = 1.0f, releaseCoef = 0.99f;
    float svfG = 1.0f, svfK = 1.0f; int filterType = 0;
    double spawnInterval[kMaxChops];
    float grainLen[kMaxChops], grainNorm[kMaxChops];
    int density[kMaxChops];
    bool sync = false, snap = true;
    float bend = 0.0f;
    double regionStart = 0, regionLen = 1;
    double chopS[kMaxChops], chopL[kMaxChops];
    bool original = false, lastOriginal = false; int origEnd = 0;
    static constexpr int kXfade = 256;

    int held[32]; int heldVel[32]; int heldCount = 0;
    bool seqRunning = false;
    int64_t lastStepIdx = INT64_MIN;
    double internalBeat = 0;
    int playStep = -1;
    int lastMode = -1;
    float activity = 0.0f;
    int lastChop = 0;
    int lastAdvOff = 0;
    double curBpm = 120.0;

    bool auditionOn = false;
    int auditionNote = -1;

    inline float chopVal(int chop, int which) const { return params[chopParam(chop, which)]; }
    int numChops() const { int n = (int)params[kChops]; return n < 1 ? 1 : (n > kMaxChops ? kMaxChops : n); }
    int selChop() const { return std::min(numChops() - 1, std::max(0, (int)params[kSelChop] - 1)); }

    // ------------------------------------------------------------------ auto-advance (runs every 32 samples)
    void tickAdvance(const TimeInfo& ti, double bpm, uint32_t at, uint32_t len) {
        bool sounding = auditionOn || heldCount > 0;
        for (auto& v : voices) if (v.active && !v.released) sounding = true;
        const int n = numChops();
        const bool advOn = params[kAdvOn] > 0.5f;
        const bool hostPlay = ti.playing && ti.valid;
        const double beat = ti.ppq + (double)at * bpm / 60.0 / sampleRate;
        const bool changed = adv.tick(advOn, (int)params[kAdvEvery], (int)params[kAdvOrder], n, hostPlay, beat, bpm,
                                      ti.beatsPerBar, sounding, len / sampleRate, &params[kAdvListFirst],
                                      std::max(1, std::min(kAdvListMax, (int)params[kAdvLen])), &params[kAdvHoldFirst]);
        const int off = advOffset();
        if (changed || off != lastAdvOff) {
            for (auto& v : voices) if (v.active && !v.released && !v.fromStep) { v.chop = (v.baseChop + off) % n; lastChop = v.chop; }
        }
        lastAdvOff = off;
        if (!advOn) for (auto& v : voices) if (v.active && !v.fromStep && v.chop != v.baseChop % n) v.chop = v.baseChop % n;
    }

    // ------------------------------------------------------------------ audition (Play button)
    int auditionTarget(int mode) const { return mode == kModeHold ? 60 + selChop() : 60; }
    void updateAudition(int mode) {
        const bool want = params[kAudition] > 0.5f;
        if (want) {
            const int note = auditionTarget(mode);
            if (!auditionOn || note != auditionNote) {
                if (auditionOn) noteOff(auditionNote, mode);
                noteOn(note, 0.8f, mode);
                auditionOn = true; auditionNote = note;
            } else if (mode == kModeKeys) {
                // follow chop selection in KEYS mode
                for (auto& v : voices)
                    if (v.active && !v.released && !v.fromStep && v.note == auditionNote && v.baseChop != selChop()) {
                        noteOff(auditionNote, mode); noteOn(auditionNote, 0.8f, mode); break;
                    }
            }
        } else if (auditionOn) {
            noteOff(auditionNote, mode);
            auditionOn = false; auditionNote = -1;
        }
    }

    // ------------------------------------------------------------------ windows
    void buildWindows() {
        for (int i = 0; i <= kWinSize; ++i) {
            const float x = (float)i / kWinSize;
            const float edge = 0.015f, tr = 0.2f;
            windows[kEnvSquare][i]    = x < edge ? x / edge : (x > 1 - edge ? (1 - x) / edge : 1.0f);
            windows[kEnvTrapezoid][i] = x < tr ? x / tr : (x > 1 - tr ? (1 - x) / tr : 1.0f);
            windows[kEnvHann][i]      = 0.5f - 0.5f * std::cos(2.0f * kPi * x);
            const float g = (x - 0.5f) / 0.16f;
            const float e0 = std::exp(-0.5f * (0.5f / 0.16f) * (0.5f / 0.16f));
            windows[kEnvGaussian][i]  = std::max(0.0f, (std::exp(-0.5f * g * g) - e0) / (1.0f - e0));
            windows[kEnvTriangle][i]  = 1.0f - std::fabs(2.0f * x - 1.0f);
        }
        buildCustomWindow();
    }
    void buildCustomWindow() {
        for (int i = 0; i <= kWinSize; ++i) {
            const float x = (float)i / kWinSize * (kCustomPoints - 1);
            const int a = std::min((int)x, kCustomPoints - 2);
            const float f = x - a;
            float v = custom[a] * (1 - f) + custom[a + 1] * f;
            const float xn = (float)i / kWinSize, edge = 0.01f;
            if (xn < edge) v *= xn / edge; else if (xn > 1 - edge) v *= (1 - xn) / edge;
            windows[kEnvCustom][i] = v;
        }
    }

    // ------------------------------------------------------------------ params
    void updateBlockParams(double bpm) {
        curBpm = bpm;
        outGain = 2.2f * std::pow(10.0f, params[kOutput] / 20.0f);
        attackInc = 1.0f / (float)(std::max(0.001f, params[kAttack]) * sampleRate);
        releaseCoef = (float)std::exp(std::log(0.0005) / (std::max(0.01f, params[kRelease]) * sampleRate));
        sync = params[kSync] > 0.5f;
        snap = params[kSnap] > 0.5f;
        filterType = (int)params[kFilterTypeP];
        const float fc = std::min((float)(sampleRate * 0.45), std::max(20.0f, params[kCutoff]));
        const float g = std::tan(kPi * fc / (float)sampleRate);
        svfG += (g - svfG) * 0.5f;
        svfK = 2.0f - 1.96f * std::min(1.0f, std::max(0.0f, params[kResonance]));

        if (sample && !sample->empty()) {
            const double n = (double)sample->frames();
            double rs = std::min(1.0f, std::max(0.0f, params[kRegionStart]));
            double re = std::min(1.0f, std::max(0.0f, params[kRegionEnd]));
            if (re < rs) std::swap(rs, re);
            if (re - rs < 0.002) re = std::min(1.0, rs + 0.002);
            regionStart = rs * n; regionLen = std::max(64.0, (re - rs) * n);
        }
        for (int c = 0; c < kMaxChops; ++c) {
            double a, b; chopBounds(params, c, a, b);
            chopS[c] = regionStart + a * regionLen; chopL[c] = std::max(32.0, (b - a) * regionLen);
        }
        original = params[kOriginal] > 0.5f;
        if (original != lastOriginal) { smoothGain *= original ? 1.0f / 2.2f : 2.2f; lastOriginal = original; }
        origEnd = std::min(2, std::max(0, (int)params[kOrigEnd]));

        for (int c = 0; c < kMaxChops; ++c) {
            const float lenSec = std::max(0.005f, chopVal(c, kCSize) * 0.001f);
            double interval;
            if (sync) interval = kSyncDivBeats[rateToSyncDiv(chopVal(c, kCRate))] * 60.0 / bpm * sampleRate;
            else      interval = sampleRate / rateToHz(chopVal(c, kCRate));
            spawnInterval[c] = std::max(16.0, interval);
            grainLen[c] = lenSec * (float)sampleRate;
            density[c] = std::min(8, std::max(1, (int)std::lround(chopVal(c, kCDensity))));
            const float overlap = grainLen[c] / (float)spawnInterval[c] * (float)density[c];
            grainNorm[c] = 1.0f / std::sqrt(std::max(1.0f, overlap));
        }
    }

    // ------------------------------------------------------------------ voices
    Voice* allocVoice() {
        Voice* best = nullptr;
        for (auto& v : voices) if (!v.active) { best = &v; break; }
        if (!best) {
            for (auto& v : voices) if (v.released && (!best || v.order < best->order)) best = &v;
            if (!best) for (auto& v : voices) if (!best || v.order < best->order) best = &v;
        }
        return best;
    }

    void startVoice(int note, int chop, float transpose, float vel, bool fromStep, int gate, float posOffset = 0.0f) {
        Voice* v = allocVoice();
        if (!v) return;
        v->baseChop = chop;
        if (!fromStep && params[kAdvOn] > 0.5f) chop = (chop + advOffset()) % numChops();
        lastChop = chop;
        v->active = true; v->released = false; v->fromStep = fromStep;
        v->note = note; v->chop = chop; v->transpose = transpose; v->velocity = vel; v->posOffset = posOffset;
        v->grainTimer = 0; v->env = 0; v->gateRemaining = gate; v->order = ++voiceCounter;
        v->s1[0] = v->s1[1] = v->s2[0] = v->s2[1] = 0;
        for (auto& g : v->grains) g.on = false;
        v->cOn = false; v->fOn = false; v->cChop = -1; v->fLeft = 0;
    }

    void releaseNote(int note) { for (auto& v : voices) if (v.active && !v.fromStep && v.note == note) v.released = true; }
    void releaseStepVoices()   { for (auto& v : voices) if (v.active && v.fromStep) v.released = true; }
    void releaseAll()          { for (auto& v : voices) if (v.active) v.released = true; }

    void handleMidi(const MidiMsg& m, int mode) {
        const uint8_t st = m.status & 0xF0;
        if (st == 0x90 && m.d2 > 0) noteOn(m.d1, m.d2 / 127.0f, mode);
        else if (st == 0x80 || (st == 0x90 && m.d2 == 0)) noteOff(m.d1, mode);
        else if (st == 0xE0) { const int v = ((int)m.d2 << 7) | m.d1; bend = (v - 8192) / 8192.0f * 2.0f; }
        else if (st == 0xB0 && (m.d1 == 123 || m.d1 == 120)) { releaseAll(); heldCount = 0; seqRunning = false; playStep = -1; auditionOn = false; }
    }

    void noteOn(int note, float vel, int mode) {
        const int n = numChops();
        if (mode == kModeHold) {
            startVoice(note, ((note - 60) % n + n) % n, 0.0f, vel, false, -1);
        } else if (mode == kModeKeys) {
            startVoice(note, selChop(), (float)(note - 60), vel, false, -1);
        } else {
            if (heldCount < 32) { held[heldCount] = note; heldVel[heldCount] = (int)(vel * 127); ++heldCount; }
            if (!seqRunning) { seqRunning = true; lastStepIdx = INT64_MIN; internalBeat = 0; }
        }
    }

    void noteOff(int note, int mode) {
        if (mode == kModeStep) {
            for (int i = 0; i < heldCount; ++i) if (held[i] == note) {
                for (int j = i; j < heldCount - 1; ++j) { held[j] = held[j + 1]; heldVel[j] = heldVel[j + 1]; }
                --heldCount; break;
            }
            if (heldCount == 0) { seqRunning = false; releaseStepVoices(); playStep = -1; }
        } else {
            releaseNote(note);
        }
    }

    void runSequencer(double beat) {
        const int rate = std::min(kNumStepRates - 1, std::max(0, (int)params[kStepRate]));
        const double stepBeats = kStepRateBeats[rate];
        const double sp = beat / stepBeats + 1e-7;
        const int64_t idx = (int64_t)std::floor(sp);
        const double frac = sp - (double)idx;
        if (idx == lastStepIdx) return;
        const float swing = std::min(0.75f, std::max(0.0f, params[kStepSwing]));
        if ((idx & 1) && frac < swing * 0.5 && lastStepIdx != INT64_MIN) return; // swung off-beat waits
        lastStepIdx = idx;
        const int count = std::min(kMaxSteps, std::max(1, (int)params[kStepCount]));
        const int s = (int)(((idx % count) + count) % count);
        playStep = s;
        releaseStepVoices();
        const int slot = (int)params[kStepFirst + s];
        if (slot <= 0 || heldCount == 0) return;
        const int chop = (slot - 1 + advOffset()) % numChops();
        const int note = held[heldCount - 1];
        const float vel = heldVel[heldCount - 1] / 127.0f;
        double len = stepBeats * (1.0 - ((idx & 1) ? swing * 0.5 : 0.0));
        const int gate = (int)(std::min(1.0f, std::max(0.05f, params[kStepGate])) * len * 60.0 / curBpm * sampleRate);
        startVoice(note, chop, (float)(note - 60) + params[kStepPitchFirst + s], vel, true, std::max(32, gate), params[kStepPosFirst + s]);
    }

    inline float readHermite(const std::vector<float>& d, double pos) const {
        const int64_t n = (int64_t)d.size();
        const int64_t i = (int64_t)std::floor(pos);
        const float f = (float)(pos - (double)i);
        auto at = [&](int64_t k) { return (k < 0 || k >= n) ? 0.0f : d[(size_t)k]; };
        const float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float b = w + a;
        return ((((a * f) - b) * f + c) * f + x0);
    }

    void spawnGrain(Voice& v, int stream, int streams) {
        Grain* g = nullptr;
        for (auto& gg : v.grains) if (!gg.on) { g = &gg; break; }
        if (!g) return;

        const int n = numChops();
        const int chop = std::min(v.chop, n - 1);
        const double frames = (double)sample->frames();
        const double chopLen = chopL[chop];
        const double chopStart = chopS[chop];
        const float chaos = chopVal(v.chop, kCChaos);
        const float space = chopVal(v.chop, kCSpace);
        const float spread = streams > 1 ? ((float)stream / (float)(streams - 1) - 0.5f) : 0.0f;

        double pos = chopVal(v.chop, kCPosition) + v.posOffset;
        pos = pos - std::floor(pos); // wrap inside chop
        double start = chopStart + pos * chopLen;
        start += spread * 0.08 * chopLen * (0.5 + chaos);           // density streams fan out
        if (chaos > 0.0f) start += rng.bi() * chaos * 0.5 * chopLen;
        start = std::min(frames - 2.0, std::max(0.0, start));

        float semis = chopVal(v.chop, kCPitch);
        if (snap) semis = std::round(semis);
        semis += v.transpose + bend;
        if (streams > 1) semis += spread * 0.12f;                    // gentle chorus between streams
        if (chaos > 0.0f) {
            float j = rng.bi() * chaos * (snap ? 2.0f : 0.35f);
            if (snap) j = std::round(j * (rng.u() < chaos ? 1.0f : 0.0f));
            semis += j;
            if (rng.u() < chaos * 0.2f) semis += rng.u() < 0.5f ? 12.0f : -12.0f;
        }
        double inc = sample->rate / sampleRate * std::pow(2.0, semis / 12.0);
        if (chaos > 0.0f && rng.u() < chaos * 0.25f) inc = -inc;

        float len = grainLen[v.chop];
        if (chaos > 0.0f) len *= 1.0f + chaos * 0.4f * rng.bi();

        float pan = space * (streams > 1 ? spread * 1.4f + rng.bi() * 0.3f : rng.bi());
        pan = std::max(-1.0f, std::min(1.0f, pan));
        const float amp = grainNorm[v.chop] * (1.0f - (chaos > 0.0f ? chaos * 0.4f * rng.u() : 0.0f)) * 1.41421356f;
        const float ang = (pan + 1.0f) * kPi * 0.25f;

        g->on = true; g->pos = start; g->inc = inc; g->age = 0; g->len = std::max(8, (int)len);
        g->gl = std::cos(ang) * amp; g->gr = std::sin(ang) * amp;
    }

    // ORIGINAL SOUND: the chop played straight from its start, at its own speed; a short crossfade on every jump
    void jumpHead(Voice& v, double to, bool silent) {
        if (v.cOn) { v.fOn = true; v.fPos = v.cPos; v.fLeft = kXfade; }
        v.cOn = !silent; v.cPos = to;
    }
    void renderOriginal(Voice& v, float& outL, float& outR) {
        const int n = numChops();
        const int chop = std::min(v.chop, n - 1);
        if (chop != v.cChop) {   // new chop (start or auto-advance): start at its beginning
            double off = v.posOffset; off -= std::floor(off);
            jumpHead(v, chopS[chop] + (v.fromStep ? off * chopL[chop] : 0.0), false);
            if (v.cChop < 0) { v.fOn = false; v.fLeft = 0; }
            v.cChop = chop;
        }
        const double frames = (double)sample->frames();
        const double inc = sample->rate / sampleRate * std::pow(2.0, (v.transpose + bend) / 12.0);
        const double end = chopS[chop] + chopL[chop];
        if (v.cOn) {
            const bool past = origEnd == 1 ? v.cPos >= frames - 2 : v.cPos >= end;
            if (past) {
                if (origEnd == 0) jumpHead(v, chopS[chop], false);   // LOOP
                else jumpHead(v, v.cPos, true);                      // STOP (or end of the file)
            }
        }
        const std::vector<float>& dL = sample->L;
        const std::vector<float>& dR = sample->R;
        float l = 0, r = 0;
        const float fin = v.fOn ? 1.0f - (float)v.fLeft / kXfade : 1.0f;
        if (v.cOn) { l += readHermite(dL, v.cPos) * fin; r += readHermite(dR, v.cPos) * fin; v.cPos += inc; }
        if (v.fOn) {
            const float fo = (float)v.fLeft / kXfade;
            l += readHermite(dL, v.fPos) * fo; r += readHermite(dR, v.fPos) * fo;
            v.fPos += inc;
            if (--v.fLeft <= 0) v.fOn = false;
        }
        const float g = v.env * std::min(1.0f, 0.2f + v.velocity);
        outL = l * g; outR = r * g;
    }

    void renderVoice(Voice& v, float& outL, float& outR) {
        if (v.gateRemaining > 0 && --v.gateRemaining == 0) v.released = true;
        if (original) {
            // fixed, inaudible fades: the sample itself is heard, not an envelope
            if (v.released) { v.env -= 1.0f / (0.025f * (float)sampleRate); if (v.env <= 0.0f) { v.active = false; outL = outR = 0; return; } }
            else if (v.env < 1.0f) v.env = std::min(1.0f, v.env + 1.0f / (0.002f * (float)sampleRate));
            renderOriginal(v, outL, outR);
            return;
        }
        if (v.released) {
            v.env *= releaseCoef;
            if (v.env < 0.0005f) { v.active = false; outL = outR = 0; return; }
        } else if (v.env < 1.0f) {
            v.env = std::min(1.0f, v.env + attackInc);
        }

        v.grainTimer -= 1.0;
        if (v.grainTimer <= 0.0) {
            const int d = density[v.chop];
            for (int k = 0; k < d; ++k) spawnGrain(v, k, d);
            double iv = spawnInterval[v.chop];
            const float chaos = chopVal(v.chop, kCChaos);
            if (!sync && chaos > 0.0f) iv *= 1.0 + chaos * 0.6 * rng.bi();
            v.grainTimer += std::max(16.0, iv);
        }

        const int shape = std::min(kNumEnvShapes - 1, std::max(0, (int)params[kEnvShapeP]));
        const float* win = windows[shape];
        const std::vector<float>& dL = sample->L;
        const std::vector<float>& dR = sample->R;
        float l = 0, r = 0;
        for (auto& g : v.grains) {
            if (!g.on) continue;
            const float ph = (float)g.age / (float)g.len * kWinSize;
            const int wi = std::min(kWinSize - 1, (int)ph);
            const float wf = ph - wi;
            const float w = win[wi] * (1 - wf) + win[wi + 1] * wf;
            l += readHermite(dL, g.pos) * w * g.gl;
            r += readHermite(dR, g.pos) * w * g.gr;
            g.pos += g.inc;
            if (++g.age >= g.len) g.on = false;
        }

        if (filterType != kFilterOff) {
            const float gg = svfG, k = svfK;
            const float a1 = 1.0f / (1.0f + gg * (gg + k)), a2 = gg * a1, a3 = gg * a2;
            float in[2] = { l, r }, out[2];
            for (int ch = 0; ch < 2; ++ch) {
                const float v3 = in[ch] - v.s2[ch];
                const float v1 = a1 * v.s1[ch] + a2 * v3;
                const float v2 = v.s2[ch] + a2 * v.s1[ch] + a3 * v3;
                v.s1[ch] = 2 * v1 - v.s1[ch];
                v.s2[ch] = 2 * v2 - v.s2[ch];
                if (filterType == kFilterLP) out[ch] = v2;
                else if (filterType == kFilterBP) out[ch] = v1 * k;
                else out[ch] = in[ch] - k * v1 - v2;
            }
            l = out[0]; r = out[1];
        }

        const float gain = v.env * (0.35f + 0.65f * v.velocity);
        outL = l * gain; outR = r * gain;
    }
};

// ------------------------------------------------------------------------------------------------
// Offline render: what the Play button plays, for `beats` beats at `bpm`, plus the release tail.
// Used by the UI for "drag to DAW".

inline void renderOffline(const float* params, const float* customEnv, const SampleData& sample,
                          double sampleRate, double bpm, double beats,
                          std::vector<float>& outL, std::vector<float>& outR)
{
    GrainEngine e;
    for (uint32_t i = 0; i < kOutPlayStep; ++i) e.params[i] = params[i];
    e.params[kAudition] = 1.0f;
    e.setCustomEnvelope(customEnv);
    e.setSampleRate(sampleRate);
    e.setSample(&sample);
    TimeInfo ti; ti.valid = true; ti.playing = true; ti.bpm = bpm; ti.ppq = 0.0;
    const uint32_t block = 256;
    const size_t bodyFrames = (size_t)(beats * 60.0 / bpm * sampleRate);
    const double tail = params[kOriginal] > 0.5f ? 0.05
                      : std::min(6.0, (double)params[kRelease] + 0.6 + params[kReverbMix] * params[kReverbSize] * 2.5 + params[kDelayMix] * params[kDelayFeedback] * 3.0);
    const size_t total = bodyFrames + (size_t)(tail * sampleRate);
    outL.assign(total, 0.0f); outR.assign(total, 0.0f);
    size_t pos = 0;
    while (pos < total) {
        const uint32_t n = (uint32_t)std::min<size_t>(block, total - pos);
        if (pos >= bodyFrames) e.params[kAudition] = 0.0f; // release
        e.process(&outL[pos], &outR[pos], n, nullptr, 0, ti);
        ti.ppq += n * bpm / 60.0 / sampleRate;
        pos += n;
    }
}

// ------------------------------------------------------------------------------------------------
// Built-in source: eight one-second chords (soft bell / music-box tones with a breath of noise).

inline void makeDefaultSample(SampleData& s, double rate = 44100.0)
{
    s.rate = rate;
    const int segs = 8;
    const int segLen = (int)rate;
    s.L.assign((size_t)(segs * segLen), 0.0f);
    s.R.assign((size_t)(segs * segLen), 0.0f);
    static const int chords[8][4] = {
        { 57, 60, 64, 69 }, { 53, 57, 60, 65 }, { 48, 55, 60, 64 }, { 55, 59, 62, 67 },
        { 50, 57, 62, 65 }, { 52, 55, 59, 64 }, { 53, 60, 65, 69 }, { 52, 56, 59, 64 } };
    Rng rng; rng.s = 12345u;
    float lp = 0;
    for (int c = 0; c < segs; ++c) {
        for (int k = 0; k < 4; ++k) {
            const double f0 = 440.0 * std::pow(2.0, (chords[c][k] - 69) / 12.0);
            const float pan = (k - 1.5f) / 2.0f;
            const double delay = k * 0.045 * rate;
            for (int i = 0; i < segLen; ++i) {
                const double t = (i - delay) / rate;
                if (t < 0) continue;
                const double env = (1.0 - std::exp(-t * 300.0)) * std::exp(-t * 2.6) * 0.6 + 0.4 * std::exp(-t * 0.9) * (1.0 - std::exp(-t * 12.0));
                double v = 0;
                for (int h = 1; h <= 6; ++h) {
                    const double hd = std::exp(-t * (1.2 + h * 1.5));
                    v += std::sin(2.0 * kPi * f0 * h * (1.0 + 0.0007 * h * h) * t) * hd / std::pow((double)h, 1.4);
                }
                v *= env * 0.16;
                const size_t idx = (size_t)(c * segLen + i);
                s.L[idx] += (float)(v * (1.0f - pan) * 0.8f);
                s.R[idx] += (float)(v * (1.0f + pan) * 0.8f);
            }
        }
        for (int i = 0; i < segLen; ++i) {
            lp += (rng.bi() - lp) * 0.05f;
            const double t = (double)i / rate;
            const float n = lp * 0.03f * (float)(0.5 + 0.5 * std::sin(kPi * t));
            const size_t idx = (size_t)(c * segLen + i);
            s.L[idx] += n; s.R[idx] += n * 0.9f;
        }
    }
}

} // namespace gf
