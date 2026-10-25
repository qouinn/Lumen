// Grainfield — audio file loading (WAV, AIFF, FLAC, MP3)

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
#endif
#include "ext/drlibs/dr_wav.h"
#include "ext/drlibs/dr_flac.h"
#include "ext/drlibs/dr_mp3.h"
#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

#include "AudioFile.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
# include <windows.h>
#endif

namespace gf {

static bool readWholeFile(const char* path, std::vector<unsigned char>& data)
{
    FILE* f = nullptr;
#ifdef _WIN32
    const int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (wlen <= 0) return false;
    std::vector<wchar_t> wpath((size_t)wlen);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath.data(), wlen);
    f = _wfopen(wpath.data(), L"rb");
#else
    f = std::fopen(path, "rb");
#endif
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

bool loadAudioFile(const char* path, SampleData& out, double maxSeconds)
{
    if (!path || !*path) return false;
    std::vector<unsigned char> data;
    if (!readWholeFile(path, data) || data.size() < 12) return false;

    unsigned int channels = 0, rate = 0;
    unsigned long long frames = 0;
    float* pcm = nullptr;
    int kind = 0; // 1 wav, 2 flac, 3 mp3

    const unsigned char* d = data.data();
    if (!std::memcmp(d, "RIFF", 4) || !std::memcmp(d, "RF64", 4) || !std::memcmp(d, "FORM", 4) ||
        !std::memcmp(d, "RIFX", 4) || !std::memcmp(d, "riff", 4)) {
        drwav_uint64 n = 0;
        pcm = drwav_open_memory_and_read_pcm_frames_f32(d, data.size(), &channels, &rate, &n, nullptr);
        frames = n; kind = 1;
    } else if (!std::memcmp(d, "fLaC", 4)) {
        drflac_uint64 n = 0;
        pcm = drflac_open_memory_and_read_pcm_frames_f32(d, data.size(), &channels, &rate, &n, nullptr);
        frames = n; kind = 2;
    } else {
        drmp3_config cfg; std::memset(&cfg, 0, sizeof(cfg));
        drmp3_uint64 n = 0;
        pcm = drmp3_open_memory_and_read_pcm_frames_f32(d, data.size(), &cfg, &n, nullptr);
        channels = cfg.channels; rate = cfg.sampleRate; frames = n; kind = 3;
    }

    if (!pcm || channels == 0 || rate == 0 || frames < 16) {
        if (pcm) {
            if (kind == 1) drwav_free(pcm, nullptr);
            else if (kind == 2) drflac_free(pcm, nullptr);
            else drmp3_free(pcm, nullptr);
        }
        return false;
    }

    const unsigned long long maxFrames = (unsigned long long)(maxSeconds * rate);
    if (frames > maxFrames) frames = maxFrames;

    SampleData s;
    s.rate = rate;
    s.L.resize((size_t)frames);
    s.R.resize((size_t)frames);
    for (unsigned long long i = 0; i < frames; ++i) {
        const float* fr = pcm + i * channels;
        if (channels == 1) { s.L[i] = s.R[i] = fr[0]; }
        else { s.L[i] = fr[0]; s.R[i] = fr[1]; }
    }

    if (kind == 1) drwav_free(pcm, nullptr);
    else if (kind == 2) drflac_free(pcm, nullptr);
    else drmp3_free(pcm, nullptr);

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
