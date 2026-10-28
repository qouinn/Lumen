// Lumen — plugin (DSP side)

#include "DistrhoPlugin.hpp"
#include "extra/Mutex.hpp"
#include "extra/String.hpp"

#include "LumenDSP.hpp"
#include "LumenOwn.hpp"
#include "AudioFile.hpp"

#include <memory>
#include <vector>

START_NAMESPACE_DISTRHO

using namespace lm;

enum States { kStateSample = 0, kStateEdits, kStateOwn, kStateCount };

class LumenPlugin : public Plugin
{
public:
    LumenPlugin()
        : Plugin(kParamCount, kNumPresets, kStateCount)
    {
        fBuiltin.reset(new SampleData());
        makeLumenSource(*fBuiltin, 44100.0);
        ownLoadLibrary();
        fEngine.reset(new LumenEngine());
        fEngine->setEditSource(&fEdits);
        float p[kParamCount]; presetValues(0, p);
        for (uint32_t i = 0; i < kNumIn; ++i) fEngine->p[i] = p[i];
        fEngine->setSampleRate(getSampleRate());
        fEngine->setSample(fBuiltin.get());
        fMidi.reserve(512);
    }

protected:
    const char* getLabel() const override { return "Lumen"; }
    const char* getDescription() const override { return DISTRHO_PLUGIN_DESCRIPTION; }
    const char* getMaker() const override { return DISTRHO_PLUGIN_BRAND; }
    const char* getHomePage() const override { return "https://grainfield.audio"; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(1, 4, 0); }

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        port.groupId = kPortGroupStereo;
        Plugin::initAudioPort(input, index, port);
    }

    void initParameter(uint32_t index, Parameter& p) override
    {
        const ParamInfo in = getParamInfo(index);
        p.name = in.name; p.symbol = in.symbol;
        p.ranges.min = in.min; p.ranges.max = in.max;
        float def = in.def;
        if (index < kNumIn) { float pv[kParamCount]; presetValues(0, pv); def = pv[index]; }   // defaults = the first preset
        p.ranges.def = def;
        p.hints = in.isOut() ? kParameterIsOutput : kParameterIsAutomatable;
        if (in.isInt()) p.hints |= kParameterIsInteger;
        if (in.isBool()) p.hints |= kParameterIsBoolean;
        if (in.isLog()) p.hints |= kParameterIsLogarithmic;
        auto setEnum = [&](const char* const* names, int count) {
            p.enumValues.count = (uint8_t)count; p.enumValues.restrictedMode = true;
            p.enumValues.values = new ParameterEnumerationValue[count];
            for (int i = 0; i < count; ++i) { p.enumValues.values[i].label = names[i]; p.enumValues.values[i].value = in.min + i; }
        };
        switch (index) {
        case kKeys: setEnum(kKeysNames, 2); break;
        case kDMode: setEnum(kDriveNames, 5); break;
        case kFType: setEnum(kFilterNames, 4); break;
        case kMType: setEnum(kModNames, 7); break;
        case kSMode: setEnum(kStutNames, 6); break;
        case kSDiv: setEnum(kStutDivNames, 7); break;
        case kYDiv: setEnum(kDelayNames, 9); break;
        case kTlCurve: setEnum(kCurveNames, 3); break;
        case kTlTails: setEnum(kTailNames, 2); break;
        case kOut: case kTrim: p.unit = "dB"; break;
        case kFCut: p.unit = "Hz"; break;
        case kGSize: case kRPre: p.unit = "ms"; break;
        case kRDecay: p.unit = "s"; break;
        case kGPitch: case kPA: case kPB: p.unit = "st"; break;
        default: break;
        }
        if (index == kMacroSet) p.hints = kParameterIsAutomatable | kParameterIsInteger;   // kept for project recall
        if (index == kCue) p.hints = kParameterIsInteger;                                     // internal: restarts a preview
        if (index == kStop) p.hints = kParameterIsInteger;                                    // internal: hard stop
    }

    void initProgramName(uint32_t index, String& name) override { name = presetAt((int)index).name; }

    void initState(uint32_t index, State& st) override
    {
        switch (index) {
        case kStateSample: st.key = "sample"; st.label = "Sample"; st.hints = kStateIsFilenamePath; st.defaultValue = ""; break;
        case kStateEdits:  st.key = "sec_edits"; st.label = "Section edits"; st.hints = 0; st.defaultValue = ""; break;
        default:           st.key = "own"; st.label = "Own presets used"; st.hints = kStateIsOnlyForDSP; st.defaultValue = ""; break;
        }
    }

    float getParameterValue(uint32_t i) const override
    {
        switch (i) {
        case kOutPos: return fEngine->playhead();
        case kOutLevel: return fEngine->getLevel();
        case kOutPlaying: return fEngine->playing() ? 1.0f : 0.0f;
        case kOutBpm: return (float)fEngine->getBpm();
        case kOutSection: return (float)fEngine->currentSection();
        default: return i < kParamCount ? fEngine->p[i] : 0.0f;
        }
    }
    void setParameterValue(uint32_t i, float v) override { if (i < kNumIn) fEngine->p[i] = v; }

    void loadProgram(uint32_t index) override
    {
        float pr[kParamCount];
        presetValues((int)index, pr);
        for (uint32_t i = 0; i < kNumIn; ++i) if (!presetKeeps(i)) fEngine->p[i] = pr[i];
    }

    String getState(const char* key) const override
    {
        if (!std::strcmp(key, "sample")) return fPath;
        if (!std::strcmp(key, "sec_edits")) return fEditsText;
        if (!std::strcmp(key, "own")) return String(ownProjectText(fEngine->p).c_str());   // copies travel with the project
        return String();
    }

    void setState(const char* key, const char* value) override
    {
        if (!std::strcmp(key, "sec_edits")) {
            fEditsText = value ? value : "";
            float ov[kMaxSections][kNumEdit];
            editsFromText(value, ov, kMaxSections);
            fEdits.write(ov);
            return;
        }
        if (!std::strcmp(key, "own")) { ownFromProject(value); fEngine->refreshSounds(); return; }
        if (std::strcmp(key, "sample")) return;
        std::unique_ptr<SampleData> ns;
        if (value && *value) {
            ns.reset(new SampleData());
            if (!gf::loadAudioFile(value, *ns)) { d_stderr("Lumen: could not load '%s'", value); return; }
        }
        std::unique_ptr<SampleData> old;
        {
            const MutexLocker ml(fMutex);
            old.swap(fSample); fSample.swap(ns);
            fPath = value ? value : "";
            fEngine->setSample(fSample ? fSample.get() : fBuiltin.get());
        }
    }

    void activate() override { fEngine->reset(); }
    void sampleRateChanged(double sr) override { const MutexLocker ml(fMutex); fEngine->setSampleRate(sr); }

    void run(const float**, float** outputs, uint32_t frames, const MidiEvent* ev, uint32_t evCount) override
    {
        const MutexTryLocker tl(fMutex);
        if (!tl.wasLocked()) { std::memset(outputs[0], 0, sizeof(float) * frames); std::memset(outputs[1], 0, sizeof(float) * frames); return; }
        fMidi.clear();
        for (uint32_t i = 0; i < evCount && fMidi.size() < fMidi.capacity(); ++i) {
            if (ev[i].size == 0 || ev[i].size > 3) continue;
            gf::MidiMsg m; m.frame = ev[i].frame < frames ? ev[i].frame : frames - 1;
            m.status = ev[i].data[0]; m.d1 = ev[i].size > 1 ? ev[i].data[1] : 0; m.d2 = ev[i].size > 2 ? ev[i].data[2] : 0;
            fMidi.push_back(m);
        }
        const TimePosition& pos = getTimePosition();
        gf::TimeInfo ti; ti.playing = pos.playing; ti.valid = pos.bbt.valid;
        if (pos.bbt.valid) {
            ti.bpm = pos.bbt.beatsPerMinute;
            const double tpb = pos.bbt.ticksPerBeat > 0 ? pos.bbt.ticksPerBeat : 1920.0;
            const double bpb = pos.bbt.beatsPerBar > 0 ? pos.bbt.beatsPerBar : 4.0;
            ti.ppq = (pos.bbt.bar - 1) * bpb + (pos.bbt.beat - 1) + pos.bbt.tick / tpb;
            ti.beatsPerBar = bpb;
        }
        fEngine->process(outputs[0], outputs[1], frames, fMidi.data(), (uint32_t)fMidi.size(), ti);
    }

private:
    std::unique_ptr<LumenEngine> fEngine;
    std::unique_ptr<SampleData> fBuiltin, fSample;
    std::vector<gf::MidiMsg> fMidi;
    Mutex fMutex;
    String fPath, fEditsText;
    EditBox fEdits;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LumenPlugin)
};

Plugin* createPlugin() { return new LumenPlugin(); }

END_NAMESPACE_DISTRHO
