// Minimal CLAP host used to test the real Lumen binary:
// loads the .clap, checks params/presets/state, renders audio with notes in all modes,
// loads a WAV through the plugin state, and (optionally) opens the GUI on an X11 display.
#include "clap/entry.h"
#include "clap/plugin-factory.h"
#include "clap/ext/params.h"
#include "clap/ext/state.h"
#include "clap/ext/gui.h"
#include "clap/ext/timer-support.h"
#include "clap/ext/audio-ports.h"
#include "clap/ext/note-ports.h"

#ifdef _WIN32
# include <windows.h>
# define NO_GUI_TEST 1
static void* dlopen(const char* p, int) { return (void*)LoadLibraryA(p); }
static void* dlsym(void* h, const char* s) { return (void*)GetProcAddress((HMODULE)h, s); }
static const char* dlerror() { return "LoadLibrary failed"; }
static void dlclose(void* h) { FreeLibrary((HMODULE)h); }
# define RTLD_NOW 0
# define RTLD_LOCAL 0
#else
# include <dlfcn.h>
# include <X11/Xlib.h>
# include <unistd.h>
#endif
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <set>
#include <vector>
#include <chrono>
#include <thread>

static int failures = 0;
static void check(bool ok, const std::string& what) { std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what.c_str()); if (!ok) ++failures; }

// ---------------- host
struct Timer { clap_id id; uint32_t period; };
static std::vector<Timer> gTimers;
static bool CLAP_ABI regTimer(const clap_host_t*, uint32_t period, clap_id* id) { *id = (clap_id)gTimers.size() + 1; gTimers.push_back({ *id, period }); return true; }
static bool CLAP_ABI unregTimer(const clap_host_t*, clap_id) { return true; }
static const clap_host_timer_support_t gTimerExt = { regTimer, unregTimer };
static void CLAP_ABI hgResize(const clap_host_t*) {}
static bool CLAP_ABI hgReq(const clap_host_t*, uint32_t, uint32_t) { return true; }
static bool CLAP_ABI hgShow(const clap_host_t*) { return true; }
static void CLAP_ABI hgClosed(const clap_host_t*, bool) {}
static const clap_host_gui_t gGuiExt = { hgResize, hgReq, hgShow, hgShow, hgClosed };
static const void* CLAP_ABI hostGetExt(const clap_host_t*, const char* id) {
    if (std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0) return &gTimerExt;
    if (std::strcmp(id, CLAP_EXT_GUI) == 0) return &gGuiExt;
    return nullptr;
}
static void CLAP_ABI hostNoop(const clap_host_t*) {}
static clap_host_t gHost = { CLAP_VERSION, nullptr, "gf-test-host", "test", "", "1.0", hostGetExt, hostNoop, hostNoop, hostNoop };

// ---------------- events
struct EvList {
    std::vector<std::vector<uint8_t>> evs;
    void note(uint32_t t, bool on, int key, double vel = 0.8) {
        clap_event_note_t e{}; e.header.size = sizeof e; e.header.time = t; e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        e.header.type = on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF; e.note_id = -1; e.port_index = 0; e.channel = 0; e.key = (int16_t)key; e.velocity = vel;
        push(&e, sizeof e);
    }
    void param(uint32_t t, clap_id id, double v) {
        clap_event_param_value_t e{}; e.header.size = sizeof e; e.header.time = t; e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        e.header.type = CLAP_EVENT_PARAM_VALUE; e.param_id = id; e.cookie = nullptr; e.note_id = -1; e.port_index = -1; e.channel = -1; e.key = -1; e.value = v;
        push(&e, sizeof e);
    }
    void push(const void* p, size_t n) { evs.emplace_back((const uint8_t*)p, (const uint8_t*)p + n); }
};
static uint32_t CLAP_ABI inSize(const clap_input_events_t* l) { return (uint32_t)((EvList*)l->ctx)->evs.size(); }
static const clap_event_header_t* CLAP_ABI inGet(const clap_input_events_t* l, uint32_t i) { return (const clap_event_header_t*)((EvList*)l->ctx)->evs[i].data(); }
static bool CLAP_ABI outPush(const clap_output_events_t*, const clap_event_header_t*) { return true; }

