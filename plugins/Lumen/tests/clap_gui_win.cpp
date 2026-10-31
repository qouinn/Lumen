// Windows GUI test host (run under Wine or on Windows).
// Opens the plugin's editor in a Win32 window, runs audio, and exposes a drop-target window
// that records files dropped on it (stands in for a DAW). Mouse input is driven externally.
// usage: clap_gui_win.exe plugin.clap seconds report.txt
#define NOMINMAX
#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
#include "clap/entry.h"
#include "clap/plugin-factory.h"
#include "clap/ext/params.h"
#include "clap/ext/gui.h"
#include "clap/ext/timer-support.h"
#include "clap/ext/state.h"
#include <shlobj.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

struct Timer { clap_id id; uint32_t period; };
static std::vector<Timer> gTimers;
static bool CLAP_ABI regTimer(const clap_host_t*, uint32_t period, clap_id* id) { *id = (clap_id)gTimers.size() + 1; gTimers.push_back({ *id, period }); return true; }
static bool CLAP_ABI unregTimer(const clap_host_t*, clap_id) { return true; }
static const clap_host_timer_support_t gTimerExt = { regTimer, unregTimer };
static HWND gParent = nullptr;
static void CLAP_ABI hgResize(const clap_host_t*) {}
static bool CLAP_ABI hgReq(const clap_host_t*, uint32_t w, uint32_t h) { if (gParent) SetWindowPos(gParent, nullptr, 0, 0, (int)w, (int)h, SWP_NOMOVE | SWP_NOZORDER); return true; }
static bool CLAP_ABI hgShow(const clap_host_t*) { return true; }
static void CLAP_ABI hgClosed(const clap_host_t*, bool) {}
static const clap_host_gui_t gGuiExt = { hgResize, hgReq, hgShow, hgShow, hgClosed };
static const void* CLAP_ABI hostGetExt(const clap_host_t*, const char* id) {
    if (!std::strcmp(id, CLAP_EXT_TIMER_SUPPORT)) return &gTimerExt;
    if (!std::strcmp(id, CLAP_EXT_GUI)) return &gGuiExt;
    return nullptr;
}
static void CLAP_ABI hostNoop(const clap_host_t*) {}
static clap_host_t gHost = { CLAP_VERSION, nullptr, "gf-win-gui-test", "test", "", "1.0", hostGetExt, hostNoop, hostNoop, hostNoop };

static uint32_t CLAP_ABI inSize(const clap_input_events_t*) { return 0; }
static const clap_event_header_t* CLAP_ABI inGet(const clap_input_events_t*, uint32_t) { return nullptr; }
static bool CLAP_ABI outPush(const clap_output_events_t*, const clap_event_header_t*) { return true; }

// ---- drop target recording dropped files
static std::wstring gDropped;
class Target : public IDropTarget {
    LONG refs = 1;
public:
    virtual ~Target() {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) { *ppv = this; AddRef(); return S_OK; } *ppv = nullptr; return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&refs); if (!r) delete this; return (ULONG)r; }
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject*, DWORD, POINTL, DWORD* e) override { *e = DROPEFFECT_COPY; return S_OK; }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD* e) override { *e = DROPEFFECT_COPY; return S_OK; }
    HRESULT STDMETHODCALLTYPE DragLeave() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* d, DWORD, POINTL, DWORD* e) override {
        FORMATETC f = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM m;
        if (SUCCEEDED(d->GetData(&f, &m))) {
            HDROP h = (HDROP)m.hGlobal;
            wchar_t buf[1024]; if (DragQueryFileW(h, 0, buf, 1024)) gDropped = buf;
            ReleaseStgMedium(&m);
        }
        *e = DROPEFFECT_COPY; return S_OK;
    }
};

