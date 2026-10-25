// Lumen engine tests + preset level measurement
//   g++ -std=gnu++17 -O2 -Iplugins/Shared -Iplugins/Lumen -Idpf/distrho plugins/Lumen/tests/test_lumen.cpp plugins/Shared/Export.cpp -o test_lumen && ./test_lumen out [trims.txt]
#include "LumenDSP.hpp"
#include "Export.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <set>
#include <complex>
#include <algorithm>

using namespace lm;

static int failures = 0;
static void check(bool ok, const std::string& what) { std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what.c_str()); if (!ok) ++failures; }

struct Stats { float peak = 0; double rms = 0; bool finite = true; };
static Stats run(LumenEngine& e, double secs, std::vector<float>* keep = nullptr, std::vector<MidiMsg> ev = {}, double sr = 44100)
{
    Stats st; double sum = 0; size_t n = 0;
    std::vector<float> l(256), r(256);
    TimeInfo ti; ti.valid = true; ti.bpm = 120; ti.playing = false;
    for (size_t pos = 0; pos < (size_t)(secs * sr); pos += 256) {
        std::vector<MidiMsg> be; for (auto& m : ev) if (m.frame >= pos && m.frame < pos + 256) { MidiMsg x = m; x.frame -= (uint32_t)pos; be.push_back(x); }
        e.process(l.data(), r.data(), 256, be.data(), (uint32_t)be.size(), ti);
        for (int i = 0; i < 256; ++i) {
            if (!std::isfinite(l[i]) || !std::isfinite(r[i])) st.finite = false;
            st.peak = std::max(st.peak, std::max(std::fabs(l[i]), std::fabs(r[i])));
            sum += (double)l[i] * l[i] + (double)r[i] * r[i]; ++n;
        }
        if (keep) { keep->insert(keep->end(), l.begin(), l.end()); }
    }
    st.rms = std::sqrt(sum / std::max<size_t>(1, n * 2));
    return st;
}

