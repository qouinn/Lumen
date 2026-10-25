// Grainfield — render export helpers

#include "Export.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <algorithm>

#ifdef _WIN32
# ifndef NOMINMAX
#  define NOMINMAX
# endif
# include <windows.h>
# include <ole2.h>
# include <shlobj.h>
# include <shellapi.h>
#else
# include <sys/stat.h>
#endif

namespace gf {

#ifdef _WIN32
static std::wstring widen(const std::string& s)
{
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    w.resize((size_t)n - 1);
    return w;
}
static std::string narrow(const wchar_t* w)
{
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return std::string();
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    s.resize((size_t)n - 1);
    return s;
}
#endif

bool writeWav24(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, unsigned rate)
{
    FILE* f = nullptr;
#ifdef _WIN32
    f = _wfopen(widen(path).c_str(), L"wb");
#else
    f = std::fopen(path.c_str(), "wb");
#endif
    if (!f) return false;
    const uint32_t n = (uint32_t)L.size();
    const uint32_t dataBytes = n * 2 * 3;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w32(36 + dataBytes); std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(2); w32(rate); w32(rate * 6); w16(6); w16(24);
    std::fwrite("data", 1, 4, f); w32(dataBytes);
    std::vector<unsigned char> buf;
    buf.reserve(6 * 4096);
    // gentle safety: normalise only if the render clips
    float peak = 0.0f;
    for (uint32_t i = 0; i < n; ++i) peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
    const float g = peak > 0.989f ? 0.989f / peak : 1.0f;
    for (uint32_t i = 0; i < n; ++i) {
        for (int ch = 0; ch < 2; ++ch) {
            float x = (ch ? R[i] : L[i]) * g;
            x = x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);
            const int32_t v = (int32_t)std::lround(x * 8388607.0f);
            buf.push_back((unsigned char)(v & 0xff));
            buf.push_back((unsigned char)((v >> 8) & 0xff));
            buf.push_back((unsigned char)((v >> 16) & 0xff));
        }
        if (buf.size() >= 6 * 4096) { std::fwrite(buf.data(), 1, buf.size(), f); buf.clear(); }
    }
    if (!buf.empty()) std::fwrite(buf.data(), 1, buf.size(), f);
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    return ok;
}

std::string rendersFolder()
{
#ifdef _WIN32
    wchar_t docs[MAX_PATH] = { 0 };
    std::wstring dir;
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL | CSIDL_FLAG_CREATE, nullptr, 0, docs)))
        dir = std::wstring(docs) + L"\\Grainfield Renders";
    else {
        wchar_t tmp[MAX_PATH] = { 0 };
        GetTempPathW(MAX_PATH, tmp);
        dir = std::wstring(tmp) + L"Grainfield Renders";
    }
    CreateDirectoryW(dir.c_str(), nullptr);
    return narrow(dir.c_str());
#else
    const char* home = std::getenv("HOME");
    std::string base = home ? home : "/tmp";
    std::string docs = base + "/Documents";
    struct stat st;
    if (stat(docs.c_str(), &st) != 0) docs = base;
    std::string dir = docs + "/Grainfield Renders";
    mkdir(dir.c_str(), 0755);
    return dir;
#endif
}

#ifdef _WIN32
// ------------------------------------------------------------------------------------------
// Minimal OLE data object carrying CF_HDROP (what Explorer provides when dragging a file)