// ---- a drag source (GFH_DRAGIN=file): pressing the mouse on it drags that file, like FL's browser or Explorer
static std::wstring gDragIn;
class SrcData : public IDataObject {
    LONG refs = 1; HGLOBAL hd = nullptr;
public:
    explicit SrcData(const std::wstring& p) {
        const SIZE_T bytes = sizeof(DROPFILES) + (p.size() + 2) * sizeof(wchar_t);
        hd = GlobalAlloc(GHND, bytes); DROPFILES* df = (DROPFILES*)GlobalLock(hd); df->pFiles = sizeof(DROPFILES); df->fWide = TRUE;
        memcpy((char*)df + sizeof(DROPFILES), p.c_str(), p.size() * sizeof(wchar_t)); GlobalUnlock(hd);
    }
    virtual ~SrcData() { GlobalFree(hd); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override { if (riid == IID_IUnknown || riid == IID_IDataObject) { *ppv = this; AddRef(); return S_OK; } *ppv = nullptr; return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&refs); if (!r) delete this; return (ULONG)r; }
    static bool ok(const FORMATETC* f) { return f && f->cfFormat == CF_HDROP && (f->tymed & TYMED_HGLOBAL); }
    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* f, STGMEDIUM* m) override {
        if (!ok(f)) return DV_E_FORMATETC;
        const SIZE_T sz = GlobalSize(hd); HGLOBAL c = GlobalAlloc(GHND, sz); memcpy(GlobalLock(c), GlobalLock(hd), sz); GlobalUnlock(c); GlobalUnlock(hd);
        m->tymed = TYMED_HGLOBAL; m->hGlobal = c; m->pUnkForRelease = nullptr; return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* f) override { return ok(f) ? S_OK : DV_E_FORMATETC; }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* o) override { o->ptd = nullptr; return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dir, IEnumFORMATETC** e) override {
        if (dir != DATADIR_GET) return E_NOTIMPL;
        static FORMATETC fe = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        return SHCreateStdEnumFmtEtc(1, &fe, e);
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }
};
class Src : public IDropSource {
    LONG refs = 1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override { if (riid == IID_IUnknown || riid == IID_IDropSource) { *ppv = this; AddRef(); return S_OK; } *ppv = nullptr; return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&refs); if (!r) delete this; return (ULONG)r; }
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL esc, DWORD keys) override { if (esc) return DRAGDROP_S_CANCEL; if (!(keys & MK_LBUTTON)) return DRAGDROP_S_DROP; return S_OK; }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};
static FILE* gRep = nullptr;
static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_LBUTTONDOWN && !gDragIn.empty() && GetPropW(h, L"gfsrc")) {
        SrcData* d = new SrcData(gDragIn); Src* s = new Src(); DWORD eff = 0;
        const HRESULT hr = DoDragDrop(d, s, DROPEFFECT_COPY, &eff);
        if (gRep) { fprintf(gRep, "dragin=%s\n", hr == DRAGDROP_S_DROP ? "dropped" : "cancelled"); fflush(gRep); }
        d->Release(); s->Release();
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int main(int argc, char** argv)
{
    if (argc < 4) return 2;
    const double seconds = atof(argv[2]);
    FILE* rep = fopen(argv[3], "w");
    OleInitialize(nullptr);
    SetProcessDPIAware();

    HMODULE lib = LoadLibraryA(argv[1]);
    if (!lib) { fprintf(rep, "LOADFAIL\n"); fclose(rep); return 1; }
    if (getenv("GFH_DPIAWARE")) {   // act like a per-monitor DPI-aware host (FL Studio is one)
        typedef BOOL(WINAPI* PSet)(HANDLE);
        if (PSet f = (PSet)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext")) f((HANDLE)-4);
    }
    auto entry = (const clap_plugin_entry_t*)GetProcAddress(lib, "clap_entry");
    entry->init(argv[1]);
    auto factory = (const clap_plugin_factory_t*)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    const clap_plugin_t* plug = factory->create_plugin(factory, &gHost, factory->get_plugin_descriptor(factory, 0)->id);
    plug->init(plug);
    auto params = (const clap_plugin_params_t*)plug->get_extension(plug, CLAP_EXT_PARAMS);
    auto gui = (const clap_plugin_gui_t*)plug->get_extension(plug, CLAP_EXT_GUI);
    auto timer = (const clap_plugin_timer_support_t*)plug->get_extension(plug, CLAP_EXT_TIMER_SUPPORT);

    WNDCLASSW wc = {}; wc.lpfnWndProc = wndProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"gfhost";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);
    gParent = CreateWindowExW(0, L"gfhost", L"Lumen", WS_POPUP | WS_VISIBLE, 0, 0, 1000, 720, nullptr, nullptr, wc.hInstance, nullptr);
    HWND drop = CreateWindowExW(WS_EX_TOPMOST, L"gfhost", L"Drop", WS_POPUP | WS_VISIBLE, 1100, 900, 400, 200, nullptr, nullptr, wc.hInstance, nullptr);
    Target* tgt = new Target();
    gRep = rep;
    if (const char* di = getenv("GFH_DRAGIN")) {   // a source window at (1100, 600) that drags this file
        wchar_t wb[1024]; MultiByteToWideChar(CP_UTF8, 0, di, -1, wb, 1024); gDragIn = wb;
        HWND src = CreateWindowExW(WS_EX_TOPMOST, L"gfhost", L"DragSrc", WS_POPUP | WS_VISIBLE, 1100, 600, 300, 150, nullptr, nullptr, wc.hInstance, nullptr);
        SetPropW(src, L"gfsrc", (HANDLE)1);
    }
    const HRESULT rr = RegisterDragDrop(drop, tgt);

    gui->create(plug, CLAP_WINDOW_API_WIN32, false);
    if (const char* hs = getenv("GFH_SETSCALE")) gui->set_scale(plug, atof(hs));   // the host announces a display scale
    clap_window_t cw = {}; cw.api = CLAP_WINDOW_API_WIN32; cw.win32 = gParent;
    gui->set_parent(plug, &cw);
    gui->show(plug);
    uint32_t w = 0, h = 0; gui->get_size(plug, &w, &h);
    if (const char* sc = getenv("GFH_HOSTSCALE"); sc && atof(sc) > 0) {   // host resizes the editor, like dragging FL's window edge
        const double f = atof(sc);
        uint32_t rw = (uint32_t)(w * f), rh = (uint32_t)(h * f);
        gui->adjust_size(plug, &rw, &rh);
        gui->set_size(plug, rw, rh);
        w = rw; h = rh;
    }
    SetWindowPos(gParent, nullptr, 0, 0, (int)w, (int)h, SWP_NOZORDER);
    { HDC dc = GetDC(gParent); fprintf(rep, "dpi=%d size=%ux%u regdrop=%ld\n", GetDeviceCaps(dc, LOGPIXELSX), w, h, (long)rr); ReleaseDC(gParent, dc); } fflush(rep);

    plug->activate(plug, 48000, 32, 256);
    plug->start_processing(plug);
    std::vector<float> L(256), R(256); float* ch[2] = { L.data(), R.data() };
    clap_audio_buffer_t out = {}; out.data32 = ch; out.channel_count = 2;
    float peak = 0;

    const DWORD t0 = GetTickCount();
    DWORD lastLog = t0; int iters = 0; DWORD maxMsg = 0, maxTimer = 0;
    while ((GetTickCount() - t0) < seconds * 1000) {
        DWORD a = GetTickCount();
        MSG msg; int nm = 0; while (nm++ < 8 && PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        DWORD b = GetTickCount();
        for (auto& t : gTimers) timer->on_timer(plug, t.id);
        DWORD c2 = GetTickCount();
        maxMsg = std::max(maxMsg, b - a); maxTimer = std::max(maxTimer, c2 - b); ++iters;
        if (c2 - lastLog > 1000) { fprintf(rep, "t=%lu iters=%d maxMsg=%lu maxTimer=%lu peak=%g\n", (unsigned long)(c2 - t0), iters, (unsigned long)maxMsg, (unsigned long)maxTimer, peak); fflush(rep); lastLog = c2; iters = 0; maxMsg = maxTimer = 0; }
        clap_input_events_t in = { nullptr, inSize, inGet }; clap_output_events_t o = { nullptr, outPush };
        for (int b = 0; b < (getenv("GFH_NOAUDIO") ? 0 : 2); ++b) {
            clap_process_t pr = {}; pr.steady_time = -1; pr.frames_count = 256; pr.audio_outputs = &out; pr.audio_outputs_count = 1;
            pr.in_events = &in; pr.out_events = &o;
            plug->process(plug, &pr);
            for (float v : L) peak = std::max(peak, std::fabs(v));
        }
        Sleep(10);
    }
    auto val = [&](const char* name) {
        const uint32_t n = params->count(plug);
        for (uint32_t i = 0; i < n; ++i) { clap_param_info_t inf; params->get_info(plug, i, &inf); if (!strcmp(inf.name, name)) { double v; params->get_value(plug, inf.id, &v); return v; } }
        return -999.0;
    };
    if (const char* list = getenv("GFH_PARAMS")) {
        std::string l = list; size_t a = 0;
        while (a < l.size()) { size_t b = l.find(',', a); if (b == std::string::npos) b = l.size(); std::string nm = l.substr(a, b - a); fprintf(rep, "%s=%g ", nm.c_str(), val(nm.c_str())); a = b + 1; }
        fprintf(rep, "peak=%g\n", peak);
    } else
    fprintf(rep, "mode=%g size=%g play=%g chops=%g env=%g region=%g peak=%g\n", val("Mode"), val("Chop 1 Size"), val("Play"), val("Chops"), val("Grain Envelope"), val("Region Start"), peak);
    if (const char* sk = getenv("GFH_STATEKEY")) {   // the value of a saved state key (e.g. the loaded sample)
        auto st = (const clap_plugin_state_t*)plug->get_extension(plug, CLAP_EXT_STATE);
        struct B { std::string d; } b;
        clap_ostream_t os = { &b, [](const clap_ostream_t* s, const void* buf, uint64_t n) -> int64_t { ((B*)s->ctx)->d.append((const char*)buf, (size_t)n); return (int64_t)n; } };
        if (st && st->save(plug, &os)) { const std::string k = std::string(sk) + std::string(1, '\0'); size_t p = b.d.find(k); fprintf(rep, "%s=%s\n", sk, p == std::string::npos ? "?" : b.d.substr(p + k.size(), b.d.find('\0', p + k.size()) - p - k.size()).c_str()); }
    }
    char dropped[1024] = { 0 }; WideCharToMultiByte(CP_UTF8, 0, gDropped.c_str(), -1, dropped, 1024, nullptr, nullptr);
    fprintf(rep, "dropped=%s\n", dropped);
    fclose(rep);
    gui->hide(plug); gui->destroy(plug);
    plug->stop_processing(plug); plug->deactivate(plug); plug->destroy(plug);
    entry->deinit();
    return 0;
}