// ---- sound fingerprint for the duplicate check: what the preset does to the same input
//   spectrum shape (16 bands), harmony (12 pitch classes), width, movement, rhythm and ring-out
constexpr int kNumFeat = 24, kBands = 16;
struct Feat { double f[kNumFeat]; double chroma[12]; };
static void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1) {
        const std::complex<double> w = std::polar(1.0, -2 * M_PI / len);
        for (size_t i = 0; i < n; i += len) { std::complex<double> u = 1; for (size_t k = 0; k < len / 2; ++k) { const auto x = a[i + k], y = a[i + k + len / 2] * u; a[i + k] = x + y; a[i + k + len / 2] = x - y; u *= w; } }
    }
}
static Feat fingerprint(LumenEngine& e)
{
    std::vector<float> L, R; L.reserve(44100 * 8); R.reserve(44100 * 8);
    std::vector<float> l(256), r(256);
    TimeInfo ti; ti.valid = true; ti.bpm = 120; ti.playing = false;
    for (int pos = 0; pos < 44100 * 8; pos += 256) {
        if (pos >= 44100 * 6) e.p[kPlay] = 0;               // last 2 s: what rings on
        e.process(l.data(), r.data(), 256, nullptr, 0, ti);
        L.insert(L.end(), l.begin(), l.end()); R.insert(R.end(), r.begin(), r.end());
    }
    const size_t a = 44100, b = 44100 * 6;
    Feat f{};
    // spectrum: averaged 4096-point frames
    const int N = 4096; std::vector<double> mag(N / 2, 0.0);
    for (size_t s0 = a; s0 + N <= b; s0 += N) {
        std::vector<std::complex<double>> x(N);
        for (int i = 0; i < N; ++i) x[i] = 0.5 * (L[s0 + i] + R[s0 + i]) * (0.5 - 0.5 * std::cos(2 * M_PI * i / N));
        fft(x); for (int k = 0; k < N / 2; ++k) mag[k] += std::norm(x[k]);
    }
    double band[kBands] = {}, tot = 1e-12;
    for (int k = 2; k < N / 2; ++k) {
        const double hz = k * 44100.0 / N;
        const int bi = std::min(kBands - 1, std::max(0, (int)(std::log2(hz / 40.0) * kBands / 9.0)));   // 40 Hz .. 20 kHz
        band[bi] += mag[k]; tot += mag[k];
        if (hz > 90 && hz < 4000) { const int pc = ((int)std::lround(12 * std::log2(hz / 440.0)) % 12 + 12) % 12; f.chroma[pc] += mag[k]; }
    }
    for (int k = 0; k < kBands; ++k) f.f[k] = std::log10(band[k] / tot + 1e-5);
    double cs = 1e-12; for (double c : f.chroma) cs += c * c; for (double& c : f.chroma) c /= std::sqrt(cs);
    // time domain
    double mid2 = 0, side2 = 0; std::vector<double> env; double win = 0; int wn = 0;
    for (size_t i = a; i < b; ++i) {
        const double m = 0.5 * (L[i] + R[i]), s = 0.5 * (L[i] - R[i]);
        mid2 += m * m; side2 += s * s;
        win += m * m + s * s; if (++wn == 441) { env.push_back(std::sqrt(win / wn)); win = 0; wn = 0; }
    }
    double tail = 0; for (size_t i = b + 4410; i < L.size(); ++i) tail += (double)L[i] * L[i] + (double)R[i] * R[i];
    double em = 0, ev = 0, ed = 0; for (double x : env) em += x; em /= env.size();
    for (size_t i = 0; i < env.size(); ++i) { ev += (env[i] - em) * (env[i] - em); if (i) ed += std::fabs(env[i] - env[i - 1]); }
    auto ac = [&](size_t lag) { double s = 0, n = 0; for (size_t i = lag; i < env.size(); ++i) { s += (env[i] - em) * (env[i - lag] - em); n += 1; } return s / std::max(1e-12, n * ev / env.size()); };
    f.f[16] = std::log10(side2 / (mid2 + 1e-12) + 1e-3);                // width
    f.f[17] = std::sqrt(ev / env.size()) / (em + 1e-9);                  // how much the level moves
    f.f[18] = ed / env.size() / (em + 1e-9);                             // how fast it moves
    f.f[19] = std::log10(tail / (mid2 + side2 + 1e-12) + 1e-4);          // ring-out after stop
    f.f[20] = ac(12); f.f[21] = ac(25); f.f[22] = ac(6); f.f[23] = ac(50); // rhythm at 1/16, 1/8, 1/32, 1/4
    return f;
}
// recipe distance: per module, both off = same, one off = different, both on = how far apart the settings are
static double recipeDist(const float* a, const float* b)
{
    double d = 0;
    for (const auto& m : kModules) {
        const bool oa = a[m.on] > 0.5f, ob = b[m.on] > 0.5f;
        if (!oa && !ob) continue;
        if (oa != ob) { d += 1; continue; }
        double s = 0; int n = 0;
        for (uint32_t k = m.first; k <= m.last; ++k, ++n) {
            const ParamInfo in = getParamInfo(k);
            if (in.isInt() && in.max - in.min <= 8) s += (std::lround(a[k]) != std::lround(b[k])) ? 1.0 : 0.0;
            else s += std::fabs(toNorm(k, a[k]) - toNorm(k, b[k]));
        }
        d += s / n;
    }
    return d;
}