class FileDropSource : public IDropSource {
    LONG refs = 1;
public:
    virtual ~FileDropSource() {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropSource) { *ppv = static_cast<IDropSource*>(this); AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    ULONG STDMETHODCALLTYPE Release() override { const LONG r = InterlockedDecrement(&refs); if (r == 0) delete this; return (ULONG)r; }
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override {
        if (escape) return DRAGDROP_S_CANCEL;
        if (!(keys & MK_LBUTTON)) return DRAGDROP_S_DROP;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
};

class FileDataObject : public IDataObject {
    LONG refs = 1;
    HGLOBAL hdrop = nullptr;
public:
    explicit FileDataObject(const std::wstring& path) {
        const size_t bytes = sizeof(DROPFILES) + (path.size() + 2) * sizeof(wchar_t);
        hdrop = GlobalAlloc(GHND, bytes);
        if (hdrop) {
            DROPFILES* df = (DROPFILES*)GlobalLock(hdrop);
            df->pFiles = sizeof(DROPFILES);
            df->fWide = TRUE;
            wchar_t* dst = (wchar_t*)((char*)df + sizeof(DROPFILES));
            std::memcpy(dst, path.c_str(), path.size() * sizeof(wchar_t));
            dst[path.size()] = 0; dst[path.size() + 1] = 0;
            GlobalUnlock(hdrop);
        }
    }
    virtual ~FileDataObject() { if (hdrop) GlobalFree(hdrop); }
    bool ok() const { return hdrop != nullptr; }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDataObject) { *ppv = static_cast<IDataObject*>(this); AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    ULONG STDMETHODCALLTYPE Release() override { const LONG r = InterlockedDecrement(&refs); if (r == 0) delete this; return (ULONG)r; }

    static bool isHdrop(const FORMATETC* f) { return f && f->cfFormat == CF_HDROP && (f->tymed & TYMED_HGLOBAL) && f->dwAspect == DVASPECT_CONTENT; }

    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* fmt, STGMEDIUM* med) override {
        if (!isHdrop(fmt)) return DV_E_FORMATETC;
        const SIZE_T sz = GlobalSize(hdrop);
        HGLOBAL copy = GlobalAlloc(GHND, sz);
        if (!copy) return E_OUTOFMEMORY;
        std::memcpy(GlobalLock(copy), GlobalLock(hdrop), sz);
        GlobalUnlock(copy); GlobalUnlock(hdrop);
        med->tymed = TYMED_HGLOBAL; med->hGlobal = copy; med->pUnkForRelease = nullptr;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* fmt) override { return isHdrop(fmt) ? S_OK : DV_E_FORMATETC; }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* out) override { if (out) out->ptd = nullptr; return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dir, IEnumFORMATETC** e) override {
        if (dir != DATADIR_GET) return E_NOTIMPL;
        FORMATETC f = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        return SHCreateStdEnumFmtEtc(1, &f, e);
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }
};

bool canDragFiles() { return true; }

bool dragFileOut(const std::string& utf8Path)
{
    const HRESULT init = OleInitialize(nullptr); // S_FALSE if already initialised on this thread
    const bool needUninit = SUCCEEDED(init);
    FileDataObject* data = new FileDataObject(widen(utf8Path));
    FileDropSource* src = new FileDropSource();
    DWORD effect = DROPEFFECT_NONE;
    HRESULT hr = E_FAIL;
    if (data->ok())
        hr = DoDragDrop(data, src, DROPEFFECT_COPY | DROPEFFECT_LINK, &effect);
    data->Release();
    src->Release();
    if (needUninit) OleUninitialize();
    return hr == DRAGDROP_S_DROP;
}

void returnKeyboardFocus(uintptr_t pluginWindow)
{
    HWND h = (HWND)pluginWindow;
    if (h != nullptr && GetFocus() == h) { HWND p = GetParent(h); if (p != nullptr) SetFocus(p); }
}

void revealFile(const std::string& utf8Path)
{
    const std::wstring args = L"/select,\"" + widen(utf8Path) + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
}

#else

bool canDragFiles() { return false; }
bool dragFileOut(const std::string&) { return false; }
void returnKeyboardFocus(uintptr_t) {}

void revealFile(const std::string& utf8Path)
{
    std::string dir = utf8Path.substr(0, utf8Path.find_last_of('/'));
# ifdef __APPLE__
    std::string cmd = "open -R \"" + utf8Path + "\" &";
# else
    std::string cmd = "xdg-open \"" + dir + "\" >/dev/null 2>&1 &";
# endif
    if (std::system(cmd.c_str()) != 0) {}
}

#endif

} // namespace gf
