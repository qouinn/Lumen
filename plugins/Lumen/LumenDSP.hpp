// Lumen — sample player + 9-module effect chain
//   player -> GRAIN -> PITCH -> DRIVE -> FILTER -> LO-FI -> MOD -> STUTTER -> DELAY -> REVERB -> dry/wet -> out
#pragma once
#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

#include "LumenPresetTypes.hpp"
#include "GrainEngine.hpp"   // gf::SampleData, gf::Rng, gf::MidiMsg, gf::TimeInfo

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace lm {

using gf::SampleData;
using gf::MidiMsg;
using gf::TimeInfo;

constexpr float kPi = 3.14159265358979f;

struct Rand {
    uint32_t s = 0x2545F491u;
    inline float u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float)(s >> 8) * (1.0f / 16777216.0f); }
    inline float bi() { return u() * 2.0f - 1.0f; }
};
inline uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
inline float hashU(int64_t k, uint32_t salt) { return (float)(hash32((uint32_t)k * 2654435761U ^ salt) >> 8) * (1.0f / 16777216.0f); }

// ---------------------------------------------------------------------------------------------
// building blocks
struct Ring {   // power-of-two ring buffer with fractional reads
    std::vector<float> b; uint32_t mask = 0, w = 0;
    void init(size_t minLen) { size_t n = 1; while (n < minLen) n <<= 1; b.assign(n, 0.0f); mask = (uint32_t)n - 1; w = 0; }
    void clear() { std::fill(b.begin(), b.end(), 0.0f); }
    inline void push(float x) { b[w] = x; w = (w + 1) & mask; }
    inline float at(int64_t back) const { return b[(uint32_t)((int64_t)w - 1 - back) & mask]; }      // back = 0 -> newest
    inline float frac(double back) const {   // linear interpolation, back >= 0
        const int64_t i = (int64_t)back; const float f = (float)(back - (double)i);
        return at(i) * (1 - f) + at(i + 1) * f;
    }
    inline float abs(double pos) const {     // absolute index (pos in the same counting as w), linear
        const int64_t i = (int64_t)std::floor(pos); const float f = (float)(pos - (double)i);
        return b[(uint32_t)i & mask] * (1 - f) + b[(uint32_t)(i + 1) & mask] * f;
    }
};