// ---------------- streams
struct Buf { std::string data; size_t pos = 0; };
static int64_t CLAP_ABI osWrite(const clap_ostream_t* s, const void* b, uint64_t n) { ((Buf*)s->ctx)->data.append((const char*)b, (size_t)n); return (int64_t)n; }
static int64_t CLAP_ABI isRead(const clap_istream_t* s, void* b, uint64_t n) {
    Buf* x = (Buf*)s->ctx; const size_t k = std::min((size_t)n, x->data.size() - x->pos);
    std::memcpy(b, x->data.data() + x->pos, k); x->pos += k; return (int64_t)k;
}

struct Stats { float peak = 0; double rms = 0; bool finite = true; };

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    const char* path = argv[1];
    const char* wav = argc > 2 ? argv[2] : nullptr;
    const bool gui = argc > 3;
    void* lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    check(lib != nullptr, "dlopen");
    if (!lib) return 1;
    auto entry = (const clap_plugin_entry_t*)dlsym(lib, "clap_entry");
    entry->init(path);
    auto factory = (const clap_plugin_factory_t*)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);
    std::printf("      plugin: %s by %s (%s)\n", desc->name, desc->vendor, desc->id);
    bool instrument = false; for (const char* const* f = desc->features; f && *f; ++f) if (!std::strcmp(*f, "instrument")) instrument = true;
    check(instrument, "registered as an instrument (shows with the instruments in FL)");
    const clap_plugin_t* plug = factory->create_plugin(factory, &gHost, desc->id);
    check(plug && plug->init(plug), "create + init");
    auto params = (const clap_plugin_params_t*)plug->get_extension(plug, CLAP_EXT_PARAMS);
    auto state = (const clap_plugin_state_t*)plug->get_extension(plug, CLAP_EXT_STATE);
    auto guiExt = (const clap_plugin_gui_t*)plug->get_extension(plug, CLAP_EXT_GUI);
    auto timerExt = (const clap_plugin_timer_support_t*)plug->get_extension(plug, CLAP_EXT_TIMER_SUPPORT);
    const uint32_t pc = params->count(plug);
    std::printf("      %u parameters\n", pc);
    check(pc == 312, "parameter table exported (" + std::to_string(pc) + ")");
    auto findParam = [&](const char* name) -> clap_id { for (uint32_t i = 0; i < pc; ++i) { clap_param_info_t inf; params->get_info(plug, i, &inf); if (!std::strcmp(inf.name, name)) return inf.id; } return CLAP_INVALID_ID; };
    auto val = [&](const char* name) { double v = -999; params->get_value(plug, findParam(name), &v); return v; };
    const double sr = 48000; const uint32_t block = 256;
    plug->activate(plug, sr, 32, block); plug->start_processing(plug);
    std::vector<float> L(block), R(block); float* chans[2] = { L.data(), R.data() };
    clap_audio_buffer_t out{}; out.data32 = chans; out.channel_count = 2;
    auto run = [&](double seconds, EvList ev) -> Stats {
        Stats st; const int blocks = (int)(seconds * sr / block);
        for (int b = 0; b < blocks; ++b) {
            EvList cur;
            for (auto& e : ev.evs) { auto* h = (clap_event_header_t*)e.data(); if (h->time >= (uint32_t)b * block && h->time < (uint32_t)(b + 1) * block) { std::vector<uint8_t> c = e; ((clap_event_header_t*)c.data())->time -= (uint32_t)b * block; cur.evs.push_back(c); } }
            clap_input_events_t in{ &cur, inSize, inGet }; clap_output_events_t o{ nullptr, outPush };
            clap_process_t pr{}; pr.steady_time = -1; pr.frames_count = block; pr.transport = nullptr; pr.audio_outputs = &out; pr.audio_outputs_count = 1; pr.in_events = &in; pr.out_events = &o;
            plug->process(plug, &pr);
            for (uint32_t i = 0; i < block; ++i) { if (!std::isfinite(L[i])) st.finite = false; st.peak = std::max(st.peak, std::fabs(L[i])); }
        }
        return st;
    };
    { EvList e; auto s = run(0.3, e); check(s.peak == 0, "silent with no notes"); }
    { EvList e; e.note(0, true, 60); e.note(40000, false, 60); auto s = run(1.5, e); std::printf("      built-in note: peak %.3f\n", s.peak); check(s.finite && s.peak > 0.03f, "a key plays the built-in sample through the first sound"); }
    { EvList e; e.param(0, findParam("Play"), 1); auto s = run(1.0, e); check(s.peak > 0.03f, "Play parameter plays the sample"); EvList off; off.param(0, findParam("Play"), 0); run(3.0, off); }
    plug->stop_processing(plug);
    Buf saved; clap_ostream_t os{ &saved, osWrite }; check(state->save(plug, &os), "state save");
    if (wav) {
        std::string st = saved.data; const std::string key = std::string("sample\0", 7);
        size_t p = st.find(key); check(p != std::string::npos, "state has sample");
        if (p != std::string::npos) { const size_t vs = p + key.size(); st.replace(vs, st.find('\0', vs) - vs, wav); }
        Buf in; in.data = st; clap_istream_t is{ &in, isRead }; check(state->load(plug, &is), "state load with a sample");
    }
    plug->start_processing(plug);
    { EvList e; e.note(0, true, 60); e.note(40000, false, 60); auto s = run(1.5, e); std::printf("      loaded sample: peak %.3f\n", s.peak); check(s.finite && s.peak > 0.02f, "plays the loaded sample"); }
    run(3.0, EvList());