int main(int argc, char** argv)
{
    const std::string out = argc > 1 ? argv[1] : ".";
    SampleData src; makeLumenSource(src, 44100.0);

    // ---- ORIGINAL: the file comes out untouched
    {
        std::unique_ptr<LumenEngine> e(new LumenEngine()); e->setSampleRate(44100); e->setSample(&src);
        float pv[kParamCount]; presetValues(10, pv); for (uint32_t i = 0; i < kNumIn; ++i) e->p[i] = pv[i];
        e->p[kOriginal] = 1; e->p[kPlay] = 1;
        std::vector<float> L; run(*e, 2.0, &L);
        double e2 = 0, s2 = 0; for (size_t i = 3000; i < 60000; ++i) { const double d = L[i] - src.L[i]; e2 += d * d; s2 += (double)src.L[i] * src.L[i]; }
        const double rel = std::sqrt(e2 / s2);
        std::printf("      original error %.5f\n", rel);
        check(rel < 0.01, "ORIGINAL = the sample itself (only the Play velocity level)");
    }
    // ---- loop off: stops at the end; loop on: starts again
    {
        std::unique_ptr<LumenEngine> e(new LumenEngine()); e->setSampleRate(44100); e->setSample(&src);
        e->p[kOriginal] = 1; e->p[kLoop] = 0; e->p[kPlay] = 1;
        run(*e, 8.3); const Stats s = run(*e, 0.5);
        check(s.peak < 1e-4f && !e->playing(), "LOOP off: plays once, then silence");
        std::unique_ptr<LumenEngine> f(new LumenEngine()); f->setSampleRate(44100); f->setSample(&src);
        f->p[kOriginal] = 1; f->p[kLoop] = 1; f->p[kPlay] = 1; f->p[kRegEnd] = 0.25f;   // 2 s region
        std::vector<float> L; run(*f, 2.6, &L);
        double e2 = 0, s2 = 0; for (size_t i = 0; i < 20000; ++i) { const double d = L[88200 + 2000 + i] - src.L[2000 + i]; e2 += d * d; s2 += (double)src.L[2000 + i] * src.L[2000 + i]; }
        check(std::sqrt(e2 / s2) < 0.02, "LOOP on with END at 25 %: starts again after 2 s");
    }
    // ---- keys transpose
    {
        std::unique_ptr<LumenEngine> e(new LumenEngine()); e->setSampleRate(44100); e->setSample(&src);
        e->p[kOriginal] = 1;
        std::vector<float> L; run(*e, 1.0, &L, { { 0, 0x90, 72, 100 } });
        // an octave up plays twice as fast: output at t matches the source at 2t
        double e2 = 0, s2 = 0; for (size_t i = 2000; i < 12000; ++i) { const double d = L[i] - src.L[2 * i] * 1.0; e2 += d * d; s2 += (double)src.L[2 * i] * src.L[2 * i]; }
        check(std::sqrt(e2 / s2) < 0.15, "C4 plays the sample an octave up (keys = PITCH)");
    }
    // ---- every preset: stable, audible, measure level for matching
    double dryRms;
    {
        std::unique_ptr<LumenEngine> e(new LumenEngine()); e->setSampleRate(44100); e->setSample(&src);
        e->p[kOriginal] = 1; e->p[kPlay] = 1; dryRms = run(*e, 6.0).rms;
    }
    std::FILE* tf = argc > 2 ? std::fopen(argv[2], "w") : nullptr;
    int bad = 0; float worstPeak = 0;
    for (int i = 0; i < kNumPresets; ++i) {
        std::unique_ptr<LumenEngine> e(new LumenEngine()); e->setSampleRate(44100); e->setSample(&src);
        float pv[kParamCount]; presetValues(i, pv); for (uint32_t k = 0; k < kNumIn; ++k) e->p[k] = pv[k];
        e->p[kPlay] = 1;
        const Stats s = run(*e, 6.0);
        // also sweep every macro to both ends: must stay stable
        bool ok = s.finite && s.rms > 0.003;
        for (int m = 0; m < 4 && ok; ++m) for (float v : { 0.0f, 1.0f }) {
            e->p[kM1 + m] = v; const Stats t = run(*e, 0.6); if (!t.finite || t.peak > 1.5f) ok = false;
            e->p[kM1 + m] = pv[kM1 + m];
        }
        const double cur = s.rms / std::pow(10.0, pv[kTrim] / 20.0);   // level without the stored trim
        double trim = 20.0 * std::log10(dryRms / std::max(1e-6, cur));
        trim = std::max(-18.0, std::min(18.0, trim));
        if (tf) std::fprintf(tf, "%d %.2f\n", i, trim);
        worstPeak = std::max(worstPeak, s.peak);
        if (s.peak > 0.97f) std::printf("      hits the limiter: %s (peak %.2f)\n", kPresets[i].name, s.peak);
        if (!ok) { ++bad; std::printf("      preset %d %s: rms %.4f peak %.3f finite %d\n", i, kPresets[i].name, s.rms, s.peak, (int)s.finite); }
        if (std::fabs(20.0 * std::log10(s.rms / dryRms)) > 3.0 && !tf) std::printf("      level off: %s %.1f dB\n", kPresets[i].name, 20.0 * std::log10(s.rms / dryRms));
    }
    if (tf) std::fclose(tf);
    check(bad == 0, "all " + std::to_string(kNumPresets) + " presets are stable and audible, also with every macro at 0 and 1");
    std::printf("      loudest preset peak %.3f\n", worstPeak);

    // ---- no duplicates: every sound must differ from every other in recipe AND in how it sounds
    {
        std::vector<Feat> F(kNumPresets); std::vector<std::vector<float>> V(kNumPresets, std::vector<float>(kParamCount));
        for (int i = 0; i < kNumPresets; ++i) {
            std::unique_ptr<LumenEngine> e(new LumenEngine()); e->setSampleRate(44100); e->setSample(&src);
            presetValues(i, V[i].data()); for (uint32_t k = 0; k < kNumIn; ++k) e->p[k] = V[i][k];
            e->p[kPlay] = 1; F[i] = fingerprint(*e);
        }
        double mu[kNumFeat] = {}, sd[kNumFeat] = {};
        for (auto& f : F) for (int k = 0; k < kNumFeat; ++k) mu[k] += f.f[k] / kNumPresets;
        for (auto& f : F) for (int k = 0; k < kNumFeat; ++k) sd[k] += (f.f[k] - mu[k]) * (f.f[k] - mu[k]) / kNumPresets;
        for (int k = 0; k < kNumFeat; ++k) sd[k] = std::sqrt(sd[k]) + 1e-9;
        struct Pair { double audio, recipe; int a, b; };
        std::vector<Pair> pairs;
        for (int i = 0; i < kNumPresets; ++i) for (int j = i + 1; j < kNumPresets; ++j) {
            double d = 0; for (int k = 0; k < kNumFeat; ++k) { const double x = (F[i].f[k] - F[j].f[k]) / sd[k]; d += x * x; }
            double cdot = 0; for (int k = 0; k < 12; ++k) cdot += F[i].chroma[k] * F[j].chroma[k];
            d = std::sqrt(d / kNumFeat) + 6.0 * (1.0 - cdot);            // harmony counts as much as everything else
            pairs.push_back({ d, recipeDist(V[i].data(), V[j].data()), i, j });
        }
        // a near-duplicate is close in BOTH the recipe and the sound; the original 120 set the bar
        auto score = [](const Pair& q) { return std::min(q.audio / 0.45, 1.0) + std::min(q.recipe / 0.9, 1.0); };
        std::sort(pairs.begin(), pairs.end(), [&](const Pair& x, const Pair& y) { return score(x) < score(y); });
        double oldMin = 1e9; for (auto& q : pairs) if (q.b < 120) { oldMin = score(q); std::printf("      closest pair of the original 120: %.3f (audio %.2f recipe %.2f) %s / %s\n", oldMin, q.audio, q.recipe, kPresets[q.a].name, kPresets[q.b].name); break; }
        int shown = 0, tooClose = 0;
        for (auto& q : pairs) {
            if (q.b < 120) continue;
            if (shown++ < 25) std::printf("        %.3f  audio %.2f recipe %.2f   %s  /  %s\n", score(q), q.audio, q.recipe, kPresets[q.a].name, kPresets[q.b].name);
            if (score(q) < std::max(oldMin, 0.8)) ++tooClose;
        }
        check(tooClose == 0, "no new sound is a near-copy of another (closer than the closest pair of the original 120)");
        std::set<std::string> names; bool uniq = true; for (int i = 0; i < kNumPresets; ++i) uniq &= names.insert(kPresets[i].name).second;
        check(uniq, "all sound names are unique");
    }

    // ---- stutter repeat really repeats; tape stop slows; gate chops
    {
        std::unique_ptr<LumenEngine> e(new LumenEngine()); e->setSampleRate(44100); e->setSample(&src);
        e->p[kPlay] = 1; e->p[kSOn] = 1; e->p[kSMode] = 1; e->p[kSDiv] = 4; e->p[kSAmt] = 1; e->p[kSPat] = 1;  // gate 1/16 alternating
        std::vector<float> L; run(*e, 3.0, &L);
        double on = 0, off = 0; const size_t step = 5512;   // 1/16 at 120 bpm
        for (size_t k = 8; k < 24; ++k) for (size_t i = 1500; i < step - 500; ++i) { const double v = (double)L[k * step + i] * L[k * step + i]; if (k % 2 == 0) on += v; else off += v; }
        std::printf("      gate on/off energy ratio %.1f\n", on / std::max(1e-12, off));
        check(on > off * 50, "GATE pattern chops on the 1/16 grid");
    }
    {
        // offline render (drag to DAW)
        float pv[kParamCount]; presetValues(16, pv);   // Shimmer Hall
        std::vector<float> L, R; renderLumenOffline(pv, src, 44100, 120, 1, L, R);
        float pk = 0; for (float v : L) pk = std::max(pk, std::fabs(v));
        check(L.size() > 8 * 44100 && L.size() < 30 * 44100 && pk > 0.05f, "offline render = sample + reverb tail (" + std::to_string(L.size() / 44100.0) + " s)");
        gf::writeWav24(out + "/lumen_shimmer.wav", L, R, 44100);
    }
    // ---- TIMELINE: fade from one sound to the next as the sample plays
    {
        auto find = [](const char* n) { for (int i = 0; i < kNumPresets; ++i) if (!std::strcmp(kPresets[i].name, n)) return i; return -1; };
        const int dark = find("Lowpass Muffle"), bright = find("Highpass Rise"), hall = find("Infinite");
        check(dark >= 0 && bright >= 0 && hall >= 0, "test presets exist");
        // brightness = energy of the first difference / energy (high = bright)
        auto bright_at = [](const std::vector<float>& L, double t0, double t1) {
            double d = 0, e = 0; for (size_t i = (size_t)(t0 * 44100) + 1; i < (size_t)(t1 * 44100); ++i) { const double x = L[i], dx = L[i] - L[i - 1]; e += x * x; d += dx * dx; }
            return d / std::max(1e-12, e);
        };
        auto setup = [&](LumenEngine& e, int a, int b, float fade, int curve, int tails) {
            e.setSampleRate(44100); e.setSample(&src);
            e.p[kPlay] = 1; e.p[kLoop] = 0; e.p[kTlOn] = 1; e.p[kTlCount] = 2; e.p[kTlCurve] = (float)curve; e.p[kTlTails] = (float)tails;
            e.p[slotParam(0, kSfSound)] = (float)a; e.p[slotParam(1, kSfSound)] = (float)b;
            float pa[kParamCount], pb[kParamCount]; presetValues(a, pa); presetValues(b, pb);
            for (int m = 0; m < 4; ++m) { e.p[slotParam(0, kSfM1 + m)] = pa[kM1 + m]; e.p[slotParam(1, kSfM1 + m)] = pb[kM1 + m]; }
            e.p[slotParam(1, kSfStart)] = 0.5f; e.p[slotParam(1, kSfFade)] = fade;   // B starts at 4 s of the 8 s sample
        };
        std::unique_ptr<LumenEngine> e(new LumenEngine()); setup(*e, dark, bright, 0.5f, 0, 0);
        std::vector<float> L; run(*e, 8.0, &L);
        const double b1 = bright_at(L, 0.5, 2.5), b2 = bright_at(L, 5.5, 7.5);
        std::printf("      timeline brightness: section 1 %.4f, section 2 %.4f\n", b1, b2);
        check(b2 > b1 * 4, "TIMELINE: the first half sounds like sound 1 (dark), the second like sound 2 (bright)");
        // the fade really takes time: halfway through the fade both sounds are present
        // compare the timeline with each sound played alone: correlation in a window
        auto alone = [&](int preset, std::vector<float>& out) {
            std::unique_ptr<LumenEngine> s(new LumenEngine()); s->setSampleRate(44100); s->setSample(&src);
            float pv[kParamCount]; presetValues(preset, pv); for (uint32_t i = 0; i < kNumIn; ++i) s->p[i] = pv[i];
            s->p[kPlay] = 1; s->p[kLoop] = 0; run(*s, 8.0, &out);
        };
        auto corr = [](const std::vector<float>& a, const std::vector<float>& b, double t0, double t1) {
            double ab = 0, aa = 0, bb = 0; for (size_t i = (size_t)(t0 * 44100); i < (size_t)(t1 * 44100); ++i) { ab += (double)a[i] * b[i]; aa += (double)a[i] * a[i]; bb += (double)b[i] * b[i]; }
            return ab / std::sqrt(std::max(1e-12, aa * bb));
        };
        std::vector<float> A1, B1; alone(dark, A1); alone(bright, B1);
        { std::unique_ptr<LumenEngine> h(new LumenEngine()); setup(*h, dark, bright, 0.0f, 0, 1);
          std::vector<float> H; run(*h, 8.0, &H);
          const double hA = corr(H, A1, 3.6, 3.95), hB = corr(H, B1, 4.1, 4.6);
          const double sA = corr(L, A1, 3.9, 4.1), sB = corr(L, B1, 3.9, 4.1);
          const double eA = corr(L, A1, 1.0, 2.5), eB = corr(L, B1, 5.5, 7.0);
          std::printf("      hard cut: before = sound 1 (%.2f), after = sound 2 (%.2f) \xC2\xB7 soft fade middle: sound 1 %.2f + sound 2 %.2f \xC2\xB7 away from it %.2f / %.2f\n", hA, hB, sA, sB, eA, eB);
          check(hA > 0.95 && hB > 0.9, "FADE 0 = hard cut: sound 1 right up to the line, sound 2 right after it");
          check(sA > 0.3 && sB > 0.3 && eA > 0.95 && eB > 0.9, "a soft fade blends both sounds in the middle, and each is pure away from the fade"); }
        // tails: a huge reverb into the dark sound -> RING keeps the reverb sounding after the line, CUT stops it
        { std::unique_ptr<LumenEngine> r(new LumenEngine()); setup(*r, hall, dark, 0.0f, 0, 0);
          std::unique_ptr<LumenEngine> c(new LumenEngine()); setup(*c, hall, dark, 0.0f, 0, 1);
          std::vector<float> RA, CB; run(*r, 6.0, &RA); run(*c, 6.0, &CB);
          double diff = 0, tot = 0; for (size_t i = (size_t)(4.1 * 44100); i < (size_t)(5.5 * 44100); ++i) { const double d = RA[i] - CB[i]; diff += d * d; tot += (double)CB[i] * CB[i]; }
          std::printf("      after the line, the old reverb adds %.0f %% energy with TAILS RING\n", diff / std::max(1e-12, tot) * 100);
          check(diff > tot * 0.05, "TAILS RING lets the old reverb ring on, TAILS CUT stops it"); }
        // no clicks through a fade
        { float jump = 0; for (size_t i = (size_t)(3.0 * 44100); i < (size_t)(5.0 * 44100); ++i) jump = std::max(jump, std::fabs(L[i + 1] - L[i]));
          float j0 = 0; for (size_t i = (size_t)(0.5 * 44100); i < (size_t)(2.5 * 44100); ++i) j0 = std::max(j0, std::fabs(L[i + 1] - L[i]));
          check(jump < std::max(0.2f, j0 * 3), "no clicks while fading"); }
        // weights: smooth = constant power, linear sums to one, 3 sections
        { float pp[kParamCount]; for (uint32_t i = 0; i < kParamCount; ++i) pp[i] = getParamInfo(i).def;
          pp[kTlCount] = 3; pp[slotParam(1, kSfStart)] = 0.3f; pp[slotParam(2, kSfStart)] = 0.7f; pp[slotParam(1, kSfFade)] = 1; pp[slotParam(2, kSfFade)] = 1;
          bool okP = true, okL = true; float a[kMaxSections];
          for (int s = 0; s <= 100; ++s) { sectionWeights(pp, s / 100.0f, a); const float pw = a[0] * a[0] + a[1] * a[1] + a[2] * a[2]; if (std::fabs(pw - 1) > 0.01f) okP = false; }
          pp[kTlCurve] = 1;
          for (int s = 0; s <= 100; ++s) { sectionWeights(pp, s / 100.0f, a); if (std::fabs(a[0] + a[1] + a[2] - 1) > 0.01f) okL = false; }
          sectionWeights(pp, 0.05f, a); const bool startA = a[0] > 0.99f;
          sectionWeights(pp, 0.5f, a); const bool midB = a[1] > 0.99f;
          sectionWeights(pp, 0.95f, a); const bool endC = a[2] > 0.99f;
          check(okP && okL && startA && midB && endC, "3 sections: A at the start, B in the middle, C at the end; constant power / linear blends"); }
        // preview from a section: PLAY FROM starts the sample there
        { std::unique_ptr<LumenEngine> v(new LumenEngine()); v->setSampleRate(44100); v->setSample(&src); v->p[kOriginal] = 1;
          v->p[kPlayFrom] = 0.5f; v->p[kPlay] = 1; std::vector<float> P; run(*v, 0.5, &P);
          double e2 = 0, s2 = 0; for (size_t i = 3000; i < 15000; ++i) { const double d = P[i] - src.L[4 * 44100 + i]; e2 += d * d; s2 += (double)src.L[4 * 44100 + i] * src.L[4 * 44100 + i]; }
          check(std::sqrt(e2 / s2) < 0.01, "PLAY FROM 50 %: preview starts in the middle of the sample");
          v->p[kPlayFrom] = 0.0f; v->p[kCue] = 1; std::vector<float> Q; run(*v, 0.5, &Q);
          e2 = 0; s2 = 0; for (size_t i = 3000; i < 15000; ++i) { const double d = Q[i] - src.L[i]; e2 += d * d; s2 += (double)src.L[i] * src.L[i]; }
          check(std::sqrt(e2 / s2) < 0.05, "a new cue restarts playback from the new point while playing"); }
        // render with the timeline
        { float pv[kParamCount]; for (uint32_t i = 0; i < kNumIn; ++i) pv[i] = e->p[i];
          std::vector<float> RL, RR; renderLumenOffline(pv, src, 44100, 120, 1, RL, RR);
          check(RL.size() >= 8 * 44100 && bright_at(RL, 5.5, 7.5) > bright_at(RL, 0.5, 2.5) * 4, "DRAG TO DAW renders the timeline");
          gf::writeWav24(out + "/lumen_timeline.wav", RL, RR, 44100); }
    }
    // CPU with 6 heavy sections all sounding at once is still fine
    {
        std::unique_ptr<LumenEngine> e(new LumenEngine()); e->setSampleRate(48000); e->setSample(&src);
        e->p[kPlay] = 1; e->p[kTlOn] = 1; e->p[kTlCount] = 6;
        for (int k = 0; k < 6; ++k) { e->p[slotParam(k, kSfSound)] = (float)(k * 19 % kNumPresets); e->p[slotParam(k, kSfStart)] = k / 6.0f; e->p[slotParam(k, kSfFade)] = 1; }
        const auto t0 = std::chrono::steady_clock::now(); run(*e, 10.0, nullptr, {}, 48000);
        const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("      timeline with 6 sections: %.1f %% of one core\n", el / 10.0 * 100);
        check(el < 3.0, "timeline CPU is reasonable");
    }
    // ---- 1.2.1 STOP: silence within ~10 ms, for Play AND for keys, including echoes, reverb and frozen grains
    {
        auto find = [](const char* n) { for (int i = 0; i < kNumPresets; ++i) if (!std::strcmp(kPresets[i].name, n)) return i; return -1; };
        bool allSilent = true, smooth = true;
        for (const char* name : { "Infinite", "Event Horizon", "Frozen Pad", "Dub Space", "Vinyl Night" }) {
            std::unique_ptr<LumenEngine> e(new LumenEngine()); e->setSampleRate(44100); e->setSample(&src);
            float pv[kParamCount]; presetValues(find(name), pv); for (uint32_t k = 0; k < kNumIn; ++k) e->p[k] = pv[k];
            e->p[kLoop] = 0; e->p[kPlay] = 1;
            run(*e, 2.0, nullptr, { { 0, 0x90, 67, 100 } });           // Play + a held key
            e->p[kPlay] = 0; e->p[kStop] = e->p[kStop] + 1;          // the STOP button
            std::vector<float> L; run(*e, 2.0, &L);
            float after = 0, step = 0; for (size_t i = 0; i < L.size(); ++i) { if (i > 512) after = std::max(after, std::fabs(L[i])); if (i) step = std::max(step, std::fabs(L[i] - L[i - 1])); }
            float before = 0; { std::unique_ptr<LumenEngine> f(new LumenEngine()); f->setSampleRate(44100); f->setSample(&src); for (uint32_t k = 0; k < kNumIn; ++k) f->p[k] = pv[k]; f->p[kPlay] = 1; std::vector<float> B; run(*f, 2.0, &B); for (size_t i = 1; i < B.size(); ++i) before = std::max(before, std::fabs(B[i] - B[i - 1])); }
            std::printf("      STOP %-14s after 12 ms: %.1e   biggest step in the fade %.3f (playing: %.3f)\n", name, after, step, before);
            if (after > 1e-6f || e->playing()) allSilent = false;
            if (step > before * 1.5f + 0.02f) smooth = false;
        }
        check(allSilent, "STOP silences everything (Play, held keys, echoes, reverb, frozen grains, hiss) within 12 ms");
        check(smooth, "STOP fades in 10 ms instead of clicking");
        // a stop counter that is already set when a project loads does not cut anything
        std::unique_ptr<LumenEngine> e(new LumenEngine()); e->setSampleRate(44100); e->setSample(&src);
        e->p[kOriginal] = 1; e->p[kStop] = 7; e->p[kPlay] = 1; const Stats s = run(*e, 0.5);
        check(s.rms > 0.01, "a saved STOP value does not mute a reloaded project");
        e->p[kPlay] = 0; std::vector<float> L; run(*e, 0.4, &L, { { 0, 0xB0, 120, 0 } });
        float tail = 0; for (size_t i = 1024; i < L.size(); ++i) tail = std::max(tail, std::fabs(L[i]));
        check(tail < 1e-6f, "MIDI All Sound Off (CC 120) also cuts everything");
    }
    // ---- nothing keeps sounding (or builds up) on its own after the sample ends
    {
        std::vector<float> l(256), r(256); TimeInfo ti; ti.valid = true; ti.bpm = 120;
        int bad = 0;
        for (int i = 0; i < kNumPresets; ++i) {
            std::unique_ptr<LumenEngine> e(new LumenEngine()); e->setSampleRate(44100); e->setSample(&src);
            float pv[kParamCount]; presetValues(i, pv); for (uint32_t k = 0; k < kNumIn; ++k) e->p[k] = pv[k];
            e->p[kPlay] = 1; double late = 0;
            for (int pos = 0; pos < 44100 * 25; pos += 256) {
                if (pos >= 44100 * 3) e->p[kPlay] = 0;
                e->process(l.data(), r.data(), 256, nullptr, 0, ti);
                if (pos > 44100 * 23) for (int k = 0; k < 256; ++k) late += (double)l[k] * l[k] + (double)r[k] * r[k];
            }
            late = std::sqrt(late / (44100.0 * 2 * 2));
            if (late > 0.002) { ++bad; std::printf("      still sounding 20 s after the end: %s (%.4f)\n", kPresets[i].name, late); }
        }
        check(bad == 0, "every sound dies out after the sample stops (no howling reverb, no endless frozen hold or hiss)");
    }
    std::printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
