// Grainfield / Flock / Lumen — audio file loading
//
//   WAV, AIFF (dr_wav) · FLAC (dr_flac) · MP3 (dr_mp3) · Ogg Vorbis (stb_vorbis)      — built in, every system
//   everything else Windows can play — M4A/AAC, ALAC, WMA, and the audio track of videos (MP4, MOV, M4V, MKV, WebM,
//   AVI, WMV, 3GP, …) — through Windows Media Foundation. It is loaded only when needed, so the plugin still opens on
//   Windows versions without it (N editions). On Linux/macOS builds, ffmpeg (if installed) does that part.
//
// The format is recognised from the file's contents, not its name.

#define DR_WAV_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#define DR_FLAC_NO_STDIO
#define DR_MP3_NO_STDIO

#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-function"
# pragma GCC diagnostic ignored "-Wunused-parameter"
# pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
# pragma GCC diagnostic ignored "-Wsign-compare"
# pragma GCC diagnostic ignored "-Wunused-value"
# pragma GCC diagnostic ignored "-Wunused-variable"
# pragma GCC diagnostic ignored "-Wmisleading-indentation"
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include "ext/drlibs/dr_wav.h"
#include "ext/drlibs/dr_flac.h"
#include "ext/drlibs/dr_mp3.h"
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#include "ext/stb/stb_vorbis.c"
#undef L
#undef R
#undef C
#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

#include "AudioFile.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
# include <windows.h>
# include <mfapi.h>
# include <mfidl.h>
# include <mfreadwrite.h>
# include <mferror.h>
#endif

namespace gf {

// ---------------------------------------------------------------------------------------------
// interleaved float -> stereo SampleData (more than 2 channels are folded down)
static void toStereo(const float* pcm, unsigned long long frames, unsigned channels, std::vector<float>& L, std::vector<float>& R)
{
    const size_t base = L.size();
    L.resize(base + (size_t)frames); R.resize(base + (size_t)frames);
    for (unsigned long long i = 0; i < frames; ++i) {
        const float* f = pcm + i * channels;
        float l, r;
        if (channels == 1) l = r = f[0];
        else if (channels == 2) { l = f[0]; r = f[1]; }
        else {   // surround (FL FR C LFE BL BR …): fronts + centre + backs
            const float c = channels > 2 ? f[2] : 0.0f, bl = channels > 4 ? f[4] : 0.0f, br = channels > 5 ? f[5] : 0.0f;
            l = (f[0] + 0.707f * c + 0.707f * bl) * 0.58f;
            r = (f[1] + 0.707f * c + 0.707f * br) * 0.58f;
        }
        L[base + (size_t)i] = l; R[base + (size_t)i] = r;
    }
}

#ifdef _WIN32
static std::wstring widePath(const char* path)
{
    const int n = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (n <= 0) return std::wstring();
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path, -1, &w[0], n);
    w.resize((size_t)n - 1);
    return w;
}
#endif

static std::FILE* openRead(const char* path)
{
#ifdef _WIN32
    return _wfopen(widePath(path).c_str(), L"rb");
#else
    return std::fopen(path, "rb");
#endif
}

static bool readWholeFile(const char* path, std::vector<unsigned char>& data)
{
    std::FILE* f = openRead(path);
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > 1024L * 1024L * 1024L) { std::fclose(f); return false; }
    data.resize((size_t)size);
    const size_t got = std::fread(data.data(), 1, (size_t)size, f);
    std::fclose(f);
    return got == (size_t)size;
}

static size_t readHead(const char* path, unsigned char* buf, size_t n)
{
    std::FILE* f = openRead(path);
    if (!f) return 0;
    const size_t got = std::fread(buf, 1, n, f);
    std::fclose(f);
    return got;
}

// ---------------------------------------------------------------------------------------------
// built-in decoders (whole file in memory)
enum Kind { kNone, kWav, kFlac, kMp3, kVorbis, kOther };

static Kind sniff(const unsigned char* d, size_t n)
{
    if (n < 12) return kNone;
    if (!std::memcmp(d, "RIFF", 4) && n >= 12 && !std::memcmp(d + 8, "WAVE", 4)) return kWav;
    if (!std::memcmp(d, "RF64", 4) || !std::memcmp(d, "RIFX", 4) || !std::memcmp(d, "riff", 4)) return kWav;   // RF64 / big-endian / W64
    if (!std::memcmp(d, "FORM", 4) && (!std::memcmp(d + 8, "AIFF", 4) || !std::memcmp(d + 8, "AIFC", 4))) return kWav;
    if (!std::memcmp(d, "fLaC", 4)) return kFlac;
    if (!std::memcmp(d, "OggS", 4)) {   // Ogg: Vorbis is built in, Opus/FLAC-in-Ogg go to the system decoder
        for (size_t i = 0; i + 7 <= n && i < 200; ++i) if (d[i] == 0x01 && !std::memcmp(d + i + 1, "vorbis", 6)) return kVorbis;
        return kOther;
    }
    if (!std::memcmp(d, "ID3", 3)) return kMp3;                                        // (an AAC with an ID3 tag falls back below)
    if (d[0] == 0xFF && (d[1] & 0xE0) == 0xE0 && (d[1] & 0x06) != 0) return kMp3;      // MPEG audio frame (layer bits set; ADTS AAC has 00)
    return kOther;                                                                       // MP4/M4A/MOV, MKV/WebM, ASF/WMA, AVI, ADTS AAC, …
}