struct SVF {   // TPT state-variable filter
    float s1 = 0, s2 = 0;
    void reset() { s1 = s2 = 0; }
    inline void run(float x, float g, float k, float& lp, float& bp, float& hp) {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2 * v1 - s1; s2 = 2 * v2 - s2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

struct OnePole { float z = 0; inline float lp(float x, float a) { z += (x - z) * a; return z; } };
inline float onePoleCoef(float hz, float sr) { return 1.0f - std::exp(-2.0f * kPi * std::min(hz, sr * 0.49f) / sr); }

// dual-tap delay-line pitch shifter
struct Shifter {
    Ring r; float ph = 0;
    void init(float sr) { r.init((size_t)(sr * 0.2f)); ph = 0; }
    void clear() { r.clear(); ph = 0; }
    inline float process(float x, float ratio, float win) {
        r.push(x);
        ph += (1.0f - ratio) / win; ph -= std::floor(ph);
        float p2 = ph + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
        const float a = r.frac(1.0 + ph * win), b = r.frac(1.0 + p2 * win);
        return a * std::sin(kPi * ph) + b * std::sin(kPi * p2);
    }
};

// ---------------------------------------------------------------------------------------------
// 1 GRAIN: a granular cloud made from the last few seconds of the signal (or a frozen moment)
struct GrainCloud {
    struct G { bool on = false; double pos = 0, inc = 1; int age = 0, len = 1; float gl = 0, gr = 0; };
    Ring bl, br; G g[64]; float win[1025]; double timer = 0, filled = 0; float sr = 44100; Rand rng;
    void init(float s) { sr = s; bl.init((size_t)(s * 5)); br.init((size_t)(s * 5)); for (int i = 0; i <= 1024; ++i) win[i] = 0.5f - 0.5f * std::cos(2 * kPi * i / 1024.0f); reset(); }
    void reset() { bl.clear(); br.clear(); for (auto& x : g) x.on = false; timer = 0; filled = 0; }
    inline void process(float& L, float& R, const float* q, float act = 1.0f) {
        // FREEZE holds the buffer, but only once it holds sound (so a frozen preset still captures the start)
        const double need = (q[kGSpread] * 2.0 + q[kGSize] * 0.001 * 2.0) * sr + 0.5 * sr;
        const bool freeze = q[kGFreeze] > 0.5f && filled >= need;
        if (!freeze) { bl.push(L); br.push(R); if (std::fabs(L) + std::fabs(R) > 1e-5f || filled > 0) filled += 1; }
        const float mix = q[kGMix], jit = q[kGJit], spread = q[kGSpread];
        const float lenS = q[kGSize] * 0.001f, dens = q[kGDens];
        timer -= 1.0;
        if (timer <= 0) {
            for (auto& x : g) {
                if (x.on) continue;
                const float semis = q[kGPitch] + jit * 2.0f * rng.bi() + (rng.u() < jit * 0.15f ? (rng.u() < 0.5f ? 12.0f : -12.0f) : 0.0f);
                const double ratio = std::pow(2.0, semis / 12.0);
                const int len = std::max(32, (int)(lenS * sr * (1.0f + 0.35f * jit * rng.bi())));
                const bool rev = rng.u() < q[kGRev];
                const double scatter = rng.u() * spread * 2.0 * sr + 64;
                double back;   // how far behind the write head the grain starts
                if (rev) back = scatter;
                else back = scatter + len * (freeze ? ratio : std::max(0.0, ratio - 1.0));
                back = std::min(back, (double)bl.mask - len * 2.0 - 8);
                x.on = true; x.age = 0; x.len = len;
                x.pos = (double)bl.w - back; x.inc = rev ? -ratio : ratio;
                const float pan = std::max(-1.0f, std::min(1.0f, rng.bi() * (0.25f + spread * 0.75f)));
                const float ang = (pan + 1) * kPi * 0.25f;
                x.gl = std::cos(ang) * 1.4142f; x.gr = std::sin(ang) * 1.4142f;
                break;
            }
            timer += std::max(8.0, sr / dens * (1.0 + 0.6 * jit * rng.bi()));
        }
        float gl = 0, gr = 0;
        for (auto& x : g) {
            if (!x.on) continue;
            const float ph = (float)x.age / (float)x.len * 1024.0f;
            const int wi = std::min(1023, (int)ph); const float wf = ph - wi;
            const float w = win[wi] * (1 - wf) + win[wi + 1] * wf;
            gl += bl.abs(x.pos) * w * x.gl; gr += br.abs(x.pos) * w * x.gr;
            x.pos += x.inc;
            if (++x.age >= x.len) x.on = false;
        }
        const float overlap = std::max(1.0f, dens * lenS);
        const float norm = (freeze ? act : 1.0f) / std::sqrt(overlap);   // a frozen hold fades out once the sample stops
        L = L * (1 - mix) + gl * norm * mix;
        R = R * (1 - mix) + gr * norm * mix;
    }
};

// 2 PITCH: two shifted voices + dry
struct PitchMod {
    Shifter aL, aR, bL, bR; float sr = 44100;
    void init(float s) { sr = s; aL.init(s); aR.init(s); bL.init(s); bR.init(s); }
    void reset() { aL.clear(); aR.clear(); bL.clear(); bR.clear(); }
    inline void process(float& L, float& R, const float* q) {
        const float win = 0.055f * sr;
        const float ra = std::pow(2.0f, q[kPA] / 12.0f), rb = std::pow(2.0f, q[kPB] / 12.0f);
        float oL = L * q[kPDry], oR = R * q[kPDry];
        if (q[kPMixA] > 0.0005f) { oL += aL.process(L, ra, win) * q[kPMixA]; oR += aR.process(R, ra, win) * q[kPMixA]; }
        if (q[kPMixB] > 0.0005f) { oL += bL.process(L, rb, win) * q[kPMixB]; oR += bR.process(R, rb, win) * q[kPMixB]; }
        L = oL; R = oR;
    }
};

// 3 DRIVE with automatic level compensation
struct Drive {
    OnePole tl, tr; float dcxL = 0, dcyL = 0, dcxR = 0, dcyR = 0, envIn = 0, envOut = 0, comp = 1, sr = 44100;
    void init(float s) { sr = s; reset(); }
    void reset() { tl.z = tr.z = 0; dcxL = dcyL = dcxR = dcyR = 0; envIn = envOut = 0; comp = 1; }
    static inline float shape(float x, int mode) {
        switch (mode) {
        case 1: { const float a = std::fabs(x); return a < 0.8f ? x : (x > 0 ? 1 : -1) * (0.8f + 0.2f * std::tanh((a - 0.8f) * 5.0f)); }
        case 2: return std::sin(x * 1.3f);
        case 3: { const float y = x > 0 ? 1.0f - std::exp(-x * 1.4f) : -(1.0f - std::exp(x * 2.2f)) * 0.75f; return y; }
        case 4: { const float y = x / (1.0f + std::fabs(x)); return y + 0.18f * y * y; }
        default: return std::tanh(x);
        }
    }
    inline void process(float& L, float& R, const float* q) {
        const int mode = (int)q[kDMode];
        const float amt = q[kDAmt], pre = 1.0f + amt * amt * 40.0f;
        const float a = onePoleCoef(500.0f * std::pow(2.0f, q[kDTone] * 5.4f), sr);
        float yl = tl.lp(shape(L * pre, mode), a), yr = tr.lp(shape(R * pre, mode), a);
        // dc blocker (fuzz / tube are asymmetric)
        const float dl = yl - dcxL + 0.995f * dcyL; dcxL = yl; dcyL = dl;
        const float dr = yr - dcxR + 0.995f * dcyR; dcxR = yr; dcyR = dr;
        envIn += (L * L + R * R - envIn) * 0.0005f;
        envOut += (dl * dl + dr * dr - envOut) * 0.0005f;
        const float target = envOut > 1e-9f ? std::min(3.0f, std::max(0.12f, std::sqrt(envIn / envOut))) : 1.0f;
        comp += (target - comp) * 0.0004f;
        const float m = q[kDMix];
        L = L * (1 - m) + dl * comp * m; R = R * (1 - m) + dr * comp * m;
    }
};

// 4 FILTER with LFO and envelope follower
struct Filter {
    SVF l, r; float lfo = 0, env = 0, sr = 44100;
    void init(float s) { sr = s; reset(); }
    void reset() { l.reset(); r.reset(); env = 0; }
    inline void process(float& L, float& R, const float* q) {
        lfo += q[kFRate] / sr; if (lfo >= 1) lfo -= 1;
        const float a = std::fabs(L) + std::fabs(R);
        env += (a - env) * (a > env ? 0.004f : 0.0004f);
        const float oct = std::sin(2 * kPi * lfo) * q[kFDepth] * 4.0f + q[kFEnv] * std::min(1.0f, env * 2.0f) * 5.0f;
        const float fc = std::min(sr * 0.45f, std::max(20.0f, q[kFCut] * std::pow(2.0f, oct)));
        const float g = std::tan(kPi * fc / sr), k = 2.0f - 1.94f * q[kFRes];
        float lp, bp, hp, lp2, bp2, hp2;
        l.run(L, g, k, lp, bp, hp); r.run(R, g, k, lp2, bp2, hp2);
        switch ((int)q[kFType]) {
        case 1: L = hp; R = hp2; break;
        case 2: L = bp * k; R = bp2 * k; break;
        case 3: L = lp + hp; R = lp2 + hp2; break;
        default: L = lp; R = lp2; break;
        }
    }
};

// 5 LO-FI: bits, rate, wow/flutter, hiss, crackle, age
struct LoFi {
    Ring dl, dr; float ph = 0, holdL = 0, holdR = 0, wow = 0, flut = 0, drift = 0, driftV = 0, sr = 44100;
    OnePole nl, nr, hpl, hpr, lpl, lpr, cr; float crackEnv = 0, crackSign = 1; Rand rng;
    void init(float s) { sr = s; dl.init((size_t)(s * 0.05f)); dr.init((size_t)(s * 0.05f)); reset(); }
    void reset() { dl.clear(); dr.clear(); ph = 0; holdL = holdR = 0; nl.z = nr.z = hpl.z = hpr.z = lpl.z = lpr.z = cr.z = 0; crackEnv = 0; }
    inline void process(float& L, float& R, const float* q, float act = 1.0f) {
        // wow & flutter (modulated short delay)
        wow += 0.55f / sr; if (wow >= 1) wow -= 1;
        flut += 7.3f / sr; if (flut >= 1) flut -= 1;
        driftV += (rng.bi() * 0.02f - drift * 0.001f) / sr * 60.0f; drift = std::max(-1.0f, std::min(1.0f, drift + driftV / sr * 30.0f));
        const float dms = 6.0f + q[kLWow] * 5.0f * (std::sin(2 * kPi * wow) + 0.4f * drift) + q[kLFlutter] * 0.35f * std::sin(2 * kPi * flut);
        dl.push(L); dr.push(R);
        if (q[kLWow] > 0.001f || q[kLFlutter] > 0.001f) { const double d = std::max(1.0f, dms * 0.001f * sr); L = dl.frac(d); R = dr.frac(d); }
        // sample-rate reduction + bit depth
        ph += q[kLRate] / sr;
        if (ph >= 1.0f || q[kLRate] >= sr * 0.98f) { ph -= std::floor(ph); holdL = L; holdR = R; }
        L = holdL; R = holdR;
        if (q[kLBits] < 15.9f) { const float s = std::pow(2.0f, q[kLBits] - 1.0f); L = std::round(L * s) / s; R = std::round(R * s) / s; }
        // age: narrow band + a little warmth
        const float age = q[kLAge];
        if (age > 0.001f) {
            const float h = onePoleCoef(20.0f + age * age * 600.0f, sr), lo = onePoleCoef(20000.0f * std::pow(0.12f, age), sr);
            L = lpl.lp(L - hpl.lp(L, h), lo); R = lpr.lp(R - hpr.lp(R, h), lo);
            L = std::tanh(L * (1 + age)) / (1 + age * 0.6f); R = std::tanh(R * (1 + age)) / (1 + age * 0.6f);
        }
        // hiss + crackle
        // hiss + crackle follow the sample: they fade out after it stops
        if (q[kLNoise] > 0.001f && act > 1e-4f) { const float n = q[kLNoise] * q[kLNoise] * 0.05f * act; L += nl.lp(rng.bi(), 0.35f) * n; R += nr.lp(rng.bi(), 0.35f) * n; }
        if (q[kLCrackle] > 0.001f) {
            if (rng.u() < q[kLCrackle] * 0.0006f * (44100.0f / sr)) { crackEnv = 0.2f + rng.u() * 0.6f; crackSign = rng.u() < 0.5f ? -1.0f : 1.0f; }
            const float c = cr.lp(crackEnv * crackSign * rng.u(), 0.6f) * q[kLCrackle] * 0.5f * act;
            crackEnv *= 0.6f;
            L += c; R += c * 0.9f;
        }
    }
};

// 6 MOD: chorus, flanger, phaser, tremolo, pan, ring, vibrato
struct Mod {
    Ring dl, dr; float lfo = 0, car = 0, fbl = 0, fbr = 0, zl[6] = { 0 }, zr[6] = { 0 }, sr = 44100;
    void init(float s) { sr = s; dl.init((size_t)(s * 0.06f)); dr.init((size_t)(s * 0.06f)); reset(); }
    void reset() { dl.clear(); dr.clear(); fbl = fbr = 0; for (int i = 0; i < 6; ++i) zl[i] = zr[i] = 0; }
    inline void process(float& L, float& R, const float* q) {
        const int type = (int)q[kMType];
        const float rate = q[kMRate], depth = q[kMDepth], fb = q[kMFb], mix = q[kMMix];
        lfo += rate / sr; if (lfo >= 1) lfo -= 1;
        const float s1 = std::sin(2 * kPi * lfo), s2 = std::sin(2 * kPi * (lfo + 0.25f));
        float wl = L, wr = R;
        switch (type) {
        case 0: {   // chorus: two voices per side
            dl.push(L); dr.push(R);
            const float base = 0.012f * sr, dep = depth * 0.008f * sr;
            const float s3 = std::sin(2 * kPi * (lfo + 0.5f)), s4 = std::sin(2 * kPi * (lfo + 0.75f));
            wl = 0.5f * (dl.frac(base + dep * s1) + dl.frac(base * 1.37f + dep * s3));
            wr = 0.5f * (dr.frac(base + dep * s2) + dr.frac(base * 1.37f + dep * s4));
            break; }
        case 1: {   // flanger
            const float base = 0.0006f * sr, dep = depth * 0.005f * sr;
            dl.push(L + fbl * fb); dr.push(R + fbr * fb);
            wl = dl.frac(base + dep * (0.5f + 0.5f * s1)); wr = dr.frac(base + dep * (0.5f + 0.5f * s2));
            fbl = wl; fbr = wr;
            break; }
        case 2: {   // phaser: 6 allpass stages
            const float f = 300.0f * std::pow(2.0f, (0.5f + 0.5f * s1) * depth * 4.5f), f2 = 300.0f * std::pow(2.0f, (0.5f + 0.5f * s2) * depth * 4.5f);
            const float t = std::tan(kPi * std::min(f, sr * 0.45f) / sr), c = (t - 1) / (t + 1);
            const float t2 = std::tan(kPi * std::min(f2, sr * 0.45f) / sr), c2 = (t2 - 1) / (t2 + 1);
            float x = L + fbl * fb, y = R + fbr * fb;
            for (int i = 0; i < 6; ++i) { const float o = c * x + zl[i]; zl[i] = x - c * o; x = o; const float o2 = c2 * y + zr[i]; zr[i] = y - c2 * o2; y = o2; }
            fbl = x; fbr = y; wl = x; wr = y;
            break; }
        case 3: { const float g = 1.0f - depth * (0.5f + 0.5f * s1); wl = L * g; wr = R * g; break; }
        case 4: { const float p = s1 * depth; wl = L * std::sqrt(0.5f * (1 - p)) * 1.4142f; wr = R * std::sqrt(0.5f * (1 + p)) * 1.4142f; break; }
        case 5: { car += rate * 100.0f / sr; if (car >= 1) car -= 1; const float c = std::sin(2 * kPi * car); wl = L * c; wr = R * c; break; }
        default: {  // vibrato
            dl.push(L); dr.push(R);
            const float base = 0.006f * sr, dep = depth * 0.005f * sr;
            wl = dl.frac(base + dep * s1); wr = dr.frac(base + dep * s1);
            break; }
        }
        L = L * (1 - mix) + wl * mix; R = R * (1 - mix) + wr * mix;
    }
};

// 7 STUTTER: tempo-synced repeat / gate / tape stop / reverse / half speed / stutter rolls
struct Stutter {
    Ring bl, br; float sr = 44100; int64_t lastChunk = INT64_MIN; double wStart = 0, repSrc = 0; bool active = false, prevActive = false;
    float a = 0, gate = 1; double total = 0;   // total = samples written (absolute counter)
    double rStart = 0, rLen = 1, rSrc = 0; int rN = 0;   // STUTTER: current retrigger (start within the chunk, length), source slice, count
    // STUTTER: length of the next retrigger in beats; pos = 0..1 through the chunk, n = retriggers so far
    static double rollBeats(int pat, double pos, int n, int64_t k) {
        switch (pat) {
        case 0: return 0.25; case 1: return 0.125; case 2: return 0.0625; case 3: return 1.0 / 6.0;
        case 4: return 0.25 * std::pow(0.125, std::min(1.0, pos));            // 1/16 speeding up to 1/128
        case 5: return 0.03125 * std::pow(8.0, std::min(1.0, pos));           // 1/128 slowing down to 1/16
        case 6: return 0.125;
        default: { static const double s[4] = { 0.25, 0.125, 0.0833333, 0.0625 }; return s[hashU(k * 131 + n, 0x77u) < 0.5f ? (n & 1) : 2 + (n & 1)]; }
        }
    }
    static constexpr uint16_t kPat[8] = { 0xFFFF, 0xAAAA, 0xEEEE, 0xB6B6, 0x9249, 0xF0F0, 0xDB6D, 0x8888 };
    void init(float s) { sr = s; bl.init((size_t)(s * 9)); br.init((size_t)(s * 9)); reset(); }
    void reset() { bl.clear(); br.clear(); lastChunk = INT64_MIN; active = prevActive = false; a = 0; gate = 1; total = 0; }
    inline void process(float& L, float& R, const float* q, double beat, double bpm) {
        bl.push(L); br.push(R); total += 1;
        const int mode = (int)q[kSMode];
        const double lb = kStutDivBeats[std::min(6, std::max(0, (int)q[kSDiv]))];
        const double ls = std::max(64.0, lb * 60.0 / bpm * sr);
        const int64_t k = (int64_t)std::floor(beat / lb + 1e-9);
        const double ts = (beat - (double)k * lb) * 60.0 / bpm * sr;
        if (k != lastChunk) {
            prevActive = active;
            lastChunk = k;
            wStart = total;
            const float amt = q[kSAmt];
            active = mode == 1 ? true : hashU(k, 0x51u + (uint32_t)mode) < amt;
            if (mode == 0 && active && !prevActive) repSrc = wStart - ls;   // start repeating the chunk that just played
            if (mode == 5) { rStart = 0; rN = 0; rLen = std::max(32.0, rollBeats((int)q[kSPat], 0, 0, k) * 60.0 / bpm * sr); rSrc = wStart - 1; }
        }
        float pl = L, pr = R;
        const double d = std::min(ts, ls);
        switch (mode) {
        case 0: if (active) { const double p = repSrc + std::fmod(d, ls); pl = bl.abs(p); pr = br.abs(p); } break;
        case 1: {
            const int step = (int)(((k % 16) + 16) % 16);
            const bool open = (kPat[std::min(7, std::max(0, (int)q[kSPat]))] >> (15 - step)) & 1;
            const float target = open ? 1.0f : 1.0f - q[kSAmt];
            gate += (target - gate) * (target > gate ? 0.02f : 0.006f);
            pl = L * gate; pr = R * gate;
            break; }
        case 2: if (active) { const double p = wStart - 1.0 + d - d * d / (2.0 * ls); pl = bl.abs(p) * (float)std::sqrt(std::max(0.0, 1.0 - d / ls)); pr = br.abs(p) * (float)std::sqrt(std::max(0.0, 1.0 - d / ls)); } break;
        case 3: if (active) { const double p = wStart - 1.0 - d; pl = bl.abs(p); pr = br.abs(p); } break;
        case 5: if (active) {
            // fast retriggers of the slice that starts the chunk; each one gets its own soft edges, so it ripples instead of clicking
            const int pat = std::min(7, std::max(0, (int)q[kSPat]));
            while (d - rStart >= rLen) { rStart += rLen; ++rN; rLen = std::max(32.0, rollBeats(pat, rStart / ls, rN, k) * 60.0 / bpm * sr); }
            const double t = d - rStart;
            static const int up[6] = { 0, 2, 4, 7, 9, 12 };                              // RIPPLE climbs a pentatonic scale
            const double sp = pat == 6 ? std::pow(2.0, up[rN % 6] / 12.0) : 1.0;
            const double p = std::min(total - 1.0, (pat == 6 ? rSrc - 2.0 * rLen : rSrc) + t * sp);
            const double atk = 0.0015 * sr, rel = std::min(0.006 * sr, rLen * 0.35);
            const float env = (float)(std::min(1.0, t / atk) * std::min(1.0, (rLen - t) / rel));
            const float fall = pat == 2 ? 0.8f : 1.0f - 0.25f * (float)std::min(1.0, d / ls);   // rolls ease off a little towards the end
            pl = bl.abs(p) * env * fall; pr = br.abs(p) * env * fall;
            break; }
        default: if (active) { const double p = wStart - ls + d * 0.5; pl = bl.abs(p); pr = br.abs(p); } break;
        }
        if (mode != 1) {
            // crossfade between live and effected, plus tiny fades at every chunk edge
            a += ((active ? 1.0f : 0.0f) - a) * 0.01f;
            const float edge = (float)std::min(1.0, std::min(d / 96.0, (ls - d) / 96.0));
            const float e = active ? edge : 1.0f;
            pl = L * (1 - a) + pl * a * e; pr = R * (1 - a) + pr * a * e;
        }
        const float m = q[kSMix];
        L = L * (1 - m) + pl * m; R = R * (1 - m) + pr * m;
    }
};

// 8 DELAY: tempo-synced, ping-pong, filtered and saturated feedback, wobble
struct Delay {
    Ring bl, br; OnePole lpL, lpR, hpL, hpR; float sr = 44100, lfo = 0; double cur = 0;
    void init(float s) { sr = s; bl.init((size_t)(s * 4.5f)); br.init((size_t)(s * 4.5f)); reset(); }
    void reset() { bl.clear(); br.clear(); lpL.z = lpR.z = hpL.z = hpR.z = 0; cur = 0; }
    inline void process(float& L, float& R, const float* q, double bpm) {
        const double target = std::min(4.3 * sr, kDelayBeats[std::min(8, std::max(0, (int)q[kYDiv]))] * 60.0 / bpm * sr);
        if (cur <= 0) cur = target;
        cur += (target - cur) * 0.0005;
        lfo += 0.45f / sr; if (lfo >= 1) lfo -= 1;
        const double d = std::max(4.0, cur + q[kYMod] * 0.004 * sr * std::sin(2 * kPi * lfo));
        float wl = bl.frac(d), wr = br.frac(d);
        const float lpa = onePoleCoef(1500.0f * std::pow(12.0f, q[kYTone]), sr), hpa = onePoleCoef(90.0f, sr);
        auto loop = [&](float x, OnePole& lp, OnePole& hp) { const float y = lp.lp(x, lpa); return std::tanh((y - hp.lp(y, hpa)) * 1.1f); };
        const float fb = q[kYFb];
        if (q[kYPing] > 0.5f) { bl.push((L + R) * 0.5f + loop(wr, lpR, hpR) * fb); br.push(loop(wl, lpL, hpL) * fb); }
        else { bl.push(L + loop(wl, lpL, hpL) * fb); br.push(R + loop(wr, lpR, hpR) * fb); }
        const float m = q[kYMix];
        L = L * (1 - m * 0.3f) + wl * m; R = R * (1 - m * 0.3f) + wr * m;
    }
};

// 9 REVERB: 8-line feedback delay network with diffusion, damping, motion and shimmer
struct Reverb {
    static constexpr int N = 8;
    Ring line[N], pre, ap[4]; float len[N] = { 0 }, damp[N] = { 0 }, gain[N] = { 0 }, sr = 44100, lfo = 0, lastSize = -1, lastDecay = -1, lastDamp = -1, dampA = 1;
    Shifter shL; float shimFb = 0;
    float apLen[4] = { 0 };
    void init(float s) {
        sr = s;
        for (int i = 0; i < N; ++i) line[i].init((size_t)(s * 0.32f));
        pre.init((size_t)(s * 0.3f));
        static const float apMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
        for (int i = 0; i < 4; ++i) { ap[i].init((size_t)(s * 0.02f)); apLen[i] = apMs[i] * 0.001f * s; }
        shL.init(s);
        reset();
    }
    void reset() { for (auto& l : line) l.clear(); pre.clear(); for (auto& a : ap) a.clear(); shL.clear(); for (float& d : damp) d = 0; shimFb = 0; }
    inline void process(float& L, float& R, const float* q) {
        static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
        const float size = q[kRSize];
        if (std::fabs(size - lastSize) > 1e-4f || std::fabs(q[kRDecay] - lastDecay) > 1e-4f) {
            lastSize = size; lastDecay = q[kRDecay];
            for (int i = 0; i < N; ++i) { len[i] = baseMs[i] * 0.001f * sr * (0.35f + size * 1.55f); gain[i] = std::pow(10.0f, -3.0f * len[i] / (lastDecay * sr)); }
        }
        if (std::fabs(q[kRDamp] - lastDamp) > 1e-4f) { lastDamp = q[kRDamp]; dampA = onePoleCoef(18000.0f * std::pow(0.08f, lastDamp), sr); }
        lfo += 0.31f / sr; if (lfo >= 1) lfo -= 1;
        float x = (L + R) * 0.5f + shimFb;
        pre.push(x);
        x = pre.frac(std::max(1.0f, q[kRPre] * 0.001f * sr));
        for (int i = 0; i < 4; ++i) {   // input diffusion
            const float dly = ap[i].frac(apLen[i]);
            const float v = x + dly * 0.65f; ap[i].push(v); x = dly - v * 0.65f;
        }
        float o[N]; float sum = 0;
        const float modD = q[kRMod] * 0.0012f * sr;
        for (int i = 0; i < N; ++i) {
            double d = len[i];
            if (i == 0) d += modD * std::sin(2 * kPi * lfo); else if (i == 4) d += modD * std::sin(2 * kPi * (lfo + 0.37f));
            o[i] = line[i].frac(d);
            damp[i] += (o[i] - damp[i]) * dampA;   // damping first, then the mixing matrix: never more than unity gain
            sum += damp[i];                        // (1.2.1: mixing the undamped signal could build up and howl with strong damping)
        }
        sum *= 2.0f / N;
        float wl = 0, wr = 0;
        for (int i = 0; i < N; ++i) {
            line[i].push((damp[i] - sum) * gain[i] + x * (i & 1 ? 0.5f : -0.5f));
            if (i & 1) wr += o[i] * (i & 2 ? -1.0f : 1.0f); else wl += o[i] * (i & 4 ? -1.0f : 1.0f);
        }
        wl *= 0.55f; wr *= 0.55f;
        const float shim = q[kRShim];
        shimFb = shim > 0.001f ? shL.process((wl + wr) * 0.5f, 2.0f, 0.06f * sr) * shim * 0.55f : 0.0f;
        const float m = q[kRMix];
        L = L * (1 - m) + wl * m; R = R * (1 - m) + wr * m;
    }
};

// ---------------------------------------------------------------------------------------------
// One complete effect chain (all 9 modules + its own smoothed settings). Lumen runs one for the single sound,
// or one per timeline section.
struct Chain {
    GrainCloud grain; PitchMod pitch; Drive drive; Filter filter; LoFi lofi; Mod mod; Stutter stut; Delay delay; Reverb reverb;
    float sm[kParamCount], q[kParamCount], onS[9] = { 0 }, trimG = 1, env = 0, sr = 44100;
    float act = 1;   // 1 while the sample plays, fades to 0 after it stops (ends frozen holds and tape hiss)
    bool first = true, dirty = false; int quiet = 0;

    void init(float s) { sr = s; grain.init(s); pitch.init(s); drive.init(s); filter.init(s); lofi.init(s); mod.init(s); stut.init(s); delay.init(s); reverb.init(s); reset(); }
    void reset() {
        grain.reset(); pitch.reset(); drive.reset(); filter.reset(); lofi.reset(); mod.reset(); stut.reset(); delay.reset(); reverb.reset();
        first = true; dirty = false; quiet = 0; env = 0;
        for (float& o : onS) o = 0;
    }
    static const bool* intTable() {
        static bool t[kParamCount]; static bool done = false;
        if (!done) { for (uint32_t i = 0; i < kParamCount; ++i) t[i] = getParamInfo(i).isInt(); done = true; }
        return t;
    }
    void control(const float* p) {
        applyMacros(p, q);
        const bool* isInt = intTable();
        for (uint32_t i = 0; i < kTlOn; ++i) {   // the sound's own settings (timeline settings are not part of a chain)
            if (first || isInt[i]) sm[i] = q[i];
            else sm[i] += (q[i] - sm[i]) * 0.3f;
        }
        first = false;
        trimG = std::pow(10.0f, sm[kTrim] / 20.0f);
        for (int m = 0; m < 9; ++m) onS[m] += ((sm[kModules[m].on] > 0.5f ? 1.0f : 0.0f) - onS[m]) * 0.25f;
    }
    // the effect chain; each module fades in/out when switched
    inline void run(float& L, float& R, double beat, double bpm) {
        auto stage = [&](int m, auto&& fn) {
            if (onS[m] < 0.0005f) return;
            float l = L, r = R; fn(l, r);
            const float a = onS[m] > 0.9995f ? 1.0f : onS[m];
            L = L * (1 - a) + l * a; R = R * (1 - a) + r * a;
        };
        stage(0, [&](float& l, float& r) { grain.process(l, r, sm, act); });
        stage(1, [&](float& l, float& r) { pitch.process(l, r, sm); });
        stage(2, [&](float& l, float& r) { drive.process(l, r, sm); });
        stage(3, [&](float& l, float& r) { filter.process(l, r, sm); });
        stage(4, [&](float& l, float& r) { lofi.process(l, r, sm, act); });
        stage(5, [&](float& l, float& r) { mod.process(l, r, sm); });
        stage(6, [&](float& l, float& r) { stut.process(l, r, sm, beat, bpm); });
        stage(7, [&](float& l, float& r) { delay.process(l, r, sm, bpm); });
        stage(8, [&](float& l, float& r) { reverb.process(l, r, sm); });
        L *= trimG; R *= trimG;   // level match only the effected sound
    }
};

// how long a sound rings on after the sample stops (for renders)
inline double tailSeconds(const float* q, double bpm)
{
    double tail = 0.2;
    if (q[kROn] > 0.5f) tail += std::min(12.0f, q[kRDecay] * (1.0f + q[kRShim] * 2.0f)) * std::min(1.0f, q[kRMix] * 2.0f + 0.2f);
    if (q[kYOn] > 0.5f) tail += std::min(6.0, kDelayBeats[(int)q[kYDiv]] * 60.0 / bpm * std::log(0.001) / std::log(std::max(0.05f, q[kYFb])));
    if (q[kGOn] > 0.5f) tail += q[kGSize] * 0.001;
    return std::min(14.0, tail);
}

// Section edits handed from the editor thread to the audio thread without locking (a sequence lock: odd = being written)
struct EditBox {
    std::atomic<uint32_t> seq { 0 };
    float ov[kMaxSections][kNumEdit];
    EditBox() { for (auto& r : ov) for (float& v : r) v = kNoEdit; }
    void write(const float (*src)[kNumEdit]) {
        seq.fetch_add(1, std::memory_order_acq_rel);
        std::memcpy(ov, src, sizeof ov);
        seq.fetch_add(1, std::memory_order_acq_rel);
    }
};

class LumenEngine {
public:
    float p[kParamCount];
    void setEditSource(const EditBox* b) { editSrc = b; editSeq = 0xFFFFFFFFu; }
    void setEdits(const float (*src)[kNumEdit]) { std::memcpy(secOv, src, sizeof secOv); editsChanged = true; }   // offline renders / tests
    void refreshSounds() { refreshReq.store(true); }   // own presets arrived (project load): look the sections' sounds up again

    LumenEngine() : chains(kPool) {
        for (uint32_t i = 0; i < kParamCount; ++i) p[i] = getParamInfo(i).def;
        for (int k = 0; k < kMaxSections; ++k) { lastSnd[k] = -1; ampS[k] = ampT[k] = 0; secChain[k] = -1; }
        for (int c = 0; c < kPool; ++c) owner[c] = -1;
        for (auto& row : secOv) for (float& v : row) v = kNoEdit;
    }

    void setSampleRate(double rate) {
        sr = rate > 1000 ? (float)rate : 44100.0f;
        for (auto& c : chains) c.init(sr);
        reset();
    }
    void reset() {
        for (auto& v : voices) v.active = false;
        releaseAllChains();
        for (int k = 0; k < kMaxSections; ++k) { ampS[k] = ampT[k] = 0; lastSnd[k] = -1; }
        playOn = false; internalBeat = 0; level = 0; gFirst = true; tlWas = false; posFrac = 0; killLeft = 0;
    }
    // STOP button: everything goes silent within ~10 ms (voices from Play AND from keys, echoes, reverb, frozen grains)
    void hardStop() { if (killLeft <= 0) killLeft = killLen = std::max(1, (int)(0.01f * sr)); killOrder = counter; }
    int currentSection() const { return curSec; }
    void setSample(const SampleData* s) { sample = s; for (auto& v : voices) v.active = false; }

    float playhead() const { return outPos; }
    float getLevel() const { return level; }
    bool playing() const { for (auto& v : voices) if (v.active && !v.released) return true; return false; }
    double getBpm() const { return bpm; }

    void process(float* outL, float* outR, uint32_t frames, const MidiMsg* midi, uint32_t midiCount, const TimeInfo& ti)
    {
#if defined(__SSE__) || defined(_M_X64)
        _mm_setcsr(_mm_getcsr() | 0x8040);   // flush tiny (denormal) numbers to zero: long reverb tails stay cheap
#endif
        bpm = (ti.valid && ti.bpm > 1.0) ? ti.bpm : 120.0;
        const bool have = sample && !sample->empty();
        // STOP: a change of the stop counter cuts everything (the first value seen is just remembered)
        const int stopN = (int)p[kStop];
        if (stopInit && stopN != lastStop) hardStop();
        lastStop = stopN; stopInit = true;
        // Play button
        const bool want = p[kPlay] > 0.5f && have;
        const int cue = (int)p[kCue];
        if (want && playOn && cue != lastCue) { noteOff(60, true); playOn = false; }   // preview: start again from PLAY FROM
        lastCue = cue;
        if (want && !playOn) noteOn(60, 0.9f, true, p[kPlayFrom]);
        if (!want && playOn) noteOff(60, true);
        playOn = want;

        uint32_t mi = 0;
        float peak = 0;
        const bool original = p[kOriginal] > 0.5f;
        for (uint32_t i = 0; i < frames; ++i) {
            while (mi < midiCount && midi[mi].frame <= i) { handleMidi(midi[mi]); ++mi; }
            if (killLeft > 0 && --killLeft == 0) {          // stop fade done: drop every voice and clear every buffer
                for (auto& v : voices) if (v.order <= killOrder) v.active = false;   // a note played during the fade survives
                releaseAllChains();
                for (int k = 0; k < kMaxSections; ++k) ampS[k] = 0;
                internalBeat = 0; actS = 0;
            }
            if (ctl <= 0) { control(); ctl = 32; }
            --ctl;

            float L = 0, R = 0;
            if (have) for (auto& v : voices) if (v.active) { float l, r; renderVoice(v, l, r); L += l; R += r; }
            const bool sounding = anyActive();
            actS += ((sounding ? 1.0f : 0.0f) - actS) * (sounding ? actUp : actDown);
            for (auto& c : chains) c.act = actS;
            const double beat = (ti.playing && ti.valid) ? ti.ppq + (double)i * bpm / 60.0 / sr : internalBeat;
            if (sounding) internalBeat += bpm / 60.0 / sr; else if (!(ti.playing && ti.valid)) internalBeat = 0;

            float oL, oR;
            if (original) { oL = L; oR = R; }
            else {
                float wL = 0, wR = 0;
                if (!tl) { wL = L; wR = R; chains[0].run(wL, wR, beat, bpm); }
                else {
                    // TIMELINE: every section that can be heard borrows a chain from the pool; the playhead decides how much each one gets
                    const bool ring = (int)p[kTlTails] == 0;
                    const int n = sectionCount(p);
                    for (int k = 0; k < kMaxSections; ++k) {
                        ampS[k] += (ampT[k] - ampS[k]) * ampA;
                        const bool feeding = k < n && ampS[k] > 1e-4f;
                        int ci = secChain[k];
                        if (ci < 0) { if (!feeding) continue; ci = acquireChain(k); }
                        Chain& c = chains[(size_t)ci];
                        if (!feeding && (!ring || c.quiet > (int)(0.4f * sr) || k >= n)) { releaseChain(k); continue; }   // silent: give it back
                        float l = L, r = R;
                        if (ring) { l *= ampS[k]; r *= ampS[k]; }   // fade what goes IN: echoes and reverb die out naturally
                        c.run(l, r, beat, bpm);
                        if (!ring) { l *= ampS[k]; r *= ampS[k]; }  // fade what comes OUT: the old sound stops with its fade
                        const float a = std::max(std::fabs(l), std::fabs(r));
                        c.env = std::max(a, c.env * 0.9995f);
                        if (!feeding && c.env < 2e-5f) ++c.quiet; else c.quiet = 0;
                        wL += l; wR += r;
                    }
                }
                const float dw = dwS;
                oL = L * (1 - dw) + wL * dw; oR = R * (1 - dw) + wR * dw;
            }
            gainSm += (outG - gainSm) * 0.002f;
            oL *= gainSm; oR *= gainSm;
            if (killLeft > 0) { const float k = (float)killLeft / (float)killLen; oL *= k * k; oR *= k * k; }
            if (!original) { oL = soft(oL); oR = soft(oR); }
            if (!std::isfinite(oL)) oL = 0;
            if (!std::isfinite(oR)) oR = 0;
            outL[i] = oL; outR[i] = oR;
            peak = std::max(peak, std::max(std::fabs(oL), std::fabs(oR)));
        }
        while (mi < midiCount) { handleMidi(midi[mi]); ++mi; }
        level = peak > level ? peak : level * 0.9f + peak * 0.1f;
        // playhead of the newest voice
        const Voice* best = nullptr;
        for (auto& v : voices) if (v.active && (!best || v.order > best->order)) best = &v;
        if (best && have) outPos = (float)(best->pos / (double)sample->frames());
    }

private:
    struct Voice {
        bool active = false, released = false, button = false, fOn = false;
        int note = 60; float vel = 1, env = 0; double pos = 0, inc = 1, fPos = 0; int fLeft = 0; uint64_t order = 0;
    };
    static constexpr int kVoices = 8, kXf = 256;
    Voice voices[kVoices];
    uint64_t counter = 0;
    const SampleData* sample = nullptr;
    float sr = 44100; double bpm = 120, internalBeat = 0;
    int ctl = 0; bool playOn = false, gFirst = true, tl = false, tlWas = false;
    float outG = 1, gainSm = 1, dwS = 1, level = 0, outPos = 0, posFrac = 0, ampA = 0.004f;
    static constexpr int kPool = 8;          // effect chains shared by the timeline sections (only audible ones need one)
    std::vector<Chain> chains;
    int secChain[kMaxSections], owner[kPool];
    float secP[kMaxSections][kParamCount];   // each section's settings (its sound + its edits + its macros)
    float secOv[kMaxSections][kNumEdit];     // the edits (kNoEdit = keep the sound's value)
    const EditBox* editSrc = nullptr; uint32_t editSeq = 0xFFFFFFFFu; bool editsChanged = true; std::atomic<bool> refreshReq { false };
    void pullEdits() {
        if (!editSrc) return;
        const uint32_t a = editSrc->seq.load(std::memory_order_acquire);
        if (a == editSeq || (a & 1u)) return;
        std::memcpy(secOv, editSrc->ov, sizeof secOv);
        if (editSrc->seq.load(std::memory_order_acquire) != a) return;   // changed while copying: try again next tick
        editSeq = a; editsChanged = true;
    }
    int lastSnd[kMaxSections];
    float ampS[kMaxSections], ampT[kMaxSections];
    float actS = 0, actUp = 0.01f, actDown = 1e-5f;
    int curSec = 0, lastCue = 0, lastStop = 0, killLeft = 0, killLen = 1; uint64_t killOrder = 0; bool stopInit = false;

    static inline float soft(float x) { const float a = std::fabs(x); if (a <= 0.89f) return x; const float y = 0.89f + 0.11f * std::tanh((a - 0.89f) / 0.11f); return x < 0 ? -y : y; }
    bool anyActive() const { for (auto& v : voices) if (v.active) return true; return false; }

    void control() {
        tl = p[kTlOn] > 0.5f && sectionCount(p) >= 2;
        if (tl != tlWas) {   // switching between the single sound and the timeline: start the chains fresh
            releaseAllChains();
            for (int k = 0; k < kMaxSections; ++k) ampS[k] = 0;
            tlWas = tl;
        }
        pullEdits();
        if (refreshReq.exchange(false)) editsChanged = true;
        const float dwT = p[kDryWet], oT = std::pow(10.0f, p[kOut] / 20.0f);
        if (gFirst) { dwS = dwT; gFirst = false; } else dwS += (dwT - dwS) * 0.3f;
        outG = oT;
        ampA = 1.0f - std::exp(-1.0f / (0.006f * sr));
        actUp = 1.0f - std::exp(-1.0f / (0.005f * sr)); actDown = 1.0f - std::exp(-1.0f / (0.7f * sr));
        // where the newest voice is in the sample region
        const Voice* best = nullptr;
        for (auto& v : voices) if (v.active && (!best || v.order > best->order)) best = &v;
        if (best && sample && !sample->empty()) { double a, b; regionFrames(a, b); posFrac = (float)clampf((float)((best->pos - a) / (b - a)), 0.0f, 1.0f); }
        if (!tl) { chains[0].control(p); curSec = 0; editsChanged = false; return; }
        const int n = sectionCount(p);
        float amp[kMaxSections];
        sectionWeights(p, posFrac, amp);
        float bestA = -1;
        for (int k = 0; k < kMaxSections; ++k) {
            ampT[k] = k < n ? amp[k] : 0.0f;
            if (ampT[k] > bestA) { bestA = ampT[k]; curSec = k; }
            if (k >= n) continue;
            const int snd = (int)p[slotParam(k, kSfSound)];
            if (snd != lastSnd[k] || editsChanged) { sectionValues(snd, secOv[k], secP[k]); lastSnd[k] = snd; }
            for (int m = 0; m < 4; ++m) secP[k][kM1 + m] = p[slotParam(k, kSfM1 + m)];
            if (secChain[k] >= 0) chains[(size_t)secChain[k]].control(secP[k]);
        }
        editsChanged = false;
    }

    // ---- chain pool: kPool chains shared by up to kMaxSections sections
    int acquireChain(int k) {
        int best = -1; float bestEnv = 1e9f;
        for (int c = 0; c < kPool; ++c) if (owner[c] < 0) { best = c; break; }
        if (best < 0)   // all busy: take the quietest one that is only ringing out (or, failing that, the quietest)
            for (int pass = 0; pass < 2 && best < 0; ++pass)
                for (int c = 0; c < kPool; ++c) {
                    const int o = owner[c];
                    if (pass == 0 && ampS[o] > 1e-4f) continue;
                    if (chains[(size_t)c].env < bestEnv) { bestEnv = chains[(size_t)c].env; best = c; }
                }
        if (owner[best] >= 0) secChain[owner[best]] = -1;
        owner[best] = k; secChain[k] = best;
        Chain& c = chains[(size_t)best];
        c.reset(); c.first = true; c.quiet = 0; c.env = 0; c.act = actS;
        c.control(secP[k]);
        return best;
    }
    void releaseChain(int k) { const int c = secChain[k]; if (c >= 0) { owner[c] = -1; secChain[k] = -1; } }
    void releaseAllChains() {
        for (auto& c : chains) c.reset();
        for (int c = 0; c < kPool; ++c) owner[c] = -1;
        for (int k = 0; k < kMaxSections; ++k) secChain[k] = -1;
    }

    // ---- player
    void regionFrames(double& a, double& b) const {
        const double n = (double)sample->frames();
        double s = std::min(p[kRegStart], p[kRegEnd]), e = std::max(p[kRegStart], p[kRegEnd]);
        if (e - s < 0.001) e = std::min(1.0, s + 0.001);
        a = s * n; b = std::max(a + 64, e * n);
    }
    void noteOn(int note, float vel, bool button, float from = 0.0f) {
        if (!sample || sample->empty()) return;
        Voice* v = nullptr;
        for (auto& x : voices) if (!x.active) { v = &x; break; }
        if (!v) { for (auto& x : voices) if (!v || x.order < v->order) v = &x; }
        double a, b; regionFrames(a, b);
        v->active = true; v->released = false; v->button = button; v->note = note; v->vel = vel; v->env = 0;
        v->pos = a + clampf(from, 0.0f, 0.999f) * (b - a); v->fOn = false; v->fLeft = 0; v->order = ++counter;
        const float semis = (p[kKeys] < 0.5f && !button) ? (float)(note - 60) : 0.0f;
        v->inc = sample->rate / sr * std::pow(2.0, semis / 12.0);
    }
    void noteOff(int note, bool button) { for (auto& v : voices) if (v.active && v.note == note && v.button == button) v.released = true; }
    void handleMidi(const MidiMsg& m) {
        const uint8_t st = m.status & 0xF0;
        if (st == 0x90 && m.d2 > 0) noteOn(m.d1, m.d2 / 127.0f, false);
        else if (st == 0x80 || (st == 0x90 && m.d2 == 0)) noteOff(m.d1, false);
        else if (st == 0xB0 && m.d1 == 123) { for (auto& v : voices) if (!v.button) v.released = true; }   // all notes off: release, tails ring
        else if (st == 0xB0 && m.d1 == 120) hardStop();                                                   // all sound off: silence now
    }
    static inline float hermite(const std::vector<float>& d, double pos) {
        const int64_t n = (int64_t)d.size(), i = (int64_t)std::floor(pos); const float f = (float)(pos - (double)i);
        auto at = [&](int64_t k) { return (k < 0 || k >= n) ? 0.0f : d[(size_t)k]; };
        const float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
        const float c = (x1 - xm1) * 0.5f, v = x0 - x1, w = c + v, a = w + v + (x2 - x0) * 0.5f, b = w + a;
        return ((((a * f) - b) * f + c) * f + x0);
    }
    void renderVoice(Voice& v, float& l, float& r) {
        l = r = 0;
        if (v.released) { v.env -= 1.0f / (0.04f * sr); if (v.env <= 0) { v.active = false; return; } }
        else if (v.env < 1) v.env = std::min(1.0f, v.env + 1.0f / (0.003f * sr));
        double a, b; regionFrames(a, b);
        if (v.pos >= b) {
            v.fOn = true; v.fPos = v.pos; v.fLeft = kXf;          // old head fades out
            if (p[kLoop] > 0.5f) v.pos = a + (v.pos - b);
            else v.released = true, v.pos = (double)sample->frames() + 8;   // one-shot: silence after the end
        }
        const float fin = v.fOn ? 1.0f - (float)v.fLeft / kXf : 1.0f;
        l += hermite(sample->L, v.pos) * fin; r += hermite(sample->R, v.pos) * fin;
        v.pos += v.inc;
        if (v.fOn) {
            const float fo = (float)v.fLeft / kXf;
            l += hermite(sample->L, v.fPos) * fo; r += hermite(sample->R, v.fPos) * fo;
            v.fPos += v.inc;
            if (--v.fLeft <= 0) v.fOn = false;
        }
        const float g = v.env * std::min(1.0f, 0.1f + v.vel);
        l *= g; r *= g;
    }
};

// ---------------------------------------------------------------------------------------------
// Offline render (drag to DAW): the sample played `passes` times through the current sound, plus the tail.
inline void renderLumenOffline(const float* params, const SampleData& s, double sr, double bpm, int passes,
                               std::vector<float>& L, std::vector<float>& R, const float (*edits)[kNumEdit] = nullptr,
                               std::atomic<float>* progress = nullptr, const std::atomic<bool>* cancel = nullptr)
{
    LumenEngine* e = new LumenEngine();
    if (edits) e->setEdits(edits);
    for (uint32_t i = 0; i < kNumIn; ++i) e->p[i] = params[i];
    e->p[kPlay] = 1.0f; e->p[kPlayFrom] = 0.0f;
    if (passes <= 1) e->p[kLoop] = 0.0f;
    e->setSampleRate(sr);
    e->setSample(&s);
    const double a = std::min(params[kRegStart], params[kRegEnd]), b = std::max(params[kRegStart], params[kRegEnd]);
    const double regionSec = std::max(0.05, (b - a) * (double)s.frames() / s.rate);
    double tail = 0.2;
    if (params[kOriginal] < 0.5f) {
        float q[kParamCount];
        if (params[kTlOn] > 0.5f && sectionCount(params) >= 2) {
            const int k = sectionCount(params) - 1;   // the last section rings out
            float sp[kParamCount]; sectionValues((int)params[slotParam(k, kSfSound)], edits ? edits[k] : nullptr, sp);
            for (int m = 0; m < 4; ++m) sp[kM1 + m] = params[slotParam(k, kSfM1 + m)];
            applyMacros(sp, q);
        } else applyMacros(params, q);
        tail = tailSeconds(q, bpm);
    }
    const size_t body = (size_t)(regionSec * std::max(1, passes) * sr);
    const size_t total = body + (size_t)(tail * sr);
    L.assign(total, 0.0f); R.assign(total, 0.0f);
    TimeInfo ti; ti.valid = true; ti.playing = true; ti.bpm = bpm; ti.ppq = 0;
    size_t pos = 0;
    while (pos < total) {
        const uint32_t n = (uint32_t)std::min<size_t>(256, total - pos);
        if (pos >= body) e->p[kPlay] = 0.0f;
        e->process(&L[pos], &R[pos], n, nullptr, 0, ti);
        ti.ppq += n * bpm / 60.0 / sr;
        pos += n;
        if ((pos & 0x3FFF) < 256) {
            if (progress) progress->store((float)pos / (float)total);
            if (cancel && cancel->load()) { L.clear(); R.clear(); break; }
        }
    }
    if (progress) progress->store(1.0f);
    delete e;
}

// A built-in source so Lumen makes sound right away: a soft piano-ish chord phrase over a light beat (8 s, 120 bpm).
inline void makeLumenSource(SampleData& s, double rate = 44100.0)
{
    s.rate = rate;
    const size_t n = (size_t)(rate * 8.0);
    s.L.assign(n, 0.0f); s.R.assign(n, 0.0f);
    static const int chords[4][4] = { { 57, 60, 64, 69 }, { 53, 57, 60, 65 }, { 48, 55, 60, 64 }, { 55, 59, 62, 67 } };
    for (int c = 0; c < 4; ++c) {
        for (int k = 0; k < 4; ++k) {
            const double f0 = 440.0 * std::pow(2.0, (chords[c][k] - 69) / 12.0);
            const double start = c * 2.0 * rate + k * 0.03 * rate;
            const float pan = (k - 1.5f) / 2.5f;
            for (size_t i = (size_t)start; i < (size_t)std::min<double>((double)n, start + 2.6 * rate); ++i) {
                const double t = (i - start) / rate;
                const double env = (1.0 - std::exp(-t * 400.0)) * (0.65 * std::exp(-t * 2.2) + 0.35 * std::exp(-t * 0.7));
                double v = 0;
                for (int h = 1; h <= 7; ++h) v += std::sin(2.0 * 3.14159265 * f0 * h * (1.0 + 0.0004 * h * h) * t) * std::exp(-t * (0.8 + h * 1.1)) / std::pow((double)h, 1.6);
                v *= env * 0.11;
                s.L[i] += (float)(v * (1.0 - pan)); s.R[i] += (float)(v * (1.0 + pan));
            }
        }
    }
    // a soft beat: kick on 1 and 3, rim on 2 and 4, shaker eighths
    uint32_t r = 12345;
    auto rnd = [&]() { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return (float)(r >> 8) / 16777216.0f * 2.0f - 1.0f; };
    for (int beat = 0; beat < 16; ++beat) {
        const size_t b0 = (size_t)(beat * 0.5 * rate);
        if (beat % 2 == 0) for (size_t i = 0; i < (size_t)(0.35 * rate) && b0 + i < n; ++i) {
            const double t = i / rate; const double ph = 2 * 3.14159265 * (50 * t + 60 * (1 - std::exp(-t * 30)) / 30);
            const float v = (float)(std::sin(ph) * std::exp(-t * 9) * 0.32);
            s.L[b0 + i] += v; s.R[b0 + i] += v;
        } else for (size_t i = 0; i < (size_t)(0.08 * rate) && b0 + i < n; ++i) {
            const double t = i / rate; const float v = (float)((std::sin(2 * 3.14159265 * 1700 * t) * 0.5 + rnd() * 0.5) * std::exp(-t * 60) * 0.14);
            s.L[b0 + i] += v * 0.9f; s.R[b0 + i] += v;
        }
        for (int h = 0; h < 2; ++h) {
            const size_t h0 = b0 + (size_t)(h * 0.25 * rate);
            float lp = 0;
            for (size_t i = 0; i < (size_t)(0.05 * rate) && h0 + i < n; ++i) { const double t = i / rate; const float x = rnd(); const float hp = x - lp; lp += (x - lp) * 0.3f; const float v = (float)(hp * std::exp(-t * 90) * 0.05); s.L[h0 + i] += v; s.R[h0 + i] += v * 0.8f; }
        }
    }
}

} // namespace lm
