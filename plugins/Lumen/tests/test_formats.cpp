// Loads every test file and checks it decodes to ~4 s with 330 Hz on the left and 495 Hz on the right.
//   test_formats <folder>     (files made by tests/make_format_files.sh)
#include "AudioFile.hpp"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
static double tone(const std::vector<float>& x, double f, double sr) {   // Goertzel power around the middle second
    const size_t a = (size_t)(1.5 * sr), b = std::min(x.size(), (size_t)(2.5 * sr)); if (b <= a) return 0;
    const double w = 2 * M_PI * f / sr, c = 2 * std::cos(w); double s1 = 0, s2 = 0;
    for (size_t i = a; i < b; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / (double)(b - a);
}
int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    struct T { const char* file; bool stereo; bool expect; };
    const T tests[] = {
        { "ref.wav", true, true }, { "ref.aiff", true, true }, { "ref.flac", true, true }, { "ref.mp3", true, true }, { "ref.ogg", true, true },
        { "ref.opus", true, true }, { "ref.m4a", true, true }, { "alac.m4a", true, true }, { "ref.aac", true, true }, { "ref.wma", true, true },
        { "video.mp4", true, true }, { "video.mov", true, true }, { "video.mkv", true, true }, { "video.webm", true, true }, { "video.avi", true, true },
        { "video.wmv", true, true }, { "surround.m4a", false, true }, { "noaudio.mp4", false, false }, { "notaudio.txt", false, false },
    };
    int fails = 0;
    for (const T& t : tests) {
        gf::SampleData s; const std::string p = dir + "/" + t.file;
        const bool ok = gf::loadAudioFile(p.c_str(), s);
        const double secs = ok ? s.frames() / s.rate : 0;
        const double l330 = ok ? tone(s.L, 330, s.rate) : 0, l495 = ok ? tone(s.L, 495, s.rate) : 0, r495 = ok ? tone(s.R, 495, s.rate) : 0, r330 = ok ? tone(s.R, 330, s.rate) : 0;
        bool good;
        if (!t.expect) good = !ok;
        else good = ok && std::fabs(secs - 4.0) < 0.35 && l330 > 0.01 && (t.stereo ? (l330 > 5 * l495 && r495 > 5 * r330) : r330 > 0.01);
        std::printf("[%s] %-13s %s  %.2f s @ %.0f Hz  L330 %.3f L495 %.3f R495 %.3f\n", good ? " OK " : "FAIL", t.file, ok ? "loaded" : "refused", secs, ok ? s.rate : 0.0, l330, l495, r495);
        if (!good) ++fails;
    }
    std::printf("%s (%d failures)\n", fails ? "FORMAT TESTS FAILED" : "ALL FORMAT TESTS PASSED", fails);
    return fails ? 1 : 0;
}