static bool decodeBuiltin(Kind kind, const std::vector<unsigned char>& data, double maxSeconds, SampleData& s)
{
    const unsigned char* d = data.data();
    unsigned int channels = 0, rate = 0; unsigned long long frames = 0; float* pcm = nullptr;
    if (kind == kWav) { drwav_uint64 n = 0; pcm = drwav_open_memory_and_read_pcm_frames_f32(d, data.size(), &channels, &rate, &n, nullptr); frames = n; }
    else if (kind == kFlac) { drflac_uint64 n = 0; pcm = drflac_open_memory_and_read_pcm_frames_f32(d, data.size(), &channels, &rate, &n, nullptr); frames = n; }
    else if (kind == kMp3) { drmp3_config cfg; std::memset(&cfg, 0, sizeof cfg); drmp3_uint64 n = 0; pcm = drmp3_open_memory_and_read_pcm_frames_f32(d, data.size(), &cfg, &n, nullptr); channels = cfg.channels; rate = cfg.sampleRate; frames = n; }
    else if (kind == kVorbis) {
        int err = 0;
        stb_vorbis* v = stb_vorbis_open_memory(d, (int)data.size(), &err, nullptr);
        if (!v) return false;
        const stb_vorbis_info info = stb_vorbis_get_info(v);
        channels = (unsigned)info.channels; rate = info.sample_rate;
        const unsigned long long cap = (unsigned long long)(maxSeconds * rate);
        std::vector<float> buf(4096 * (size_t)std::max(1u, channels));
        s.L.clear(); s.R.clear();
        while (channels > 0 && (unsigned long long)s.L.size() < cap) {
            const int got = stb_vorbis_get_samples_float_interleaved(v, (int)channels, buf.data(), (int)buf.size());
            if (got <= 0) break;
            toStereo(buf.data(), (unsigned long long)got, channels, s.L, s.R);
        }
        stb_vorbis_close(v);
        if (s.L.size() > cap) { s.L.resize((size_t)cap); s.R.resize((size_t)cap); }
        s.rate = rate;
        return rate > 0 && s.L.size() >= 16;
    }
    auto release = [&]() { if (!pcm) return; if (kind == kWav) drwav_free(pcm, nullptr); else if (kind == kFlac) drflac_free(pcm, nullptr); else drmp3_free(pcm, nullptr); };
    if (!pcm || channels == 0 || rate == 0 || frames < 16) { release(); return false; }
    frames = std::min(frames, (unsigned long long)(maxSeconds * rate));
    s.L.clear(); s.R.clear();
    toStereo(pcm, frames, channels, s.L, s.R);
    s.rate = rate;
    release();
    return true;
}