#ifndef NO_GUI_TEST
    if (gui) {
        Display* dpy = XOpenDisplay(nullptr);
        guiExt->create(plug, CLAP_WINDOW_API_X11, false);
        uint32_t w = 0, h = 0; guiExt->get_size(plug, &w, &h);
        Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, w, h, 0, 0, 0); XMapWindow(dpy, win); XFlush(dpy);
        clap_window_t cw{}; cw.api = CLAP_WINDOW_API_X11; cw.x11 = win;
        guiExt->set_parent(plug, &cw); guiExt->show(plug);
        auto pump = [&](int n) { for (int i = 0; i < n; ++i) { run(block / sr, EvList()); for (auto& t : gTimers) timerExt->on_timer(plug, t.id); while (XPending(dpy)) { XEvent xe; XNextEvent(dpy, &xe); } std::this_thread::sleep_for(std::chrono::milliseconds(16)); } };
        pump(5); guiExt->get_size(plug, &w, &h); XResizeWindow(dpy, win, w, h); XFlush(dpy); pump(5);
        std::printf("      gui %ux%u\n", w, h);
        auto xdo = [&](std::string a) { if (std::system(("xdotool " + a + " >/dev/null 2>&1").c_str())) {} pump(8); };
        auto shot = [&](const char* n) { std::string c = std::string("import -window root -crop ") + std::to_string(w) + "x" + std::to_string(h) + "+0+0 " + argv[3] + "_" + n + ".png"; if (std::system(c.c_str())) {} };
        xdo("mousemove 111 354 click 1");                       // category SHIMMER & SPACE (row 3: ALL, NEW, GRANULAR, SHIMMER)
        xdo("mousemove 340 336 click 1");                       // 3rd sound in it
        check(val("Macro Set") == 17, "click a sound in the browser loads it (" + std::to_string(val("Macro Set")) + ")");
        const double m1 = val("Macro 1");
        xdo("mousemove 815 380 mousedown 1"); xdo("mousemove 815 340"); xdo("mousemove 815 300 mouseup 1");
        check(val("Macro 1") > m1 + 0.2, "drag the big macro knob (" + std::to_string(m1) + " -> " + std::to_string(val("Macro 1")) + ")");
        xdo("mousemove 862 31 click 1"); check(val("Play") == 1, "PLAY button");
        { float pk = 0; for (int i = 0; i < 60; ++i) { auto s = run(block / sr, EvList()); pk = std::max(pk, s.peak); } check(pk > 0.02f, "PLAY makes sound"); }
        pump(20); shot("sounds");
        xdo("mousemove 111 277 click 1");                       // ALL SOUNDS
        xdo("mousemove 470 450 click 5"); xdo("mousemove 470 450 click 5"); pump(4);   // scroll down the list
        shot("all");
        xdo("mousemove 293 31 click 1"); check(val("Original Sound") == 1, "ORIGINAL on"); pump(6); shot("original");
        xdo("mousemove 293 31 click 1"); check(val("Original Sound") == 0, "ORIGINAL off");
        const double g0 = val("Grain On");
        xdo("mousemove 80 723 click 1"); check(val("Grain On") != g0, "chain chip toggles the GRAIN module");
        xdo("mousemove 247 211 click 1");                       // EDIT page
        const double r0 = val("Reverb On");
        xdo("mousemove 1052 559 click 1"); check(val("Reverb On") != r0, "EDIT page: REVERB power switch");
        xdo("mousemove 1052 559 click 1");
        pump(10); shot("edit");
        xdo("mousemove 90 211 click 1");                        // back to SOUNDS
        const double ms = val("Macro Set");
        xdo("mousemove 784 31 click 1"); check(val("Macro Set") != ms, "SURPRISE picks another sound");
        // ---- TIMELINE page: A at the start, B from the middle, C near the end
        const double cur0 = val("Macro Set");
        xdo("mousemove 403 211 click 1");
        check(val("Section 1 Sound") == cur0, "TIMELINE starts with the current sound as section 1");
        xdo("mousemove 550 330 click --repeat 2 --delay 80 1");
        check(val("Timeline Sections") == 2 && std::fabs(val("Section 2 Start") - 0.5) < 0.01 && val("Timeline On") == 1,
              "double-click in the middle adds section 2 there (" + std::to_string(val("Section 2 Start")) + ")");
        xdo("mousemove 171 496 click 1");                                   // category SPACE
        xdo("mousemove 138 549 click 1");                                   // its first sound
        check(val("Section 2 Sound") == 15, "clicking a sound gives it to section 2 (" + std::to_string(val("Section 2 Sound")) + ")");
        xdo("mousemove 348 549 click 1");                                   // play button on the 2nd sound
        check(val("Section 2 Sound") == 16 && val("Play") == 1 && std::fabs(val("Play From") - 0.5) < 0.01, "a sound's play button picks it and plays from the section start");
        { float pk = 0; for (int i = 0; i < 80; ++i) { auto s = run(block / sr, EvList()); pk = std::max(pk, s.peak); } check(pk > 0.02f, "the timeline makes sound"); }
        xdo("mousemove 850 330 click --repeat 2 --delay 80 1");
        check(val("Timeline Sections") == 3, "a second double-click adds section 3");
        xdo("mousemove 625 394 mousedown 1"); xdo("mousemove 650 394"); xdo("mousemove 680 394 mouseup 1");
        check(val("Section 2 Fade In") > 0.7, "dragging a white dot makes that fade longer (" + std::to_string(val("Section 2 Fade In")) + ")");
        xdo("mousemove 786 426 click 1"); check(val("Timeline Curve") == 2, "CURVE: SHARP");
        xdo("mousemove 1013 426 click 1"); check(val("Timeline Tails") == 1, "TAILS CUT");
        pump(20); shot("timeline");
        // ---- 1.3: EDIT with the timeline on edits the selected section; SAVE AS OWN PRESET
        {
            const double snd2 = val("Section 3 Sound");
            xdo("mousemove 247 211 click 1");                                   // EDIT page (section 3 is selected)
            xdo("mousemove 1052 559 click 1");                                  // REVERB power of section 3
            pump(10); shot("edit_section");
            Buf st; clap_ostream_t so{ &st, osWrite }; state->save(plug, &so);
            const std::string sv(st.data.begin(), st.data.end());
            check(sv.find("2:r_on=0") != std::string::npos && val("Section 3 Sound") == snd2,
                  "EDIT with the timeline on changes section 3's own copy (saved with the project)");
            if (std::system("rm -f \"$HOME/Documents/Lumen/Own Presets.txt\"")) {}
            xdo("mousemove 1009 211 click 1");                                  // SAVE AS OWN PRESET
            pump(6); shot("naming");
            xdo("type --delay 60 'Test Sound'"); xdo("key Return"); pump(10);
            std::string lib; { std::FILE* f = std::fopen((std::string(std::getenv("HOME")) + "/Documents/Lumen/Own Presets.txt").c_str(), "rb"); if (f) { char b[4096]; size_t n; while ((n = std::fread(b, 1, sizeof b, f)) > 0) lib.append(b, n); std::fclose(f); } }
            check(lib.find("|Test Sound|") != std::string::npos && lib.find("r_on=0") != std::string::npos && val("Section 3 Sound") >= 300,
                  "SAVE AS OWN PRESET: type a name, Enter -> saved to Documents/Lumen with the edit, and given to section 3 (" + std::to_string(val("Section 3 Sound")) + ")");
            xdo("mousemove 403 211 click 1"); xdo("mousemove 851 520 click 1");   // TIMELINE page, OWN chip
            pump(10); shot("own_picker");
        }
        xdo("mousemove 90 211 click 1"); pump(6); shot("sounds_tl");     // SOUNDS page while the timeline is on
        xdo("mousemove 403 211 click 1");
        xdo("mousemove 850 330 click 3"); check(val("Timeline Sections") == 2, "right-click a line removes that section");
        xdo("mousemove 1028 250 click 1"); check(val("Timeline On") == 0, "timeline ON button off");
        xdo("mousemove 1028 250 click 1"); check(val("Timeline On") == 1, "timeline ON button on");
        xdo("mousemove 90 211 click 1");
        xdo("mousemove 758 77 click 1"); check(val("Loop") == 0, "LOOP button off");
        xdo("mousemove 758 77 click 1");
        xdo("mousemove 862 31 click 1"); check(val("Play") == 0, "PLAY off");
        // ---- 1.2.1: STOP is a hard stop, also for held keys; click the waveform to play from there
        { EvList e; e.note(0, true, 64); run(1.0, e); pump(4); }
        const double st0 = val("Stop All");
        xdo("mousemove 862 31 click 1");                                   // the button shows STOP while the key sounds
        check(val("Stop All") == st0 + 1, "STOP button while a key is held sends a hard stop");
        { run(0.05, EvList()); auto s = run(0.5, EvList()); check(s.peak == 0, "after STOP: total silence, even though the key is still down (peak " + std::to_string(s.peak) + ")"); }
        { EvList e; e.note(0, false, 64); run(0.1, e); }
        pump(30);
        xdo("mousemove 550 126 click 1");
        check(val("Play") == 1 && std::fabs(val("Play From") - 0.5) < 0.01, "left-click in the middle of the waveform plays from there (" + std::to_string(val("Play From")) + ")");
        { auto s = run(0.5, EvList()); check(s.peak > 0.02f, "and it sounds"); }
        xdo("mousemove 300 126 click 3");
        check(std::fabs(val("Start") - (300.0 - 28) / 1044) < 0.01, "right-click moves START there (" + std::to_string(val("Start")) + ")");
        xdo("mousemove 700 126 click 3");
        check(std::fabs(val("End") - (700.0 - 28) / 1044) < 0.01, "right-click near the end moves END there (" + std::to_string(val("End")) + ")");
        pump(10); shot("region");
        xdo("mousemove 300 82 click 3"); check(val("Start") == 0, "right-click on the start triangle resets it");
        xdo("mousemove 700 84 click 3"); check(val("End") == 1, "right-click on the end triangle resets it (" + std::to_string(val("End")) + ")");
        xdo("mousemove 862 31 click 1"); check(val("Play") == 0, "STOP after click-to-play");
        pump(30);
        if (std::system("rm -rf \"$HOME/Documents/Grainfield Renders\"")) {}
        // the render runs in the background now: wait for the file
        auto newestWav = [&]() -> std::string {
            std::string out; FILE* p = popen("ls -t \"$HOME/Documents/Grainfield Renders\"/Lumen*.wav 2>/dev/null | head -1", "r");
            if (p) { char b[1024]; if (fgets(b, sizeof b, p)) { out = b; while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back(); } pclose(p); }
            return out; };
        auto wavRms = [](const std::string& path, double from, double to) -> double {   // 24-bit stereo WAV, seconds from..to
            std::FILE* f = std::fopen(path.c_str(), "rb"); if (!f) return -1;
            std::vector<unsigned char> d; unsigned char b[65536]; size_t n; while ((n = std::fread(b, 1, sizeof b, f)) > 0) d.insert(d.end(), b, b + n); std::fclose(f);
            size_t pos = 12, data = 0, len = 0; unsigned rate = 44100;
            while (pos + 8 <= d.size()) { const size_t sz = d[pos + 4] | d[pos + 5] << 8 | d[pos + 6] << 16 | (size_t)d[pos + 7] << 24;
                if (!std::memcmp(&d[pos], "fmt ", 4)) rate = d[pos + 12] | d[pos + 13] << 8 | d[pos + 14] << 16 | d[pos + 15] << 24;
                if (!std::memcmp(&d[pos], "data", 4)) { data = pos + 8; len = std::min(sz, d.size() - data); break; } pos += 8 + sz; }
            double s = 0; size_t cnt = 0;
            for (size_t fr = (size_t)(from * rate); fr < (size_t)(to * rate) && (fr + 1) * 6 <= len; ++fr) { const unsigned char* q = &d[data + fr * 6]; int v = q[0] | q[1] << 8 | q[2] << 16; if (v & 0x800000) v -= 0x1000000; s += (double)v * v; ++cnt; }
            return cnt ? std::sqrt(s / cnt) / 8388608.0 : 0; };
        auto renderNow = [&]() -> std::string {
            if (std::system("rm -rf \"$HOME/Documents/Grainfield Renders\"")) {}
            xdo("mousemove 951 31 click 1");
            for (int i = 0; i < 300 && newestWav().empty(); ++i) pump(2);
            pump(4); return newestWav(); };
        const std::string r1 = renderNow();
        check(!r1.empty(), "drag tile click renders a Lumen WAV (in the background)");
        // what you hear is what you drag: an edit (REVERB off on the EDIT page) must be in the file
        const double tail1 = wavRms(r1, 10.5, 12.5), body1 = wavRms(r1, 1, 8);
        xdo("mousemove 247 211 click 1"); xdo("mousemove 1052 559 click 1"); xdo("mousemove 90 211 click 1");
        const std::string r2 = renderNow();
        const double tail2 = wavRms(r2, 10.5, 12.5), body2 = wavRms(r2, 1, 8);
        std::printf("      render: body %.4f tail %.5f \xC2\xB7 after switching REVERB: body %.4f tail %.5f\n", body1, tail1, body2, tail2);
        check(!r2.empty() && r2 != r1 && (std::fabs(body2 - body1) > 0.03 * body1 || std::fabs(tail2 - tail1) > 0.5 * std::max(tail1, tail2) + 1e-6), "the dragged WAV contains the edit you just made (REVERB switched on the EDIT page)");
        xdo("mousemove 247 211 click 1"); xdo("mousemove 1052 559 click 1"); xdo("mousemove 90 211 click 1");
        // ---- 1.3: many sections (add them with + ADD SOUND), then look at both pages
        {
            Buf st; clap_ostream_t so{ &st, osWrite }; state->save(plug, &so);
            check(std::string(st.data.begin(), st.data.end()).find("|Test Sound|") != std::string::npos, "the project keeps a copy of the own preset it uses");
            xdo("mousemove 403 211 click 1");
            for (int i = 0; i < 40 && val("Timeline Sections") < 24; ++i) xdo("mousemove 92 426 click 1");
            check(val("Timeline Sections") >= 24, "+ ADD SOUND keeps adding sections past 6 (" + std::to_string(val("Timeline Sections")) + ")");
            { float pk = 0; xdo("mousemove 862 31 click 1"); for (int i = 0; i < 120; ++i) { auto s2 = run(block / sr, EvList()); pk = std::max(pk, s2.peak); } check(pk > 0.02f, "a timeline with 24 sections plays"); }
            pump(20); shot("many_timeline");
            xdo("mousemove 90 211 click 1"); pump(10); shot("many_sounds");
            xdo("mousemove 862 31 click 1");
        }
        guiExt->hide(plug); guiExt->destroy(plug); XDestroyWindow(dpy, win); XCloseDisplay(dpy);
    }
#else
    (void)gui; (void)timerExt; (void)guiExt;
#endif
    plug->stop_processing(plug); plug->deactivate(plug); plug->destroy(plug); entry->deinit(); dlclose(lib);
    std::printf("\n%s (%d failure%s)\n", failures ? "HOST TESTS FAILED" : "ALL HOST TESTS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
