// Lumen — editor UI (NanoVG)

#include "DistrhoUI.hpp"

#include "LumenDSP.hpp"
#include "AudioFile.hpp"
#include "Export.hpp"
#include "res/Fonts.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

START_NAMESPACE_DISTRHO

using namespace lm;

namespace {

constexpr float W = 1100.0f, H = 760.0f;

struct Col { int r, g, b; };
constexpr Col cBg0  { 10, 7, 14 };
constexpr Col cBg1  { 22, 15, 28 };
constexpr Col cCard { 26, 19, 33 };
constexpr Col cCard2{ 35, 26, 44 };
constexpr Col cWell { 14, 10, 19 };
constexpr Col cText { 246, 238, 242 };
constexpr Col cSub  { 182, 160, 178 };
constexpr Col cDim  { 116, 96, 114 };
constexpr Col cRose { 255, 120, 150 };
constexpr Col cPeach{ 255, 196, 128 };
constexpr Col cInk  { 30, 12, 22 };
constexpr Col cMac[4] = { { 255, 120, 150 }, { 255, 170, 110 }, { 190, 140, 255 }, { 110, 210, 255 } };

struct Rect {
    float x, y, w, h;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    float cx() const { return x + w * 0.5f; }
    float cy() const { return y + h * 0.5f; }
};

// header
constexpr Rect rOrig       { 240, 14, 106, 34 };
constexpr Rect rPrev       { 362, 16, 30, 30 };
constexpr Rect rPresetBox  { 396, 16, 300, 30 };
constexpr Rect rNext       { 700, 16, 30, 30 };
constexpr Rect rSurprise   { 738, 16, 92, 30 };
constexpr Rect rPlay       { 842, 11, 40, 40 };
constexpr Rect rDrag       { 890, 12, 122, 38 };
constexpr Rect rLoad       { 1018, 12, 66, 38 };
// waveform
constexpr Rect rWave       { 16, 62, 1068, 126 };
constexpr Rect rWaveIn     { 28, 88, 1044, 76 };
constexpr Rect rPasses     { 650, 68, 64, 18 };
constexpr Rect rLoopBtn    { 720, 68, 76, 18 };
constexpr Rect rKeys       { 808, 68, 264, 18 };
// pages
constexpr Rect rTabSounds  { 16, 198, 150, 26 };
constexpr Rect rTabEdit    { 172, 198, 150, 26 };
constexpr Rect rCats       { 16, 232, 190, 456 };
constexpr Rect rList       { 216, 232, 500, 456 };
constexpr Rect rMacros     { 726, 232, 358, 456 };
constexpr Rect rDryWet     { 744, 592, 322, 24 };
constexpr Rect rOutSl      { 744, 626, 322, 24 };
constexpr Rect rEditBtn    { 744, 658, 322, 22 };
constexpr Rect rChain      { 16, 698, 1068, 50 };
// TIMELINE page
constexpr Rect rTabTl      { 328, 198, 150, 26 };
constexpr Rect rTl         { 16, 232, 1068, 214 };
constexpr Rect rTlOn       { 984, 240, 88, 20 };
constexpr Rect rLanes      { 28, 264, 1044, 140 };
constexpr Rect rAddSec     { 28, 414, 128, 24 };
constexpr Rect rDelSec     { 162, 414, 100, 24 };
constexpr Rect rFadeAll    { 274, 414, 300, 24 };
constexpr Rect rCurve      { 586, 414, 240, 24 };
constexpr Rect rTails      { 836, 414, 236, 24 };
constexpr Rect rPickCard   { 16, 456, 700, 232 };
constexpr Rect rFrom       { 486, 462, 218, 20 };
constexpr Rect rSecCard    { 726, 456, 358, 232 };
inline Rect chipT(int i) { return { 28 + i * 56.5f, 488, 53, 22 }; }
constexpr int kTiles = 20;   // 4 x 5 sounds per category in the timeline picker
inline Rect tileT(int i) { const float w = (676.0f - 18) / 4.0f; return { 28 + (i % 4) * (w + 6), 518 + (i / 4) * 33.0f, w, 30 }; }
inline constexpr const char* kCatShort[] = { "GRAIN", "SPACE", "PADS", "RHYTHM", "LO-FI", "PITCH", "FILTER", "DRIVE", "BREAK", "DREAM", "CINEMA", "SCI-FI" };
enum Page { kPageSounds = 0, kPageEdit, kPageTimeline };

inline Rect catRow(int i) { return { rCats.x + 8, rCats.y + 33 + i * 32.4f, rCats.w - 16, 29 }; }   // i = 0 ALL, 1..12 categories
inline Rect chip(int m) { const float w = (rChain.w - 16 - 8 * 6) / 9.0f; return { rChain.x + 8 + m * (w + 6), rChain.y + 8, w, rChain.h - 16 }; }
inline Rect modCard(int m) { const float w = (1068.0f - 2 * 9) / 3.0f, h = (456.0f - 2 * 9) / 3.0f; return { 16 + (m % 3) * (w + 9), 232 + (m / 3) * (h + 9), w, h }; }
inline Rect macroKnob(int m) { return { m % 2 == 0 ? 815.0f : 995.0f, m < 2 ? 372.0f : 506.0f, 34, 0 }; }

// edit-page layout per module: optional segmented selector + knobs (+ a toggle in the title row)
struct ModLayout { int seg; const char* const* segNames; int segN; uint32_t knobs[7]; int nk; const char* labels[7]; int toggle; const char* toggleLabel; };
const ModLayout kLayouts[9] = {
    { -1, nullptr, 0, { kGMix, kGSize, kGDens, kGPitch, kGSpread, kGRev, kGJit }, 7, { "MIX", "SIZE", "DENSITY", "PITCH", "SPREAD", "REVERSE", "JITTER" }, (int)kGFreeze, "FREEZE" },
    { -1, nullptr, 0, { kPA, kPMixA, kPB, kPMixB, kPDry }, 5, { "VOICE A", "LEVEL A", "VOICE B", "LEVEL B", "DRY" }, -1, nullptr },
    { (int)kDMode, kDriveNames, 5, { kDAmt, kDTone, kDMix }, 3, { "AMOUNT", "TONE", "MIX" }, -1, nullptr },
    { (int)kFType, kFilterNames, 4, { kFCut, kFRes, kFRate, kFDepth, kFEnv }, 5, { "CUTOFF", "RESO", "LFO RATE", "LFO DEPTH", "ENVELOPE" }, -1, nullptr },
    { -1, nullptr, 0, { kLBits, kLRate, kLWow, kLFlutter, kLNoise, kLCrackle, kLAge }, 7, { "BITS", "RATE", "WOW", "FLUTTER", "HISS", "CRACKLE", "AGE" }, -1, nullptr },
    { (int)kMType, kModNames, 7, { kMRate, kMDepth, kMFb, kMMix }, 4, { "RATE", "DEPTH", "FEEDBACK", "MIX" }, -1, nullptr },
    { (int)kSMode, kStutNames, 5, { kSDiv, kSPat, kSAmt, kSMix }, 4, { "DIVISION", "PATTERN", "AMOUNT", "MIX" }, -1, nullptr },
    { -1, nullptr, 0, { kYDiv, kYFb, kYTone, kYMod, kYMix }, 5, { "TIME", "FEEDBACK", "TONE", "WOBBLE", "MIX" }, (int)kYPing, "PING-PONG" },
    { -1, nullptr, 0, { kRSize, kRDecay, kRDamp, kRPre, kRShim, kRMod, kRMix }, 7, { "SIZE", "DECAY", "DAMPING", "PREDELAY", "SHIMMER", "MOTION", "MIX" }, -1, nullptr },
};

inline Rect seg(const Rect& r, int i, int n) { const float w = r.w / n; return { r.x + i * w, r.y, w, r.h }; }

struct Knob { float x, y, r; uint32_t param; int macro; const char* label; };   // macro >= 0: big macro knob

} // namespace

class LumenUI : public UI
{
public:
    LumenUI() : UI((uint)W, (uint)H)
    {
        for (uint32_t i = 0; i < kParamCount; ++i) fV[i] = getParamInfo(i).def;
        { float p[kParamCount]; presetValues(0, p); for (uint32_t i = 0; i < kNumIn; ++i) fV[i] = p[i]; }
        fFontMed  = createFontFromMemory("inter-med",  kFontInterMedium,  kFontInterMediumSize,  false);
        fFontSemi = createFontFromMemory("inter-semi", kFontInterSemiBold, kFontInterSemiBoldSize, false);
        fFontBold = createFontFromMemory("inter-bold", kFontInterBold,    kFontInterBoldSize,    false);
        const double sf = getScaleFactor();
        if (sf > 0.0 && std::fabs(sf - 1.0) > 0.01) setSize((uint)std::lround(W * sf), (uint)std::lround(H * sf));
        setGeometryConstraints((uint)(W * 0.55f), (uint)(H * 0.55f), true, false);
        updateScale(getWidth(), getHeight());
        fSample.reset(new SampleData()); makeLumenSource(*fSample, 44100.0); fName = "Built-in \xC2\xB7 piano & beat"; fBuiltin = true;
        computePeaks();
        buildKnobs();
    }

protected:
    // =========================================================================================
    void parameterChanged(uint32_t i, float v) override
    {
        if (i >= kParamCount || fV[i] == v) return;
        fV[i] = v;
        if (i == kTlCount && fSel >= secCount()) selectSection(secCount() - 1);
        if (i != kOutLevel && i != kOutPos && i != kOutBpm && i != kOutSection) repaint();
    }

    void programLoaded(uint32_t index) override
    {
        float p[kParamCount]; presetValues((int)index, p);
        for (uint32_t i = 0; i < kNumIn; ++i) if (!presetKeeps(i)) fV[i] = p[i];
        repaint();
    }

    void stateChanged(const char* key, const char* value) override
    {
        if (std::strcmp(key, "sample")) return;
        if (!value || !*value) { fSample.reset(new SampleData()); makeLumenSource(*fSample, 44100.0); fName = "Built-in \xC2\xB7 piano & beat"; fBuiltin = true; }
        else {
            std::unique_ptr<SampleData> ns(new SampleData());
            if (gf::loadAudioFile(value, *ns)) { fSample.swap(ns); fName = gf::fileNameOf(value); fBuiltin = false; }
            else toast("Could not load " + gf::fileNameOf(value));
        }
        computePeaks();
        repaint();
    }