// ---------------------------------------------------------------------------------------------
// the system decoder: Windows Media Foundation (audio files and the audio track of videos)
#ifdef _WIN32
static bool decodeSystem(const char* path, double maxSeconds, SampleData& s)
{
    typedef HRESULT(WINAPI* PStartup)(ULONG, DWORD);
    typedef HRESULT(WINAPI* PShutdown)();
    typedef HRESULT(WINAPI* PCreateMediaType)(IMFMediaType**);
    typedef HRESULT(WINAPI* PCreateReader)(LPCWSTR, IMFAttributes*, IMFSourceReader**);
    HMODULE plat = LoadLibraryW(L"mfplat.dll"), rw = LoadLibraryW(L"mfreadwrite.dll");
    if (!plat || !rw) { if (plat) FreeLibrary(plat); if (rw) FreeLibrary(rw); return false; }
#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
    PStartup mfStartup = (PStartup)GetProcAddress(plat, "MFStartup");
    PShutdown mfShutdown = (PShutdown)GetProcAddress(plat, "MFShutdown");
    PCreateMediaType mfCreateMediaType = (PCreateMediaType)GetProcAddress(plat, "MFCreateMediaType");
    PCreateReader mfCreateReader = (PCreateReader)GetProcAddress(rw, "MFCreateSourceReaderFromURL");
#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif
    bool ok = false;
    const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);   // RPC_E_CHANGED_MODE: the thread already has COM, that's fine
    if (mfStartup && mfShutdown && mfCreateMediaType && mfCreateReader && SUCCEEDED(mfStartup(MF_VERSION, MFSTARTUP_LITE))) {
        IMFSourceReader* reader = nullptr;
        const std::wstring wp = widePath(path);
        if (!wp.empty() && SUCCEEDED(mfCreateReader(wp.c_str(), nullptr, &reader)) && reader) {
            const DWORD audio = (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM;
            reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);   // ignore the video
            IMFMediaType* want = nullptr;
            if (SUCCEEDED(reader->SetStreamSelection(audio, TRUE)) && SUCCEEDED(mfCreateMediaType(&want))) {
                want->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                want->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
                IMFMediaType* got = nullptr;
                if (SUCCEEDED(reader->SetCurrentMediaType(audio, nullptr, want)) && SUCCEEDED(reader->GetCurrentMediaType(audio, &got)) && got) {
                    UINT32 ch = 0, rate = 0;
                    got->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &ch);
                    got->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
                    got->Release();
                    if (ch > 0 && rate > 0) {
                        const size_t cap = (size_t)(maxSeconds * rate);
                        s.L.clear(); s.R.clear();
                        for (;;) {
                            DWORD flags = 0; LONGLONG ts = 0; IMFSample* smp = nullptr;
                            if (FAILED(reader->ReadSample(audio, 0, nullptr, &flags, &ts, &smp))) break;
                            if (smp) {
                                IMFMediaBuffer* mb = nullptr;
                                if (SUCCEEDED(smp->ConvertToContiguousBuffer(&mb)) && mb) {
                                    BYTE* p = nullptr; DWORD len = 0;
                                    if (SUCCEEDED(mb->Lock(&p, nullptr, &len)) && p) {
                                        toStereo((const float*)p, len / (sizeof(float) * ch), ch, s.L, s.R);
                                        mb->Unlock();
                                    }
                                    mb->Release();
                                }
                                smp->Release();
                            }
                            if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) || (flags & MF_SOURCE_READERF_ERROR) || s.L.size() >= cap) break;
                        }
                        if (s.L.size() > cap) { s.L.resize(cap); s.R.resize(cap); }
                        s.rate = rate;
                        ok = s.L.size() >= 16;
                    }
                }
                want->Release();
            }
            reader->Release();
        }
        mfShutdown();
    }
    if (co == S_OK || co == S_FALSE) CoUninitialize();
    FreeLibrary(rw); FreeLibrary(plat);
    return ok;
}
#else
// Linux / macOS builds: ffmpeg, if it is installed
static bool decodeSystem(const char* path, double maxSeconds, SampleData& s)
{
    std::string q = "'";
    for (const char* p = path; *p; ++p) { if (*p == '\'') q += "'\\''"; else q += *p; }
    q += "'";
    char cmd[64];
    std::snprintf(cmd, sizeof cmd, " -vn -t %.0f -f f32le -ac 2 -ar 48000 - 2>/dev/null", maxSeconds);
    const std::string full = "ffmpeg -nostdin -v error -i " + q + cmd;
    std::FILE* p = popen(full.c_str(), "r");
    if (!p) return false;
    std::vector<float> buf(8192);
    s.L.clear(); s.R.clear();
    size_t got;
    while ((got = std::fread(buf.data(), sizeof(float), buf.size(), p)) >= 2) toStereo(buf.data(), got / 2, 2, s.L, s.R);
    pclose(p);
    s.rate = 48000;
    return s.L.size() >= 16;
}
#endif

// ---------------------------------------------------------------------------------------------
bool loadAudioFile(const char* path, SampleData& out, double maxSeconds)
{
    if (!path || !*path) return false;
    unsigned char head[256] = { 0 };
    const size_t hn = readHead(path, head, sizeof head);
    if (hn < 12) return false;
    const Kind kind = sniff(head, hn);

    SampleData s;
    bool ok = false;
    if (kind != kOther) {
        std::vector<unsigned char> data;
        if (readWholeFile(path, data)) ok = decodeBuiltin(kind, data, maxSeconds, s);
    }
    if (!ok) ok = decodeSystem(path, maxSeconds, s);                 // M4A, videos, WMA, Opus, … (and anything the above refused)
    if (!ok && kind == kOther) {                                      // last try: an MP3 without a clean start
        const char* ext = std::strrchr(path, '.');
        if (ext && (!std::strcmp(ext, ".mp3") || !std::strcmp(ext, ".MP3"))) {
            std::vector<unsigned char> data;
            if (readWholeFile(path, data)) ok = decodeBuiltin(kMp3, data, maxSeconds, s);
        }
    }
    if (!ok || s.rate <= 0 || s.L.size() < 16) return false;
    out.L.swap(s.L);
    out.R.swap(s.R);
    out.rate = s.rate;
    return true;
}

std::string fileNameOf(const char* path)
{
    if (!path) return std::string();
    const char* last = path;
    for (const char* p = path; *p; ++p) if (*p == '/' || *p == '\\') last = p + 1;
    return std::string(last);
}

} // namespace gf