    void uiIdle() override
    {
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now - fLastIdle < 0.030) return;
        const float dt = (float)std::min(0.1, now - fLastIdle);
        fLastIdle = now;
        fPulse += dt;
        if (fStopHold > 0) { fStopHold -= dt; if (fStopHold <= 0) repaint(); }
        if (fToastTime > 0) fToastTime = std::max(0, fToastTime - (int)std::lround(dt * 30));
        // a one-shot that finished: pop the Play button back out
        if (fV[kPlay] > 0.5f && fV[kOutPlaying] < 0.5f) { fEndTimer += dt; if (fEndTimer > 0.35f) { setParam(kPlay, 0.0f, true); fEndTimer = 0; } }
        else fEndTimer = 0;
        fGlow += (std::min(1.0f, fV[kOutLevel] * 1.6f) - fGlow) * 0.25f;
        if (fV[kPlay] > 0.5f || fGlow > 0.01f || fToastTime > 0 || fV[kOutPlaying] > 0.5f) repaint();
    }

    void uiScaleFactorChanged(const double sf) override { if (sf > 0) setSize((uint)std::lround(W * sf), (uint)std::lround(H * sf)); }
    void onResize(const ResizeEvent& ev) override { updateScale(ev.size.getWidth(), ev.size.getHeight()); UI::onResize(ev); }

    // =========================================================================================
    // drawing helpers
    void fillC(Col c, int a = 255) { fillColor(c.r, c.g, c.b, a); }
    void strokeC(Col c, int a = 255) { strokeColor(c.r, c.g, c.b, a); }
    void txt(float x, float y, const char* t, Col c, float size, int align, FontId f, float sp = 0, int a = 255)
    { fontFaceId(f); fontSize(size); textLetterSpacing(sp); textAlign(align); fillC(c, a); text(x, y, t, nullptr); }
    void accent(float x0, float y0, float x1, float y1, float a = 1.0f) { fillPaint(linearGradient(x0, y0, x1, y1, Color(255, 120, 150, a), Color(255, 196, 128, a))); }
    void card(const Rect& r, float rad = 14)
    {
        beginPath(); rect(r.x - 12, r.y - 8, r.w + 24, r.h + 26); roundedRect(r.x, r.y, r.w, r.h, rad); pathWinding(CW);
        fillPaint(boxGradient(r.x, r.y + 4, r.w, r.h, rad, 14, Color(0, 0, 0, 0.55f), Color(0, 0, 0, 0.0f))); fill();
        beginPath(); roundedRect(r.x, r.y, r.w, r.h, rad);
        fillPaint(linearGradient(r.x, r.y, r.x, r.y + r.h, Color(cCard2.r, cCard2.g, cCard2.b), Color(cCard.r, cCard.g, cCard.b))); fill();
        strokeColor(255, 255, 255, 14); strokeWidth(1); stroke();
        beginPath(); moveTo(r.x + rad, r.y + 0.5f); lineTo(r.x + r.w - rad, r.y + 0.5f); strokeColor(255, 255, 255, 26); stroke();
    }
    void title(float x, float y, const char* t, Col c = cSub) { txt(x, y, t, c, 9.5f, ALIGN_LEFT | ALIGN_MIDDLE, fFontSemi, 1.7f); }
    void pill(const Rect& r, const char* t, bool active, float fs = 10, Col tc = cText)
    {
        beginPath(); roundedRect(r.x, r.y, r.w, r.h, std::min(10.0f, r.h * 0.5f));
        if (active) { accent(r.x, r.y, r.x + r.w, r.y + r.h); fill(); }
        else { fillPaint(linearGradient(r.x, r.y, r.x, r.y + r.h, Color(44, 33, 54), Color(30, 22, 38))); fill(); strokeColor(255, 255, 255, 20); strokeWidth(1); stroke(); }
        if (t) txt(r.cx(), r.cy() + 0.5f, t, active ? cInk : tc, fs, ALIGN_CENTER | ALIGN_MIDDLE, fFontSemi, 0.8f);
    }
    void segmented(const Rect& r, const char* const* labels, int n, int active, float fs)
    {
        beginPath(); roundedRect(r.x, r.y, r.w, r.h, std::min(9.0f, r.h * 0.5f)); fillC(cWell); fill(); strokeColor(255, 255, 255, 12); strokeWidth(1); stroke();
        for (int i = 0; i < n; ++i) {
            const Rect s = seg(r, i, n);
            if (i == active) {
                beginPath(); roundedRect(s.x + 2, s.y + 2, s.w - 4, s.h - 4, std::min(7.0f, (s.h - 4) * 0.5f)); accent(s.x, s.y, s.x + s.w, s.y + s.h); fill();
                txt(s.cx(), s.cy() + 0.5f, labels[i], cInk, fs, ALIGN_CENTER | ALIGN_MIDDLE, fFontBold, 0.6f);
            } else txt(s.cx(), s.cy() + 0.5f, labels[i], cSub, fs, ALIGN_CENTER | ALIGN_MIDDLE, fFontSemi, 0.6f);
        }
    }
    void wrapText(float x, float y, float w, const char* t, Col c, float size, FontId f, float lh)
    {
        fontFaceId(f); fontSize(size); textLetterSpacing(0.2f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillC(c);
        textBox(x, y, w, t, nullptr);
        (void)lh;
    }

    // =========================================================================================
    void onNanoDisplay() override
    {
        const float sw = getWidth() / fScale, sh = getHeight() / fScale;
        scale(fScale, fScale); translate(fOffX, fOffY);
        beginPath(); rect(-fOffX, -fOffY, sw, sh);
        fillPaint(linearGradient(0, 0, 0, H, Color(cBg1.r, cBg1.g, cBg1.b), Color(cBg0.r, cBg0.g, cBg0.b))); fill();
        beginPath(); rect(0, 0, W, H);
        fillPaint(radialGradient(W * 0.72f, 120, 40, 680, Color(255, 120, 150, 0.06f + 0.05f * fGlow), Color(0, 0, 0, 0))); fill();

        drawHeader();
        drawWave();
        pill(rTabSounds, "SOUNDS", fPage == kPageSounds, 9.5f);
        pill(rTabEdit, "EDIT", fPage == kPageEdit, 9.5f);
        pill(rTabTl, tlActive() ? "TIMELINE \xC2\xB7 ON" : "TIMELINE", fPage == kPageTimeline, 9.5f);
        if (tlActive() && fPage != kPageTimeline) { beginPath(); circle(rTabTl.x + rTabTl.w - 12, rTabTl.cy(), 3.5f); fillC(cPeach); fill(); }
        static const char* hints[3] = { "pick a category, click a sound, shape it with the four big knobs",
                                        "every module of the current sound \xC2\xB7 knobs with a colored dot are controlled by that macro",
                                        "fade from one sound to the next while the sample plays" };
        txt(rTabTl.x + rTabTl.w + 14, rTabSounds.cy() + 0.5f, hints[fPage], cDim, 9, ALIGN_LEFT | ALIGN_MIDDLE, fFontMed, 0.3f);
        if (fPage == kPageEdit) drawEdit(); else if (fPage == kPageTimeline) drawTimeline(); else drawBrowser();
        drawChain();
        if (fV[kOriginal] > 0.5f) {
            beginPath(); rect(16, 232, 1068, 516); fillColor(12, 8, 16, 180); fill();
            txt(W * 0.5f, 450, "ORIGINAL SOUND IS ON", cPeach, 15, ALIGN_CENTER | ALIGN_MIDDLE, fFontBold, 2.2f);
            txt(W * 0.5f, 474, "you hear the sample exactly as it is \xC2\xB7 click ORIGINAL again to hear the effect", cText, 10.5f, ALIGN_CENTER | ALIGN_MIDDLE, fFontMed, 0.3f);
        }
    }

    void drawLogo(float x, float y)
    {
        for (int i = 0; i < 3; ++i) {
            const float r = 12.0f - i * 3.5f;
            beginPath(); circle(x, y, r); strokeColor(255, 140 + i * 25, 150 - i * 10, 120 + i * 50); strokeWidth(1.6f); stroke();
        }
        beginPath(); circle(x, y, 3.2f); accent(x - 3, y - 3, x + 3, y + 3); fill();
        beginPath(); circle(x, y, 16); fillPaint(radialGradient(x, y, 2, 18, Color(255, 150, 160, 0.25f + 0.25f * fGlow), Color(0, 0, 0, 0))); fill();
    }

    void drawHeader()
    {
        drawLogo(32, 31);
        txt(58, 29, "LUMEN", cText, 19, ALIGN_LEFT | ALIGN_MIDDLE, fFontBold, 4.0f);
        txt(59, 48, "SAMPLE FX INSTRUMENT  \xC2\xB7  1.2.1", cDim, 7.5f, ALIGN_LEFT | ALIGN_MIDDLE, fFontSemi, 1.6f);
        {
            const bool on = fV[kOriginal] > 0.5f;
            beginPath(); roundedRect(rOrig.x, rOrig.y, rOrig.w, rOrig.h, 11);
            if (on) accent(rOrig.x, rOrig.y, rOrig.x + rOrig.w, rOrig.y + rOrig.h); else fillC(cWell);
            fill(); strokeColor(255, 255, 255, on ? 70 : 22); strokeWidth(1); stroke();
            txt(rOrig.cx(), rOrig.y + 13, "ORIGINAL", on ? cInk : cText, 10, ALIGN_CENTER | ALIGN_MIDDLE, fFontBold, 1.4f);
            txt(rOrig.cx(), rOrig.y + 25, on ? "SOUND \xC2\xB7 ON" : "SOUND \xC2\xB7 OFF", on ? cInk : cDim, 7, ALIGN_CENTER | ALIGN_MIDDLE, fFontSemi, 1.2f);
        }
        pill(rPrev, "\xE2\x80\xB9", false, 15); pill(rNext, "\xE2\x80\xBA", false, 15);
        beginPath(); roundedRect(rPresetBox.x, rPresetBox.y, rPresetBox.w, rPresetBox.h, 10); fillC(cWell); fill(); strokeColor(255, 255, 255, 18); strokeWidth(1); stroke();
        const Preset& pr = presetAt(tlActive() ? secSound(fSel) : cur());
        if (tlActive()) { char b[32]; std::snprintf(b, sizeof b, "TIMELINE \xC2\xB7 SECTION %d", fSel + 1); txt(rPresetBox.x + 12, rPresetBox.cy() + 0.5f, b, secCol(fSel), 7.5f, ALIGN_LEFT | ALIGN_MIDDLE, fFontBold, 1.2f); }
        else txt(rPresetBox.x + 12, rPresetBox.cy() + 0.5f, kCatNames[pr.cat], cRose, 7.5f, ALIGN_LEFT | ALIGN_MIDDLE, fFontBold, 1.2f);
        txt(rPresetBox.x + rPresetBox.w - 12, rPresetBox.cy() + 0.5f, pr.name, cText, 12, ALIGN_RIGHT | ALIGN_MIDDLE, fFontSemi, 0.4f);
        pill(rSurprise, "SURPRISE", false, 9.5f);
        // play
        const bool playing = soundingNow();
        const float pcx = rPlay.cx(), pcy = rPlay.cy();
        if (playing) { beginPath(); circle(pcx, pcy, 30); fillPaint(radialGradient(pcx, pcy, 10, 30, Color(255, 140, 150, 0.25f + 0.15f * fGlow), Color(0, 0, 0, 0))); fill(); }
        beginPath(); circle(pcx, pcy, 19);
        if (playing) accent(pcx - 19, pcy - 19, pcx + 19, pcy + 19); else fillPaint(linearGradient(pcx, pcy - 19, pcx, pcy + 19, Color(52, 40, 62), Color(30, 22, 38)));
        fill(); strokeColor(255, 255, 255, playing ? 80 : 30); strokeWidth(1); stroke();
        if (playing) { beginPath(); roundedRect(pcx - 6, pcy - 6, 12, 12, 2); fillC(cInk); fill(); }
        else { beginPath(); moveTo(pcx - 5, pcy - 8); lineTo(pcx + 8, pcy); lineTo(pcx - 5, pcy + 8); closePath(); fillC(cPeach); fill(); }
        // drag tile
        beginPath(); roundedRect(rDrag.x, rDrag.y, rDrag.w, rDrag.h, 11);
        fillPaint(linearGradient(rDrag.x, rDrag.y, rDrag.x, rDrag.y + rDrag.h, Color(50, 36, 58), Color(30, 22, 38))); fill();
        strokeColor(255, 150, 160, fDragPressed ? 200 : 70); strokeWidth(1.2f); stroke();
        for (int i = 0; i < 9; ++i) { const float hgt = 4 + 10 * std::fabs(std::sin(i * 1.7f + 0.6f)); beginPath(); roundedRect(rDrag.x + 10 + i * 3.2f, rDrag.cy() - hgt * 0.5f, 2, hgt, 1); fillC(cRose, 200); fill(); }
        txt(rDrag.x + 44, rDrag.y + 14, gf::canDragFiles() ? "DRAG TO DAW" : "EXPORT WAV", cText, 9.5f, ALIGN_LEFT | ALIGN_MIDDLE, fFontBold, 1.0f);
        char info[40]; std::snprintf(info, sizeof info, "%d\xC3\x97 sample + tail \xC2\xB7 wav", fPasses);
        txt(rDrag.x + 44, rDrag.y + 27, info, cDim, 7.5f, ALIGN_LEFT | ALIGN_MIDDLE, fFontMed, 0.3f);
        pill(rLoad, "LOAD", true, 10.5f);
    }

    void drawWave()
    {
        card(rWave);
        const Rect& r = rWaveIn;
        beginPath(); roundedRect(r.x - 4, r.y - 2, r.w + 8, r.h + 4, 8); fillC(cWell, 220); fill();
        txt(rWave.x + 14, rWave.y + 14, fName.c_str(), cText, 10.5f, ALIGN_LEFT | ALIGN_MIDDLE, fFontSemi, 0.2f);
        if (fSample && !fSample->empty()) {
            char b[64]; std::snprintf(b, sizeof b, "%.2f s  \xC2\xB7  %.1f kHz", fSample->frames() / fSample->rate, fSample->rate / 1000.0);
            fontFaceId(fFontSemi); fontSize(10.5f); textLetterSpacing(0.2f); Rectangle<float> tb; textBounds(0, 0, fName.c_str(), nullptr, tb);
            txt(rWave.x + 26 + tb.getWidth(), rWave.y + 14, b, cDim, 9, ALIGN_LEFT | ALIGN_MIDDLE, fFontMed, 0.3f);
        }
        static const char* passN[3] = { "LEN 1\xC3\x97", "LEN 2\xC3\x97", "LEN 4\xC3\x97" };
        pill(rPasses, passN[fPasses == 4 ? 2 : fPasses - 1], false, 8);
        pill(rLoopBtn, fV[kLoop] > 0.5f ? "LOOP ON" : "LOOP OFF", fV[kLoop] > 0.5f, 8);
        { const Rect k { rKeys.x + 44, rKeys.y, rKeys.w - 44, rKeys.h };
          txt(rKeys.x, rKeys.cy() + 0.5f, "KEYS", cDim, 7.5f, ALIGN_LEFT | ALIGN_MIDDLE, fFontBold, 1.3f);
          static const char* kn[2] = { "PITCH (sampler)", "TRIGGER (same pitch)" };
          segmented(k, kn, 2, (int)fV[kKeys], 7.5f); }

        const float mid = r.y + r.h * 0.5f, amp = r.h * 0.46f;
        const float x0 = r.x + std::min(fV[kRegStart], fV[kRegEnd]) * r.w, x1 = r.x + std::max(fV[kRegStart], fV[kRegEnd]) * r.w;
        if (!fPeaks.empty()) {
            const int cols = (int)fPeaks.size();
            beginPath();
            for (int i = 0; i < cols; ++i) { const float x = r.x + (i + 0.5f) * r.w / cols; const float y = mid - fPeaks[(size_t)i] * amp; if (i == 0) moveTo(x, y); else lineTo(x, y); }
            for (int i = cols - 1; i >= 0; --i) lineTo(r.x + (i + 0.5f) * r.w / cols, mid + fPeaks[(size_t)i] * amp);
            closePath();
            fillPaint(linearGradient(0, r.y, 0, r.y + r.h, Color(255, 170, 160, 0.95f), Color(200, 90, 140, 0.8f))); fill();
        }
        beginPath(); rect(r.x - 4, r.y - 2, x0 - r.x + 4, r.h + 4); rect(x1, r.y - 2, r.x + r.w - x1 + 4, r.h + 4); fillColor(10, 7, 14, 190); fill();
        if (tlActive()) {   // the timeline sections as a colored strip under the waveform
            for (int s = 0; s < secCount(); ++s) {
                const float a = x0 + (x1 - x0) * secStart(s), b = x0 + (x1 - x0) * secEnd(s);
                beginPath(); rect(a, r.y + r.h - 4, b - a - (s + 1 < secCount() ? 1 : 0), 4); fillC(secCol(s), 230); fill();
            }
        }
        for (int h = 0; h < 2; ++h) {
            const float hx = h == 0 ? x0 : x1;
            beginPath(); moveTo(hx, r.y - 2); lineTo(hx, r.y + r.h + 2); strokeC(cPeach, 220); strokeWidth(1.5f); stroke();
            beginPath(); moveTo(hx - 6, r.y - 8); lineTo(hx + 6, r.y - 8); lineTo(hx, r.y - 1); closePath(); fillC(cPeach); fill();
        }
        if (fV[kOutPlaying] > 0.5f) {
            const float px = r.x + fV[kOutPos] * r.w;
            beginPath(); moveTo(px, r.y); lineTo(px, r.y + r.h); strokeColor(255, 255, 255, 60); strokeWidth(7); stroke();
            beginPath(); moveTo(px, r.y); lineTo(px, r.y + r.h); strokeColor(255, 255, 255, 230); strokeWidth(1.5f); stroke();
        }
        if (fToastTime > 0) txt(rWave.x + rWave.w - 14, rWave.y + rWave.h - 9, fToast.c_str(), cPeach, 10, ALIGN_RIGHT | ALIGN_MIDDLE, fFontSemi, 0.3f, std::min(255, fToastTime * 12));
        else txt(rWave.x + rWave.w - 14, rWave.y + rWave.h - 9, "click: play from there \xC2\xB7 right-click: move start / end there \xC2\xB7 drag the \xE2\x96\xBC handles \xC2\xB7 right-click a \xE2\x96\xBC: reset \xC2\xB7 MIDI keys play it too",
                 cDim, 8.5f, ALIGN_RIGHT | ALIGN_MIDDLE, fFontMed, 0.3f);
    }

    // ---- SOUNDS page
    int catCount(int c) const { int n = 0; for (int i = 0; i < kNumPresets; ++i) if (c < 0 || kPresets[i].cat == c) ++n; return n; }
    int listIndex(int k) const { int n = 0; for (int i = 0; i < kNumPresets; ++i) if (fCat < 0 || kPresets[i].cat == fCat) { if (n == k) return i; ++n; } return -1; }
    Rect tile(int k) const {   // k-th visible preset tile (after scroll)
        const float w = (rList.w - 24 - 8) / 2.0f;
        const int row = k / 2, col = k % 2;
        return { rList.x + 12 + col * (w + 8), rList.y + 38 + row * 46.0f - fScrollRows * 46.0f, w, 40 };
    }
    void drawBrowser()
    {
        card(rCats); title(rCats.x + 14, rCats.y + 17, "CATEGORIES");
        for (int i = 0; i <= kNumCats; ++i) {
            const Rect rr = catRow(i);
            const int c = i - 1;
            const bool sel = fCat == c;
            const bool hasCur = c >= 0 && presetAt(tlActive() ? secSound(fSel) : cur()).cat == c;
            beginPath(); roundedRect(rr.x, rr.y, rr.w, rr.h, 9);
            if (sel) { accent(rr.x, rr.y, rr.x + rr.w, rr.y + rr.h, 0.95f); fill(); }
            else if (fHoverCat == i) { fillColor(255, 255, 255, 12); fill(); }
            txt(rr.x + 12, rr.cy() + 0.5f, c < 0 ? "ALL SOUNDS" : kCatNames[c], sel ? cInk : cText, 9, ALIGN_LEFT | ALIGN_MIDDLE, fFontBold, 0.9f);
            char b[8]; std::snprintf(b, sizeof b, "%d", catCount(c));
            txt(rr.x + rr.w - 12, rr.cy() + 0.5f, b, sel ? cInk : cDim, 8.5f, ALIGN_RIGHT | ALIGN_MIDDLE, fFontSemi, 0.3f);
            if (hasCur && !sel) { beginPath(); circle(rr.x + rr.w - 30, rr.cy(), 3); fillC(cRose); fill(); }
        }
        card(rList);
        char t[64]; std::snprintf(t, sizeof t, "%s  \xC2\xB7  %d SOUNDS", fCat < 0 ? "ALL SOUNDS" : kCatNames[fCat], catCount(fCat));
        title(rList.x + 14, rList.y + 17, t);
        if (catCount(fCat) > 18) txt(rList.x + rList.w - 14, rList.y + 17, "scroll for more", cDim, 8, ALIGN_RIGHT | ALIGN_MIDDLE, fFontMed, 0.3f);
        save(); scissor(rList.x + 4, rList.y + 32, rList.w - 8, rList.h - 38);
        const int n = catCount(fCat);
        for (int k = 0; k < n; ++k) {
            const Rect tr = tile(k);
            if (tr.y + tr.h < rList.y + 30 || tr.y > rList.y + rList.h) continue;
            const int idx = listIndex(k);
            const bool sel = idx == (tlActive() ? secSound(fSel) : cur()), hov = idx == fHoverTile;
            beginPath(); roundedRect(tr.x, tr.y, tr.w, tr.h, 10);
            if (sel) { accent(tr.x, tr.y, tr.x + tr.w, tr.y + tr.h); fill(); }
            else { fillColor(255, 255, 255, hov ? 22 : 9); fill(); strokeColor(255, 255, 255, hov ? 40 : 14); strokeWidth(1); stroke(); }
            txt(tr.x + 12, tr.y + 15, kPresets[idx].name, sel ? cInk : cText, 10.5f, ALIGN_LEFT | ALIGN_MIDDLE, fFontSemi, 0.3f);
            txt(tr.x + 12, tr.y + 29, fCat < 0 ? kCatNames[kPresets[idx].cat] : moduleSummary(idx).c_str(), sel ? Col{ 90, 40, 60 } : cDim, 7.5f, ALIGN_LEFT | ALIGN_MIDDLE, fFontMed, 0.6f);
        }
        restore();
        // macro panel
        card(rMacros);
        if (tlActive()) {
            // the timeline decides the sound: clicking a sound here gives it to the selected section
            const Col sc = secCol(fSel);
            txt(rMacros.x + 16, rMacros.y + 22, "TIMELINE IS ON", sc, 13, ALIGN_LEFT | ALIGN_MIDDLE, fFontBold, 1.6f);
            char b[96]; std::snprintf(b, sizeof b, "Sounds you click here go to section %d (%s).", fSel + 1, kPresets[secSound(fSel)].name);
            wrapText(rMacros.x + 16, rMacros.y + 40, rMacros.w - 32, b, cText, 10, fFontMed, 14);
            wrapText(rMacros.x + 16, rMacros.y + 76, rMacros.w - 32, "Each section's four knobs, its fades and its play buttons are on the TIMELINE page.", cSub, 10, fFontMed, 14);
            const int n = secCount();
            for (int s = 0; s < n; ++s) {
                const Rect rr { rMacros.x + 16, rMacros.y + 124 + s * 34.0f, rMacros.w - 32, 28 };
                beginPath(); roundedRect(rr.x, rr.y, rr.w, rr.h, 8); fillColor(kSecCol[s][0], kSecCol[s][1], kSecCol[s][2], s == fSel ? 220 : 40); fill();
                char t[80]; std::snprintf(t, sizeof t, "%d   %s", s + 1, kPresets[secSound(s)].name);
                txt(rr.x + 12, rr.cy() + 0.5f, t, s == fSel ? cInk : cText, 10, ALIGN_LEFT | ALIGN_MIDDLE, fFontSemi, 0.3f);
            }
            pill(rEditBtn, "OPEN THE TIMELINE  \xE2\x80\xBA", true, 8.5f);
            slider(rDryWet, kDryWet, "DRY / WET");
            slider(rOutSl, kOut, "OUTPUT");
            return;
        }
        const Preset& pr = presetAt(cur());
        txt(rMacros.x + 16, rMacros.y + 22, pr.name, cText, 16, ALIGN_LEFT | ALIGN_MIDDLE, fFontBold, 0.4f);
        txt(rMacros.x + 16, rMacros.y + 40, kCatNames[pr.cat], cRose, 8, ALIGN_LEFT | ALIGN_MIDDLE, fFontBold, 1.4f);
        wrapText(rMacros.x + 16, rMacros.y + 54, rMacros.w - 32, pr.desc, cSub, 10, fFontMed, 14);
        for (size_t i = 0; i < fKnobs.size(); ++i) if (fKnobs[i].macro >= 0 && fKnobs[i].macro < 10) drawKnob((int)i);
        slider(rDryWet, kDryWet, "DRY / WET");
        slider(rOutSl, kOut, "OUTPUT");
        pill(rEditBtn, "EDIT THIS SOUND  \xE2\x80\xBA", false, 8.5f);
    }
    std::string moduleSummary(int idx) const {
        const Preset& pr = kPresets[idx]; std::string s;
        for (int m = 0; m < 9; ++m) for (int k = 0; k < pr.nVals; ++k) if (pr.vals[k].param == kModules[m].on && pr.vals[k].v > 0.5f) { if (!s.empty()) s += " \xC2\xB7 "; s += kModules[m].name; }
        return s;
    }
    void slider(const Rect& r, uint32_t param, const char* label)
    {
        const float nrm = toNorm(param, fV[param]);
        beginPath(); roundedRect(r.x, r.y, r.w, r.h, r.h * 0.5f); fillC(cWell); fill(); strokeColor(255, 255, 255, 14); strokeWidth(1); stroke();
        beginPath(); roundedRect(r.x, r.y, std::max(r.h, r.w * nrm), r.h, r.h * 0.5f); accent(r.x, 0, r.x + r.w, 0, 0.9f); fill();
        char b[48];
        if (param == kOut) std::snprintf(b, sizeof b, "%s  %+.1f dB", label, fV[param]);
        else std::snprintf(b, sizeof b, "%s  %.0f %%", label, fV[param] * 100);
        txt(r.cx(), r.cy() + 0.5f, b, nrm > 0.5f ? cInk : cText, 9, ALIGN_CENTER | ALIGN_MIDDLE, fFontBold, 1.0f);
    }

    // ---- EDIT page
    void drawEdit()
    {
        float q[kParamCount]; applyMacros(fV, q);
        for (int m = 0; m < 9; ++m) {
            const Rect c = modCard(m);
            const bool on = fV[kModules[m].on] > 0.5f;
            card(c);
            if (on) { beginPath(); roundedRect(c.x + 0.5f, c.y + 0.5f, c.w - 1, c.h - 1, 14); strokeColor(255, 140, 150, 80); strokeWidth(1.2f); stroke(); }
            char t[16]; std::snprintf(t, sizeof t, "%d  %s", m + 1, kModules[m].name);
            title(c.x + 14, c.y + 16, t, on ? cText : cSub);
            const Rect pw = powerRect(m);
            beginPath(); roundedRect(pw.x, pw.y, pw.w, pw.h, pw.h * 0.5f); if (on) accent(pw.x, pw.y, pw.x + pw.w, pw.y); else fillC(cWell); fill();
            strokeColor(255, 255, 255, on ? 60 : 24); strokeWidth(1); stroke();
            beginPath(); circle(on ? pw.x + pw.w - 8 : pw.x + 8, pw.cy(), 5); fillColor(on ? 40 : 160, on ? 15 : 140, on ? 30 : 160); fill();
            const ModLayout& L = kLayouts[m];
            if (L.toggle >= 0) {
                const Rect tr = toggleRect(m);
                pill(tr, L.toggleLabel, fV[L.toggle] > 0.5f, 7.5f);
            }
            if (L.seg >= 0) segmented(segRect(m), L.segNames, L.segN, (int)fV[L.seg], L.segN > 5 ? 7.0f : 8.0f);
            if (!on) { beginPath(); roundedRect(c.x, c.y + 28, c.w, c.h - 28, 12); fillColor(20, 14, 26, 150); fill(); }
        }
        for (size_t i = 0; i < fKnobs.size(); ++i) if (fKnobs[i].macro < 0) drawKnob((int)i, q);
        if (tlActive()) {
            beginPath(); rect(16, 232, 1068, 456); fillColor(12, 8, 16, 190); fill();
            txt(W * 0.5f, 440, "THE TIMELINE IS ON", cPeach, 14, ALIGN_CENTER | ALIGN_MIDDLE, fFontBold, 2.0f);
            txt(W * 0.5f, 464, "each section plays its own sound \xC2\xB7 EDIT changes the single sound, so switch the timeline off (TIMELINE page, ON button) to edit here",
                cText, 10, ALIGN_CENTER | ALIGN_MIDDLE, fFontMed, 0.3f);
        }
    }
    Rect powerRect(int m) const { const Rect c = modCard(m); return { c.x + c.w - 52, c.y + 8, 40, 18 }; }
    Rect toggleRect(int m) const { const Rect c = modCard(m); return { c.x + c.w - 144, c.y + 8, 84, 18 }; }
    Rect segRect(int m) const { const Rect c = modCard(m); return { c.x + 12, c.y + 32, c.w - 24, 20 }; }

    // ---- chain row
    void drawChain()
    {
        card(rChain);
        // with the timeline on, show the modules of the section that is playing (or selected)
        float pv[kParamCount]; const float* src = fV;
        if (tlActive()) { const int s = fV[kOutPlaying] > 0.5f ? (int)fV[kOutSection] : fSel; presetValues(secSound(s), pv); src = pv; }
        for (int m = 0; m < 9; ++m) {
            const Rect r = chip(m);
            const bool on = src[kModules[m].on] > 0.5f;
            beginPath(); roundedRect(r.x, r.y, r.w, r.h, 9);
            if (on) { accent(r.x, r.y, r.x + r.w, r.y + r.h, 0.9f); fill(); }
            else { fillC(cWell); fill(); strokeColor(255, 255, 255, 14); strokeWidth(1); stroke(); }
            txt(r.cx(), r.cy() + 0.5f, kModules[m].name, on ? cInk : cDim, 8.5f, ALIGN_CENTER | ALIGN_MIDDLE, fFontBold, 1.2f);
            if (m < 8) { const float ax = r.x + r.w + 3; beginPath(); moveTo(ax - 1.5f, r.cy() - 3); lineTo(ax + 1.5f, r.cy()); lineTo(ax - 1.5f, r.cy() + 3); strokeColor(255, 255, 255, 50); strokeWidth(1); stroke(); }
        }
    }

    // ---- knobs
    void buildKnobs()
    {
        fKnobs.clear();
        static const char* mlab[4] = { "M1", "M2", "M3", "M4" };
        for (int m = 0; m < 4; ++m) { const Rect k = macroKnob(m); fKnobs.push_back({ k.x, k.y, k.w, kM1 + (uint32_t)m, m, mlab[m] }); }
        for (int m = 0; m < 4; ++m) fKnobs.push_back({ rSecCard.x + rSecCard.w / 4.0f * (m + 0.5f), rSecCard.y + 146, 25, slotParam(fSel, kSfM1 + m), 10 + m, mlab[m] });
        for (int m = 0; m < 9; ++m) {
            const Rect c = modCard(m);
            const ModLayout& L = kLayouts[m];
            const float y = c.y + (L.seg >= 0 ? 98.0f : 84.0f);
            const float inner = c.w - 12;
            for (int k = 0; k < L.nk; ++k) fKnobs.push_back({ c.x + 6 + inner / L.nk * (k + 0.5f), y, L.nk > 5 ? 14.0f : 16.0f, L.knobs[k], -1, L.labels[k] });
        }
    }
    bool knobVisible(const Knob& k) const {
        if (k.macro >= 10) return fPage == kPageTimeline;                 // the selected section's four knobs
        if (k.macro >= 0) return fPage == kPageSounds && !tlActive();     // the single sound's four knobs
        return fPage == kPageEdit;
    }
    int knobAt(float x, float y) const {
        for (size_t i = 0; i < fKnobs.size(); ++i) {
            const Knob& k = fKnobs[i]; if (!knobVisible(k)) continue;
            const float dx = x - k.x, dy = y - k.y; if (dx * dx + dy * dy <= (k.r + 8) * (k.r + 8)) return (int)i;
        }
        return -1;
    }
    void formatValue(uint32_t idx, float v, char* b, size_t n) const
    {
        switch (idx) {
        case kGSize: case kRPre: std::snprintf(b, n, "%.0f ms", v); return;
        case kGDens: std::snprintf(b, n, "%.1f /s", v); return;
        case kGPitch: case kPA: case kPB: std::snprintf(b, n, std::fabs(v - std::round(v)) < 0.01f ? "%+.0f st" : "%+.2f st", v); return;
        case kFCut: if (v >= 1000) std::snprintf(b, n, "%.1f kHz", v / 1000); else std::snprintf(b, n, "%.0f Hz", v); return;
        case kFRate: case kMRate: std::snprintf(b, n, v < 1 ? "%.2f Hz" : "%.1f Hz", v); return;
        case kFEnv: std::snprintf(b, n, "%+.0f %%", v * 100); return;
        case kLBits: std::snprintf(b, n, "%.1f bit", v); return;
        case kLRate: std::snprintf(b, n, "%.1f kHz", v / 1000); return;
        case kRDecay: std::snprintf(b, n, "%.1f s", v); return;
        case kSDiv: std::snprintf(b, n, "%s", kStutDivNames[std::min(6, std::max(0, (int)v))]); return;
        case kSPat: std::snprintf(b, n, "pattern %d", (int)v + 1); return;
        case kYDiv: std::snprintf(b, n, "%s", kDelayNames[std::min(8, std::max(0, (int)v))]); return;
        case kMFb: case kYFb: std::snprintf(b, n, "%.0f %%", v * 100); return;
        default: std::snprintf(b, n, "%.0f %%", v * 100); return;
        }
    }
    void drawKnob(int i, const float* q = nullptr)
    {
        const Knob& k = fKnobs[(size_t)i];
        const bool isMacro = k.macro >= 0;
        const int drivenBy = isMacro ? -1 : macroFor(fV, k.param);
        const float v = (q && drivenBy >= 0) ? q[k.param] : fV[k.param];
        const float nrm = clampf(toNorm(k.param, v), 0, 1);
        const ParamInfo in = getParamInfo(k.param);
        const bool bip = in.min < 0 && in.max > 0;
        const float R = k.r, cx = k.x, cy = k.y;
        const bool hot = fDragKnob == i || fHoverKnob == i;
        const float a0 = 3.14159265f * 0.75f, a1 = 3.14159265f * 2.25f, av = a0 + (a1 - a0) * nrm;
        const int mi = k.macro >= 10 ? k.macro - 10 : k.macro;
        const Col ac = isMacro ? cMac[mi] : (drivenBy >= 0 ? cMac[drivenBy] : cRose);
        const char* label = isMacro ? presetAt(k.macro >= 10 ? secSound(fSel) : cur()).m[mi].label : k.label;
        txt(cx, cy - R - (isMacro ? 16 : 11), label, hot ? cText : cSub, isMacro ? 10.5f : 7.5f, ALIGN_CENTER | ALIGN_MIDDLE, fFontBold, isMacro ? 1.6f : 1.1f);
        if (isMacro) { beginPath(); circle(cx, cy, R + 14); fillPaint(radialGradient(cx, cy, R, R + 16, Color(ac.r, ac.g, ac.b, 0.08f + 0.12f * fGlow * nrm), Color(0, 0, 0, 0))); fill(); }
        beginPath(); circle(cx, cy + 3, R + 2); fillPaint(radialGradient(cx, cy + 3, R - 4, R + 8, Color(0, 0, 0, 0.55f), Color(0, 0, 0, 0))); fill();
        const float tw = isMacro ? 4.5f : 2.6f;
        beginPath(); arc(cx, cy, R, a0, a1, CW); strokeColor(8, 5, 12); strokeWidth(tw + 2); lineCap(ROUND); stroke();
        strokeColor(58, 44, 66); strokeWidth(tw); stroke();
        const float as = bip ? a0 + (a1 - a0) * toNorm(k.param, 0.0f) : a0;
        if (std::fabs(av - as) > 0.002f) {
            const float s0 = std::min(as, av), s1 = std::max(as, av);
            beginPath(); arc(cx, cy, R, s0, s1, CW); strokeColor(ac.r, ac.g, ac.b, hot ? 90 : 50); strokeWidth(tw + 6); lineCap(ROUND); stroke();
            beginPath(); arc(cx, cy, R, s0, s1, CW); strokeColor(ac.r, ac.g, ac.b); strokeWidth(tw); lineCap(ROUND); stroke();
        }
        const float cr = R - (isMacro ? 10 : 5.5f);
        beginPath(); circle(cx, cy, cr); fillPaint(linearGradient(cx, cy - cr, cx, cy + cr, Color(80, 64, 92), Color(28, 20, 36))); fill();
        beginPath(); circle(cx, cy, cr - 2.2f); fillPaint(radialGradient(cx - cr * 0.3f, cy - cr * 0.45f, 1, cr * 1.3f, Color(70, 56, 82), Color(32, 24, 42))); fill();
        beginPath(); moveTo(cx + std::cos(av) * cr * 0.35f, cy + std::sin(av) * cr * 0.35f); lineTo(cx + std::cos(av) * (cr - 3), cy + std::sin(av) * (cr - 3));
        strokeColor(255, 245, 248); strokeWidth(isMacro ? 3.0f : 1.8f); lineCap(ROUND); stroke();
        char b[32];
        if (isMacro) std::snprintf(b, sizeof b, "%.0f %%", v * 100); else formatValue(k.param, v, b, sizeof b);
        txt(cx, cy + R + (isMacro ? 17 : 11), b, hot ? cPeach : cText, isMacro ? 10.5f : 8, ALIGN_CENTER | ALIGN_MIDDLE, fFontMed, 0.2f, hot ? 255 : 190);
        if (drivenBy >= 0) { beginPath(); circle(cx + R * 0.85f, cy - R * 0.85f, 3.2f); fillC(cMac[drivenBy]); fill(); }
    }

    // =========================================================================================
    // input
    void toDesign(double px, double py, float& x, float& y) const { x = (float)px / fScale - fOffX; y = (float)py / fScale - fOffY; }

    bool onMouse(const MouseEvent& ev) override
    {
        float x, y; toDesign(ev.pos.getX(), ev.pos.getY(), x, y);
        if (!ev.press) {
            if (fDragKnob >= 0) { editParameter(fKnobs[(size_t)fDragKnob].param, false); fDragKnob = -1; }
            if (fDragSlider >= 0) { editParameter((uint32_t)fDragSlider, false); fDragSlider = -1; }
            if (fDragRegion >= 0) { editParameter(fDragRegion == 0 ? kRegStart : kRegEnd, false); fDragRegion = -1; }
            if (fDragCut >= 0) { editParameter(slotParam(fDragCut, kSfStart), false); fDragCut = -1; }
            if (fDragFade >= 0) { editParameter(slotParam(fDragFade, kSfFade), false); fDragFade = -1; }
            if (fDragFadeAll) fDragFadeAll = false;
            if (fDragPressed) { fDragPressed = false; if (!fDragStarted && !fRenderPath.empty()) { gf::revealFile(fRenderPath); toast("Saved: " + gf::fileNameOf(fRenderPath.c_str())); } fDragStarted = false; }
            repaint(); return true;
        }
        const bool right = ev.button == kMouseButtonRight;
        const bool dbl = (ev.time - fLastClick < 320) && std::fabs(x - fLastX) < 4 && std::fabs(y - fLastY) < 4;
        fLastClick = ev.time; fLastX = x; fLastY = y;

        // header
        if (rOrig.contains(x, y)) { setParam(kOriginal, fV[kOriginal] > 0.5f ? 0.0f : 1.0f, true); toast(fV[kOriginal] > 0.5f ? "ORIGINAL: your sample, untouched" : "Effect is back"); return true; }
        if (rPrev.contains(x, y)) { if (tlActive()) stepSectionSound(-1); else stepPreset(-1); return true; }
        if (rNext.contains(x, y) || rPresetBox.contains(x, y)) { if (tlActive()) stepSectionSound(+1); else stepPreset(+1); return true; }
        if (rSurprise.contains(x, y)) {
            const int now = tlActive() ? secSound(fSel) : cur();
            int i; do { i = (int)(std::rand() % kNumPresets); } while (i == now && kNumPresets > 1);
            if (tlActive()) { assignSound(fSel, i); fTlCat = kPresets[i].cat; } else { loadPreset(i); fCat = kPresets[i].cat; fScrollRows = 0; }
            return true;
        }
        if (Rect{ rPlay.x - 4, rPlay.y - 4, rPlay.w + 8, rPlay.h + 8 }.contains(x, y)) { if (soundingNow()) hardStop(); else { setParam(kPlayFrom, 0.0f, true); setParam(kPlay, 1.0f, true); } return true; }
        if (rDrag.contains(x, y)) { fDragPressed = true; fDragStarted = false; fPressX = x; fPressY = y; renderToFile(); repaint(); return true; }
        if (rLoad.contains(x, y)) { requestStateFile("sample"); return true; }
        // wave strip
        {   // the start / end triangles sit partly under the LEN / LOOP / KEYS buttons: they win there
            const int h = triangleAt(x, y);
            if (h >= 0) {
                if (right) { setParam(h == 0 ? kRegStart : kRegEnd, h == 0 ? 0.0f : 1.0f, true); toast(h == 0 ? "Start reset" : "End reset"); }
                else { fDragRegion = h; editParameter(h == 0 ? kRegStart : kRegEnd, true); dragRegion(x); }
                return true;
            }
        }
        if (rPasses.contains(x, y)) { fPasses = fPasses >= 4 ? 1 : fPasses * 2; repaint(); return true; }
        if (rLoopBtn.contains(x, y)) { setParam(kLoop, fV[kLoop] > 0.5f ? 0.0f : 1.0f, true); return true; }
        if (rKeys.contains(x, y)) { setParam(kKeys, x < rKeys.x + 44 + (rKeys.w - 44) * 0.5f ? 0.0f : 1.0f, true); return true; }
        if (rWave.contains(x, y) && y >= rWaveIn.y - 12) {
            // left-click = play from here, drag a handle = move it, right-click = move the nearest handle here, right-click a handle triangle = reset it
            const float x0 = rWaveIn.x + std::min(fV[kRegStart], fV[kRegEnd]) * rWaveIn.w, x1 = rWaveIn.x + std::max(fV[kRegStart], fV[kRegEnd]) * rWaveIn.w;
            int near = std::fabs(x - x0) <= std::fabs(x - x1) ? 0 : 1;
            if (fV[kRegStart] > fV[kRegEnd]) near = 1 - near;
            const float hx = std::fabs(x - x0) <= std::fabs(x - x1) ? x0 : x1;   // the nearest handle on screen
            const bool onHandle = std::fabs(x - hx) <= 7 || y < rWaveIn.y;     // its line, or the triangle row above the waveform
            if (right) {
                if (onHandle && y < rWaveIn.y) { setParam(near == 0 ? kRegStart : kRegEnd, near == 0 ? 0.0f : 1.0f, true); toast(near == 0 ? "Start reset" : "End reset"); return true; }
                fDragRegion = near; editParameter(near == 0 ? kRegStart : kRegEnd, true); dragRegion(x);
                editParameter(near == 0 ? kRegStart : kRegEnd, false); fDragRegion = -1;
                toast(near == 0 ? "Start moved here" : "End moved here");
                return true;
            }
            if (onHandle) { fDragRegion = near; editParameter(near == 0 ? kRegStart : kRegEnd, true); dragRegion(x); return true; }
            playFromX(x, x0, x1);
            return true;
        }
        // tabs
        if (rTabSounds.contains(x, y)) { fPage = kPageSounds; repaint(); return true; }
        if (rTabEdit.contains(x, y)) { fPage = kPageEdit; repaint(); return true; }
        if (rTabTl.contains(x, y)) { openTimeline(); return true; }
        // chain chips toggle modules
        for (int m = 0; m < 9; ++m) if (chip(m).contains(x, y)) {
            if (tlActive()) { toast("The timeline is on: each section uses its own sound \xC2\xB7 switch it off to change modules"); return true; }
            toggleModule(m); return true;
        }
        if (fV[kOriginal] > 0.5f && Rect{ 16, 232, 1068, 456 }.contains(x, y)) return true;
        if (fPage == kPageTimeline && pressTimeline(x, y, right, dbl)) return true;
        if (fPage == kPageEdit && tlActive() && Rect{ 16, 232, 1068, 456 }.contains(x, y)) return true;
        // knobs
        const int k = knobAt(x, y);
        if (k >= 0) {
            const Knob& kn = fKnobs[(size_t)k];
            if (kn.macro < 0) {
                const int mb = macroFor(fV, kn.param);
                if (mb >= 0) { char b[96]; std::snprintf(b, sizeof b, "This knob follows the %s macro \xC2\xB7 turn that one on the SOUNDS page", presetAt(cur()).m[mb].label); toast(b); return true; }
            }
            if (dbl || right) {
                if (kn.macro >= 10) { setParam(kn.param, presetAt(secSound(fSel)).mdef[kn.macro - 10], true); return true; }
                float pv[kParamCount]; presetValues(cur(), pv);
                setParam(kn.param, pv[kn.param], true); return true;
            }
            fDragKnob = k; fDragY = y; fDragStartNorm = toNorm(kn.param, fV[kn.param]);
            editParameter(kn.param, true);
            return true;
        }
        if (fPage == kPageSounds) {
            for (int i = 0; i <= kNumCats; ++i) if (catRow(i).contains(x, y)) { fCat = i - 1; fScrollRows = 0; repaint(); return true; }
            if (rList.contains(x, y) && y > rList.y + 32) {
                const int n = catCount(fCat);
                for (int t = 0; t < n; ++t) if (tile(t).contains(x, y)) {
                    if (tlActive()) assignSound(fSel, listIndex(t)); else loadPreset(listIndex(t));
                    return true;
                }
            }
            if (tlActive() && rMacros.contains(x, y) && !rDryWet.contains(x, y) && !rOutSl.contains(x, y)) { openTimeline(); return true; }
            if (rDryWet.contains(x, y)) { fDragSlider = (int)kDryWet; editParameter(kDryWet, true); dragSlider(x); return true; }
            if (rOutSl.contains(x, y)) {
                if (dbl || right) { setParam(kOut, 0.0f, true); return true; }
                fDragSlider = (int)kOut; editParameter(kOut, true); dragSlider(x); return true;
            }
            if (rEditBtn.contains(x, y)) { fPage = kPageEdit; repaint(); return true; }
        } else if (fPage == kPageEdit) {
            for (int m = 0; m < 9; ++m) {
                if (powerRect(m).contains(x, y)) { toggleModule(m); return true; }
                const ModLayout& L = kLayouts[m];
                if (L.toggle >= 0 && toggleRect(m).contains(x, y)) { setParam((uint32_t)L.toggle, fV[L.toggle] > 0.5f ? 0.0f : 1.0f, true); return true; }
                if (L.seg >= 0 && segRect(m).contains(x, y)) {
                    for (int s = 0; s < L.segN; ++s) if (seg(segRect(m), s, L.segN).contains(x, y)) setParam((uint32_t)L.seg, (float)s, true);
                    if (fV[kModules[m].on] < 0.5f) setParam(kModules[m].on, 1.0f, true);
                    return true;
                }
            }
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        float x, y; toDesign(ev.pos.getX(), ev.pos.getY(), x, y);
        if (fDragPressed && !fDragStarted && (std::fabs(x - fPressX) > 4 || std::fabs(y - fPressY) > 4) && !fRenderPath.empty() && gf::canDragFiles()) {
            fDragStarted = true;
            const bool dropped = gf::dragFileOut(fRenderPath);
            fDragPressed = false;
            toast(dropped ? "Dropped into your DAW" : "Drag cancelled \xC2\xB7 file kept in Documents/Grainfield Renders");
            repaint(); return true;
        }
        if (fDragKnob >= 0) {
            const Knob& kn = fKnobs[(size_t)fDragKnob];
            const bool fine = ev.mod & kModifierShift;
            setParam(kn.param, fromNormFree(kn.param, fDragStartNorm + (fDragY - y) / (fine ? 900.0f : (kn.macro >= 0 ? 240.0f : 180.0f))), false);
            return true;
        }
        if (fDragSlider >= 0) { dragSlider(x); return true; }
        if (fDragRegion >= 0) { dragRegion(x); return true; }
        if (motionTimeline(x, y)) return true;
        // hover
        int hk = knobAt(x, y), ht = -1, hc = -1;
        if (fPage == kPageSounds) {
            for (int i = 0; i <= kNumCats; ++i) if (catRow(i).contains(x, y)) hc = i;
            if (rList.contains(x, y) && y > rList.y + 32) { const int n = catCount(fCat); for (int t = 0; t < n; ++t) if (tile(t).contains(x, y)) ht = listIndex(t); }
        }
        if (fPage == kPageTimeline) ht = fHoverTile;   // set by the timeline page
        if (hk != fHoverKnob || ht != fHoverTile || hc != fHoverCat) { fHoverKnob = hk; fHoverTile = ht; fHoverCat = hc; repaint(); }
        return false;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        float x, y; toDesign(ev.pos.getX(), ev.pos.getY(), x, y);
        const float d = (float)ev.delta.getY();
        const int k = knobAt(x, y);
        if (k >= 0) {
            const Knob& kn = fKnobs[(size_t)k];
            if (kn.macro < 0 && macroFor(fV, kn.param) >= 0) return true;
            const ParamInfo in = getParamInfo(kn.param);
            if (in.isInt()) setParam(kn.param, fV[kn.param] + (d > 0 ? 1 : -1), true);
            else setParam(kn.param, fromNormFree(kn.param, toNorm(kn.param, fV[kn.param]) + (d > 0 ? 0.03f : -0.03f)), true);
            return true;
        }
        if (fPage == kPageSounds && rList.contains(x, y)) {
            const int rows = (catCount(fCat) + 1) / 2, vis = 9;
            fScrollRows = std::max(0, std::min(std::max(0, rows - vis), fScrollRows + (d > 0 ? -1 : 1)));
            repaint(); return true;
        }
        if (rPresetBox.contains(x, y)) { stepPreset(d > 0 ? -1 : 1); return true; }
        if (fPage == kPageTimeline && rFadeAll.contains(x, y)) { setFadeAll(clampf(fadeAllValue() + (d > 0 ? 0.05f : -0.05f), 0, 1)); return true; }
        if (fPage == kPageTimeline) for (int t = 0; t < kTiles; ++t) if (tileT(t).contains(x, y)) { fTlCat = (fTlCat + (d > 0 ? kNumCats - 1 : 1)) % kNumCats; repaint(); return true; }
        if (fPage == kPageSounds && (rDryWet.contains(x, y) || rOutSl.contains(x, y))) {
            const uint32_t p = rDryWet.contains(x, y) ? kDryWet : kOut;
            setParam(p, fromNormFree(p, toNorm(p, fV[p]) + (d > 0 ? 0.03f : -0.03f)), true); return true;
        }
        return false;
    }

private:
    float fV[kParamCount];
    std::vector<Knob> fKnobs;
    std::unique_ptr<SampleData> fSample;
    std::string fName; bool fBuiltin = true;
    std::vector<float> fPeaks;
    FontId fFontMed = 0, fFontSemi = 0, fFontBold = 0;
    float fScale = 1, fOffX = 0, fOffY = 0;
    int fPage = kPageSounds;
    // timeline page state
    int fSel = 0, fTlCat = 0, fDragCut = -1, fDragFade = -1, fHoverCut = -1, fHoverPrev = -1;
    bool fFromStart = false, fDragFadeAll = false;
    int fCat = 0, fScrollRows = 0, fHoverTile = -1, fHoverCat = -1, fHoverKnob = -1;
    int fDragKnob = -1, fDragSlider = -1, fDragRegion = -1;
    float fDragY = 0, fDragStartNorm = 0, fLastX = 0, fLastY = 0, fPressX = 0, fPressY = 0;
    uint fLastClick = 0;
    bool fDragPressed = false, fDragStarted = false;
    std::string fRenderPath;
    int fPasses = 1;
    std::string fToast; int fToastTime = 0;
    float fPulse = 0, fGlow = 0, fEndTimer = 0, fStopHold = 0; double fLastIdle = 0;
    // the big button is STOP while anything can be heard: Play, keys, or echoes / reverb still ringing
    bool soundingNow() const { return fStopHold <= 0 && (fV[kPlay] > 0.5f || fV[kOutPlaying] > 0.5f || fV[kOutLevel] > 0.004f); }
    void hardStop() { setParam(kPlay, 0.0f, true); setParam(kStop, (float)(((int)fV[kStop] + 1) % 100000), true); fStopHold = 0.4f; repaint(); }

    int cur() const { return std::min(kNumPresets - 1, std::max(0, (int)fV[kMacroSet])); }

    void updateScale(uint w, uint h)
    {
        fScale = std::min((float)w / W, (float)h / H); if (fScale <= 0.01f) fScale = 1;
        fOffX = ((float)w / fScale - W) * 0.5f; fOffY = ((float)h / fScale - H) * 0.5f;
    }
    void toast(const std::string& t) { fToast = t; fToastTime = 110; repaint(); }

    static float fromNormFree(uint32_t i, float n) { return fromNorm(i, n); }

    void setParam(uint32_t idx, float v, bool gesture)
    {
        const ParamInfo in = getParamInfo(idx);
        v = std::min(in.max, std::max(in.min, v));
        if (in.isInt()) v = std::round(v);
        fV[idx] = v;
        if (gesture) editParameter(idx, true);
        setParameterValue(idx, v);
        if (gesture) editParameter(idx, false);
        repaint();
    }
    void toggleModule(int m) {
        const bool on = fV[kModules[m].on] > 0.5f;
        setParam(kModules[m].on, on ? 0.0f : 1.0f, true);
        toast(std::string(kModules[m].name) + (on ? " off" : " on"));
    }
    void loadPreset(int idx)
    {
        float p[kParamCount]; presetValues(idx, p);
        for (uint32_t i = 0; i < kNumIn; ++i) { if (presetKeeps(i)) continue; if (fV[i] != p[i]) { fV[i] = p[i]; setParameterValue(i, p[i]); } }
        toast(std::string("Sound: ") + kPresets[idx].name);
        repaint();
    }
    // =========================================================================================
    // TIMELINE
    bool tlActive() const { return fV[kTlOn] > 0.5f && sectionCount(fV) >= 2; }
    int secCount() const { return sectionCount(fV); }
    int secSound(int s) const { return std::min(kNumPresets - 1, std::max(0, (int)fV[slotParam(std::max(0, std::min(kMaxSections - 1, s)), kSfSound)])); }
    float secStart(int s) const { return sectionStart(fV, s); }
    float secEnd(int s) const { return s + 1 < secCount() ? secStart(s + 1) : 1.0f; }
    float laneX(float f) const { return rLanes.x + clampf(f, 0, 1) * rLanes.w; }
    float laneF(float x) const { return clampf((x - rLanes.x) / rLanes.w, 0, 1); }
    int secAt(float f) const { int s = 0; for (int k = 1; k < secCount(); ++k) if (f >= secStart(k)) s = k; return s; }
    float fadeWidth(int k) const {   // width (fraction of the region) of the fade into section k
        if (k < 1 || k >= secCount()) return 0;
        return clampf(fV[slotParam(k, kSfFade)], 0, 1) * std::max(0.0f, std::min(secStart(k) - secStart(k - 1), secEnd(k) - secStart(k)));
    }
    Col secCol(int s) const { return { kSecCol[s][0], kSecCol[s][1], kSecCol[s][2] }; }
    Rect playIcon(int s) const { const float x0 = laneX(secStart(s)) + (s > 0 ? fadeWidth(s) * 0.5f * rLanes.w : 0.0f); return { x0 + 6, rLanes.y + 6, 20, 20 }; }

    void selectSection(int s) {
        fSel = std::max(0, std::min(secCount() - 1, s));
        for (auto& k : fKnobs) if (k.macro >= 10) k.param = slotParam(fSel, kSfM1 + (k.macro - 10));
        fTlCat = kPresets[secSound(fSel)].cat;
        repaint();
    }
    void copySlot(int from, int to) { for (int f = 0; f < kSlotParams; ++f) setParam(slotParam(to, f), fV[slotParam(from, f)], true); }
    void assignSound(int s, int idx) {
        setParam(slotParam(s, kSfSound), (float)idx, true);
        for (int m = 0; m < 4; ++m) setParam(slotParam(s, kSfM1 + m), kPresets[idx].mdef[m], true);
        char b[96]; std::snprintf(b, sizeof b, "Section %d: %s", s + 1, kPresets[idx].name); toast(b);
        if (s == fSel) fTlCat = kPresets[idx].cat;
    }
    void openTimeline() {
        fPage = kPageTimeline;
        if (secCount() <= 1) {   // a fresh timeline starts with the sound you have
            setParam(slotParam(0, kSfSound), (float)cur(), true);
            for (int m = 0; m < 4; ++m) setParam(slotParam(0, kSfM1 + m), fV[kM1 + m], true);
        }
        selectSection(std::min(fSel, secCount() - 1));
    }
    void addCutAt(float f) {
        const int n = secCount();
        if (n >= kMaxSections) { toast("6 sounds is the maximum"); return; }
        const int j = secAt(f);
        if (f < secStart(j) + 0.01f || f > secEnd(j) - 0.01f) { toast("Too close to the next line"); return; }
        for (int k = n - 1; k > j; --k) copySlot(k, k + 1);   // make room
        copySlot(j, j + 1);                                       // the new section starts with the same sound
        setParam(slotParam(j + 1, kSfStart), f, true);
        setParam(slotParam(j + 1, kSfFade), n >= 2 ? fadeAllValue() : 0.5f, true);
        setParam(kTlCount, (float)(n + 1), true);
        if (fV[kTlOn] < 0.5f) setParam(kTlOn, 1.0f, true);
        selectSection(j + 1);
        char b[96]; std::snprintf(b, sizeof b, "Section %d added \xC2\xB7 now pick its sound below", j + 2); toast(b);
    }
    void deleteSection(int s) {
        const int n = secCount();
        if (n <= 1) return;
        if (s == 0) { const float keep = 0.0f; for (int k = 0; k < n - 1; ++k) copySlot(k + 1, k); setParam(slotParam(0, kSfStart), keep, true); }
        else for (int k = s; k < n - 1; ++k) copySlot(k + 1, k);
        setParam(kTlCount, (float)(n - 1), true);
        selectSection(std::max(0, s - 1));
        toast(n - 1 >= 2 ? "Section removed" : "One sound left \xC2\xB7 the timeline is now just that sound");
    }
    float fadeAllValue() const { const int n = secCount(); if (n < 2) return 0.5f; float s = 0; for (int k = 1; k < n; ++k) s += fV[slotParam(k, kSfFade)]; return s / (n - 1); }
    void setFadeAll(float v) { for (int k = 1; k < kMaxSections; ++k) setParam(slotParam(k, kSfFade), v, false); repaint(); }
    void preview(int s) {
        if (fV[kTlOn] < 0.5f && secCount() >= 2) setParam(kTlOn, 1.0f, true);
        setParam(kPlayFrom, fFromStart ? 0.0f : secStart(s), true);
        setParam(kCue, (float)(((int)fV[kCue] + 1) % 100000), true);
        setParam(kPlay, 1.0f, true); fStopHold = 0;
    }
    void stepSectionSound(int d) {
        const int c = kPresets[secSound(fSel)].cat;
        int list[kNumPresets], n = 0, at = 0;
        for (int i = 0; i < kNumPresets; ++i) if (kPresets[i].cat == c) { if (i == secSound(fSel)) at = n; list[n++] = i; }
        assignSound(fSel, list[((at + d) % n + n) % n]);
    }
    int sectionTileSound(int t) const { int n = 0; for (int i = 0; i < kNumPresets; ++i) if (kPresets[i].cat == fTlCat) { if (n == t) return i; ++n; } return -1; }
    Rect tilePlay(int t) const { const Rect r = tileT(t); return { r.x + r.w - 28, r.y + 4, 22, 22 }; }

    void playGlyph(float cx, float cy, float r, bool hot, Col c) {
        beginPath(); circle(cx, cy, r); fillColor(hot ? c.r : 20, hot ? c.g : 14, hot ? c.b : 24, hot ? 255 : 200); fill();
        strokeColor(c.r, c.g, c.b, 200); strokeWidth(1.2f); stroke();
        beginPath(); moveTo(cx - r * 0.3f, cy - r * 0.45f); lineTo(cx + r * 0.5f, cy); lineTo(cx - r * 0.3f, cy + r * 0.45f); closePath();
        fillC(hot ? cInk : c); fill();
    }

    void drawTimeline()
    {
        const int n = secCount();
        const bool on = tlActive();
        card(rTl);
        title(rTl.x + 14, rTl.y + 17, "TIMELINE");
        txt(rTl.x + 98, rTl.y + 17, n < 2 ? "double-click in the waveform where the next sound should start"
                                          : "double-click: add a sound \xC2\xB7 drag a line: move it \xC2\xB7 drag the white dots: fade length \xC2\xB7 right-click a line: remove \xC2\xB7 play buttons preview",
            cDim, 8.5f, ALIGN_LEFT | ALIGN_MIDDLE, fFontMed, 0.3f);
        pill(rTlOn, n < 2 ? "1 SOUND" : (fV[kTlOn] > 0.5f ? "ON" : "OFF"), on, 9);
        // lanes
        const Rect& r = rLanes;
        beginPath(); roundedRect(r.x - 4, r.y - 2, r.w + 8, r.h + 4, 9); fillC(cWell, 230); fill();
        for (int s = 0; s < n; ++s) {
            const float x0 = laneX(secStart(s)), x1 = laneX(secEnd(s));
            const Col c = secCol(s);
            beginPath(); rect(x0, r.y, x1 - x0, r.h);
            fillPaint(linearGradient(0, r.y, 0, r.y + r.h, Color(c.r, c.g, c.b, s == fSel ? 0.34f : 0.18f), Color(c.r, c.g, c.b, s == fSel ? 0.16f : 0.07f))); fill();
        }
        // fade bands: one color melting into the next
        for (int k = 1; k < n; ++k) {
            const float b = laneX(secStart(k)), w = fadeWidth(k) * r.w;
            if (w < 1.0f) continue;
            const Col a = secCol(k - 1), c = secCol(k);
            beginPath(); rect(b - w * 0.5f, r.y, w, r.h);
            fillPaint(linearGradient(b - w * 0.5f, 0, b + w * 0.5f, 0, Color(a.r, a.g, a.b, 0.32f), Color(c.r, c.g, c.b, 0.32f))); fill();
        }
        // waveform of the region
        if (!fPeaks.empty()) {
            const float ra = std::min(fV[kRegStart], fV[kRegEnd]), rb = std::max(fV[kRegStart], fV[kRegEnd]);
            const int cols = (int)fPeaks.size();
            const float mid = r.y + r.h * 0.56f, amp = r.h * 0.36f;
            beginPath();
            for (int i = 0; i <= (int)r.w; i += 2) { const int ci = std::min(cols - 1, (int)((ra + (rb - ra) * i / r.w) * cols)); const float yy = mid - fPeaks[(size_t)ci] * amp; if (i == 0) moveTo(r.x + i, yy); else lineTo(r.x + i, yy); }
            for (int i = (int)r.w; i >= 0; i -= 2) { const int ci = std::min(cols - 1, (int)((ra + (rb - ra) * i / r.w) * cols)); lineTo(r.x + i, mid + fPeaks[(size_t)ci] * amp); }
            closePath(); fillColor(255, 235, 240, 120); fill();
        }
        // section labels with a play button each
        for (int s = 0; s < n; ++s) {
            const float x0 = laneX(secStart(s)), x1 = laneX(secEnd(s));
            const Col c = secCol(s);
            const Rect pi = playIcon(s);
            if (x1 - pi.x < 26) continue;
            playGlyph(pi.cx(), pi.cy(), 9, fHoverPrev == s, c);
            save(); scissor(pi.x + 24, r.y, std::max(0.0f, x1 - pi.x - 30), r.h);
            char b[64]; std::snprintf(b, sizeof b, "%d  %s", s + 1, kPresets[secSound(s)].name);
            txt(pi.x + 26, pi.cy() + 0.5f, b, s == fSel ? cText : cSub, 10, ALIGN_LEFT | ALIGN_MIDDLE, fFontBold, 0.3f);
            restore();
            if (s == fSel) { beginPath(); roundedRect(x0 + 1, r.y + 1, x1 - x0 - 2, r.h - 2, 6); strokeColor(c.r, c.g, c.b, 230); strokeWidth(2); stroke(); }
        }
        // lines + fade handles
        for (int k = 1; k < n; ++k) {
            const float b = laneX(secStart(k)), w = fadeWidth(k) * r.w;
            const bool hot = fDragCut == k || fHoverCut == k;
            beginPath(); moveTo(b, r.y); lineTo(b, r.y + r.h); strokeColor(255, 255, 255, hot ? 240 : 150); strokeWidth(hot ? 2.2f : 1.4f); stroke();
            beginPath(); roundedRect(b - 4, r.y + r.h * 0.5f - 11, 8, 22, 4); fillColor(255, 255, 255, hot ? 255 : 190); fill();
            for (int side = -1; side <= 1; side += 2) {
                const float hx = b + side * std::max(7.0f, w * 0.5f);
                const bool fh = fDragFade == k;
                beginPath(); circle(hx, r.y + r.h - 10, fh ? 6.0f : 5.0f); fillC(fh ? cPeach : Col{ 255, 255, 255 }, fh ? 255 : 210); fill();
            }
        }
        // playhead
        if (fV[kOutPlaying] > 0.5f) {
            const float ra = std::min(fV[kRegStart], fV[kRegEnd]), rb = std::max(fV[kRegStart], fV[kRegEnd]);
            const float px = laneX((fV[kOutPos] - ra) / std::max(0.001f, rb - ra));
            beginPath(); moveTo(px, r.y); lineTo(px, r.y + r.h); strokeColor(255, 255, 255, 70); strokeWidth(7); stroke();
            beginPath(); moveTo(px, r.y); lineTo(px, r.y + r.h); strokeColor(255, 255, 255, 240); strokeWidth(1.5f); stroke();
        }
        if (n < 2) txt(r.cx(), r.y + r.h - 22, "this whole sample uses one sound \xC2\xB7 double-click where the next sound should start", cText, 10, ALIGN_CENTER | ALIGN_MIDDLE, fFontSemi, 0.3f, 200);
        // bottom row
        pill(rAddSec, "+ ADD SOUND", false, 9);
        pill(rDelSec, "REMOVE", false, 9, n < 2 ? cDim : cText);
        {   // FADE ALL slider
            const Rect& f = rFadeAll; const float v = fadeAllValue();
            beginPath(); roundedRect(f.x, f.y, f.w, f.h, f.h * 0.5f); fillC(cWell); fill(); strokeColor(255, 255, 255, 14); strokeWidth(1); stroke();
            beginPath(); roundedRect(f.x, f.y, std::max(f.h, f.w * v), f.h, f.h * 0.5f); accent(f.x, 0, f.x + f.w, 0, 0.9f); fill();
            char b[48]; if (v < 0.005f) std::snprintf(b, sizeof b, "FADES  hard cut"); else std::snprintf(b, sizeof b, "FADES  %.0f %%", v * 100);
            txt(f.cx(), f.cy() + 0.5f, b, v > 0.5f ? cInk : cText, 9, ALIGN_CENTER | ALIGN_MIDDLE, fFontBold, 1.0f);
        }
        segmented(rCurve, kCurveNames, 3, (int)fV[kTlCurve], 8.5f);
        segmented(rTails, kTailNames, 2, (int)fV[kTlTails], 8.5f);

        // sound picker for the selected section
        card(rPickCard);
        const Col sc = secCol(fSel);
        beginPath(); circle(rPickCard.x + 18, rPickCard.y + 17, 5); fillC(sc); fill();
        char t[64]; std::snprintf(t, sizeof t, "SECTION %d  \xC2\xB7  CHOOSE ITS SOUND", fSel + 1);
        title(rPickCard.x + 30, rPickCard.y + 17, t, cText);
        txt(rFrom.x - 8, rFrom.cy() + 0.5f, "PREVIEW", cDim, 7.5f, ALIGN_RIGHT | ALIGN_MIDDLE, fFontBold, 1.2f);
        static const char* fromN[2] = { "FROM SECTION", "FROM START" };
        segmented(rFrom, fromN, 2, fFromStart ? 1 : 0, 8);
        for (int i = 0; i < kNumCats; ++i) pill(chipT(i), kCatShort[i], i == fTlCat, 8);
        for (int tI = 0; tI < kTiles; ++tI) {
            const int idx = sectionTileSound(tI); if (idx < 0) break;
            const Rect tr = tileT(tI);
            const bool sel = idx == secSound(fSel), hov = idx == fHoverTile;
            beginPath(); roundedRect(tr.x, tr.y, tr.w, tr.h, 9);
            if (sel) { fillColor(sc.r, sc.g, sc.b, 230); fill(); }
            else { fillColor(255, 255, 255, hov ? 22 : 9); fill(); strokeColor(255, 255, 255, hov ? 40 : 14); strokeWidth(1); stroke(); }
            txt(tr.x + 12, tr.cy() + 0.5f, kPresets[idx].name, sel ? cInk : cText, 10, ALIGN_LEFT | ALIGN_MIDDLE, fFontSemi, 0.3f);
            const Rect pb = tilePlay(tI);
            playGlyph(pb.cx(), pb.cy(), 9, fHoverPrev == 100 + tI, sel ? Col{ 255, 255, 255 } : sc);
        }
        // the selected section's sound and knobs
        card(rSecCard);
        const Preset& pr = presetAt(secSound(fSel));
        beginPath(); circle(rSecCard.x + 18, rSecCard.y + 22, 6); fillC(sc); fill();
        char st[48]; std::snprintf(st, sizeof st, "SECTION %d", fSel + 1);
        txt(rSecCard.x + 30, rSecCard.y + 18, st, sc, 8, ALIGN_LEFT | ALIGN_MIDDLE, fFontBold, 1.6f);
        txt(rSecCard.x + 30, rSecCard.y + 36, pr.name, cText, 15, ALIGN_LEFT | ALIGN_MIDDLE, fFontBold, 0.4f);
        wrapText(rSecCard.x + 16, rSecCard.y + 52, rSecCard.w - 32, pr.desc, cSub, 9.5f, fFontMed, 13);
        for (size_t i = 0; i < fKnobs.size(); ++i) if (fKnobs[i].macro >= 10) drawKnob((int)i);
        txt(rSecCard.cx(), rSecCard.y + rSecCard.h - 14, "these four knobs shape this section only", cDim, 8, ALIGN_CENTER | ALIGN_MIDDLE, fFontMed, 0.3f);
    }

    bool pressTimeline(float x, float y, bool right, bool dbl)
    {
        const int n = secCount();
        if (rTlOn.contains(x, y)) {
            if (n < 2) { toast("Double-click in the waveform to add a second sound first"); return true; }
            setParam(kTlOn, fV[kTlOn] > 0.5f ? 0.0f : 1.0f, true); toast(fV[kTlOn] > 0.5f ? "Timeline on" : "Timeline off \xC2\xB7 the single sound plays"); return true;
        }
        if (rLanes.contains(x, y) || Rect{ rLanes.x - 8, rLanes.y, rLanes.w + 16, rLanes.h }.contains(x, y)) {
            const float f = laneF(x);
            int near = -1; for (int k = 1; k < n; ++k) if (std::fabs(x - laneX(secStart(k))) < 7) near = k;
            if (right) { if (near > 0) deleteSection(near); else toast("Right-click a line to remove it"); return true; }
            for (int s = 0; s < n; ++s) if (playIcon(s).contains(x, y) && laneX(secEnd(s)) - playIcon(s).x >= 26) { selectSection(s); preview(s); return true; }
            // fade dots
            if (y > rLanes.y + rLanes.h - 22)
                for (int k = 1; k < n; ++k) {
                    const float b = laneX(secStart(k)), w = std::max(7.0f, fadeWidth(k) * rLanes.w * 0.5f);
                    if (std::fabs(x - (b - w)) < 8 || std::fabs(x - (b + w)) < 8) { fDragFade = k; editParameter(slotParam(k, kSfFade), true); return true; }
                }
            if (near > 0 && !dbl) { fDragCut = near; editParameter(slotParam(near, kSfStart), true); return true; }
            if (dbl) { addCutAt(f); return true; }
            selectSection(secAt(f));
            return true;
        }
        if (rAddSec.contains(x, y)) { addCutAt((secStart(fSel) + secEnd(fSel)) * 0.5f); return true; }
        if (rDelSec.contains(x, y)) { if (n < 2) toast("Only one sound \xC2\xB7 nothing to remove"); else deleteSection(fSel); return true; }
        if (rFadeAll.contains(x, y)) { fDragFadeAll = true; setFadeAll((x - rFadeAll.x) / rFadeAll.w); return true; }
        if (rCurve.contains(x, y)) { for (int i = 0; i < 3; ++i) if (seg(rCurve, i, 3).contains(x, y)) setParam(kTlCurve, (float)i, true); return true; }
        if (rTails.contains(x, y)) { for (int i = 0; i < 2; ++i) if (seg(rTails, i, 2).contains(x, y)) setParam(kTlTails, (float)i, true); return true; }
        if (rFrom.contains(x, y)) { fFromStart = x >= rFrom.cx(); repaint(); return true; }
        for (int i = 0; i < kNumCats; ++i) if (chipT(i).contains(x, y)) { fTlCat = i; repaint(); return true; }
        for (int t = 0; t < kTiles; ++t) if (tileT(t).contains(x, y)) {
            const int idx = sectionTileSound(t); if (idx < 0) return true;
            assignSound(fSel, idx);
            if (tilePlay(t).contains(x, y)) preview(fSel);
            return true;
        }
        return false;
    }

    bool motionTimeline(float x, float y)
    {
        if (fDragCut >= 0) {
            const int k = fDragCut;
            const float lo = secStart(k - 1) + 0.01f, hi = secEnd(k) - 0.01f;
            setParam(slotParam(k, kSfStart), clampf(laneF(x), lo, hi), false);
            return true;
        }
        if (fDragFade >= 0) {
            const int k = fDragFade;
            const float span = std::min(secStart(k) - secStart(k - 1), secEnd(k) - secStart(k));
            setParam(slotParam(k, kSfFade), span > 1e-4f ? clampf(2.0f * std::fabs(laneF(x) - secStart(k)) / span, 0, 1) : 0.0f, false);
            return true;
        }
        if (fDragFadeAll) { setFadeAll((x - rFadeAll.x) / rFadeAll.w); return true; }
        if (fPage != kPageTimeline) return false;
        int hc = -1, hp = -1, ht = -1;
        if (rLanes.contains(x, y)) {
            for (int k = 1; k < secCount(); ++k) if (std::fabs(x - laneX(secStart(k))) < 7) hc = k;
            for (int s = 0; s < secCount(); ++s) if (playIcon(s).contains(x, y)) hp = s;
        }
        for (int t = 0; t < kTiles; ++t) if (tileT(t).contains(x, y)) { ht = sectionTileSound(t); if (tilePlay(t).contains(x, y)) hp = 100 + t; }
        if (hc != fHoverCut || hp != fHoverPrev || ht != fHoverTile) { fHoverCut = hc; fHoverPrev = hp; fHoverTile = ht; repaint(); }
        return false;
    }

    void stepPreset(int d)
    {
        // inside the selected category (or all sounds)
        const int n = catCount(fCat);
        int k = 0; for (int t = 0; t < n; ++t) if (listIndex(t) == cur()) k = t;
        if (fCat >= 0 && presetAt(cur()).cat != fCat) k = d > 0 ? -1 : 0;
        k = ((k + d) % n + n) % n;
        loadPreset(listIndex(k));
        const int row = k / 2; if (row < fScrollRows) fScrollRows = row; if (row > fScrollRows + 8) fScrollRows = row - 8;
    }
    void dragSlider(float x)
    {
        const Rect& r = fDragSlider == (int)kDryWet ? rDryWet : rOutSl;
        setParam((uint32_t)fDragSlider, fromNorm((uint32_t)fDragSlider, (x - r.x) / r.w), false);
    }
    // which start / end triangle is at this point (0 = start, 1 = end, -1 = none)
    int triangleAt(float x, float y) const
    {
        if (y < rWaveIn.y - 10 || y > rWaveIn.y + 1) return -1;
        const bool swapped = fV[kRegStart] > fV[kRegEnd];
        const float x0 = rWaveIn.x + std::min(fV[kRegStart], fV[kRegEnd]) * rWaveIn.w, x1 = rWaveIn.x + std::max(fV[kRegStart], fV[kRegEnd]) * rWaveIn.w;
        const float d0 = std::fabs(x - x0), d1 = std::fabs(x - x1);
        if (std::min(d0, d1) > 8) return -1;
        const int lowHigh = d0 <= d1 ? 0 : 1;
        return swapped ? 1 - lowHigh : lowHigh;
    }
    void playFromX(float x, float x0, float x1)
    {
        float f = (x - x0) / std::max(1.0f, x1 - x0);
        if (f < 0 || f > 1) toast("That part is outside START / END \xC2\xB7 right-click there to move them");
        f = std::min(0.999f, std::max(0.0f, f));
        setParam(kPlayFrom, f, true);
        setParam(kCue, (float)(((int)fV[kCue] + 1) % 100000), true);   // restarts if it is already playing
        setParam(kPlay, 1.0f, true); fStopHold = 0; repaint();
    }
    void dragRegion(float x)
    {
        float v = std::min(1.0f, std::max(0.0f, (x - rWaveIn.x) / rWaveIn.w));
        if (fDragRegion == 0) v = std::min(v, fV[kRegEnd] - 0.005f); else v = std::max(v, fV[kRegStart] + 0.005f);
        setParam(fDragRegion == 0 ? kRegStart : kRegEnd, v, false);
    }
    void computePeaks()
    {
        const int cols = 1044; fPeaks.assign((size_t)cols, 0.0f);
        if (!fSample) return;
        const size_t n = fSample->frames(); if (!n) return;
        float gmax = 1e-6f;
        for (int c = 0; c < cols; ++c) {
            const size_t a = n * (size_t)c / cols, b = std::max(a + 1, n * (size_t)(c + 1) / cols);
            float m = 0; for (size_t i = a; i < b && i < n; ++i) m = std::max(m, std::fabs(0.5f * (fSample->L[i] + fSample->R[i])));
            fPeaks[(size_t)c] = m; gmax = std::max(gmax, m);
        }
        for (auto& v : fPeaks) v /= gmax;
    }
    void renderToFile()
    {
        if (!fSample || fSample->empty()) return;
        const double bpm = fV[kOutBpm] > 20 ? fV[kOutBpm] : 120.0;
        const double sr = getSampleRate() > 8000 ? getSampleRate() : 44100.0;
        std::vector<float> L, R;
        renderLumenOffline(fV, *fSample, sr, bpm, fPasses, L, R);
        std::string name = fBuiltin ? std::string("Built-in") : fName;
        const size_t dot = name.find_last_of('.'); if (!fBuiltin && dot != std::string::npos) name = name.substr(0, dot);
        std::string pn = fV[kOriginal] > 0.5f ? std::string("Original") : std::string(presetAt(cur()).name);
        for (auto* s : { &name, &pn }) { for (auto& ch : *s) if (std::strchr("/\\:*?\"<>|", ch)) ch = '_'; if (s->size() > 40) s->resize(40); }
        std::time_t t = std::time(nullptr); char stamp[32]; std::strftime(stamp, sizeof stamp, "%H%M%S", std::localtime(&t));
        char file[256]; std::snprintf(file, sizeof file, "Lumen - %s - %s - %s.wav", name.c_str(), pn.c_str(), stamp);
        const std::string dir = gf::rendersFolder();
#ifdef _WIN32
        fRenderPath = dir + "\\" + file;
#else
        fRenderPath = dir + "/" + file;
#endif
        if (!gf::writeWav24(fRenderPath, L, R, (unsigned)sr)) { toast("Could not write render"); fRenderPath.clear(); }
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LumenUI)
};

UI* createUI() { return new LumenUI(); }

END_NAMESPACE_DISTRHO
