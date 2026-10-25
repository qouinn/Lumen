// Lumen — your own presets: the library file (Documents/Lumen/Own Presets.txt) and the copy kept in each project.
// Shared by the plugin and its editor. Writers lock ownMutex(); the audio thread only reads the table (see LumenPresetTypes.hpp).
#pragma once

#include "LumenPresetTypes.hpp"
#include "Export.hpp"

#include <mutex>
#include <string>

namespace lm {

inline std::mutex& ownMutex() { static std::mutex m; return m; }
inline std::string ownLibraryPath()
{
#ifdef _WIN32
    return gf::documentsFolder("Lumen") + "\\Own Presets.txt";
#else
    return gf::documentsFolder("Lumen") + "/Own Presets.txt";
#endif
}

// read the library once per process (later calls do nothing)
inline void ownLoadLibrary()
{
    static bool done = false;
    std::lock_guard<std::mutex> lk(ownMutex());
    if (done) return;
    done = true;
    std::string text;
    if (!gf::readTextFile(ownLibraryPath(), text)) return;
    size_t a = 0;
    while (a < text.size()) {
        size_t b = text.find('\n', a); if (b == std::string::npos) b = text.size();
        std::string line = text.substr(a, b - a);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line[0] != '#') ownFromLine(line, 1, false);
        a = b + 1;
    }
}

inline bool ownWriteLibraryLocked()
{
    std::string out = "# Lumen own presets - one sound per line. You can copy this file to another computer.\n";
    for (int i = 0; i < kOwnMax; ++i) if (ownTable()[i].state.load() == 1) out += ownToLine(i, ownTable()[i]) + "\n";
    return gf::writeTextFile(ownLibraryPath(), out);
}

// save a new own preset from full settings p (a sound's effective values: module settings, macros, trim, base sound)
// returns its sound number, or -1 when the library is full / the file can't be written
inline int ownAdd(const char* name, const float* p, int base)
{
    std::lock_guard<std::mutex> lk(ownMutex());
    int slot = -1;
    for (int i = 0; i < kOwnMax; ++i) if (ownTable()[i].state.load() == 0) { slot = i; break; }
    if (slot < 0) return -1;
    OwnPreset& o = ownTable()[slot];
    std::snprintf(o.name, sizeof o.name, "%s", name && *name ? name : "My sound");
    for (char& c : o.name) if (c == '|' || c == '\n' || c == '\r') c = ' ';
    o.base = base < 0 || base >= kNumPresets ? 0 : base;
    for (int k = 0; k < kNumEdit; ++k) o.v[k] = p[kEditFirst + (uint32_t)k];
    for (int m = 0; m < 4; ++m) o.mdef[m] = clampf(p[kM1 + (uint32_t)m], 0, 1);
    o.trim = p[kTrim];
    ownFinish(o);
    o.state.store(1, std::memory_order_release);
    if (!ownWriteLibraryLocked()) { o.state.store(0); return -1; }
    return kOwnBase + slot;
}

// take it out of the library; it keeps working in this session (and in projects that carry a copy)
inline bool ownRemove(int idx)
{
    std::lock_guard<std::mutex> lk(ownMutex());
    if (!isOwn(idx)) return false;
    OwnPreset& o = ownTable()[idx - kOwnBase];
    if (o.state.load() != 1) return false;
    o.state.store(2, std::memory_order_release);
    return ownWriteLibraryLocked();
}

// how many own presets are in the library, and the n-th one's sound number
inline int ownCount() { int n = 0; for (int i = 0; i < kOwnMax; ++i) if (ownTable()[i].state.load() == 1) ++n; return n; }
inline int ownNth(int n) { for (int i = 0; i < kOwnMax; ++i) if (ownTable()[i].state.load() == 1 && n-- == 0) return kOwnBase + i; return -1; }

// project copy: the own presets this project uses (current sound + timeline sections), one line each
inline std::string ownProjectText(const float* p)
{
    std::lock_guard<std::mutex> lk(ownMutex());
    bool used[kOwnMax] = { false };
    auto mark = [&](float v) { const int i = (int)v; if (ownAt(i)) used[i - kOwnBase] = true; };
    mark(p[kMacroSet]);
    for (int s = 0; s < kMaxSections; ++s) mark(p[slotParam(s, kSfSound)]);
    std::string out;
    for (int i = 0; i < kOwnMax; ++i) if (used[i]) out += ownToLine(i, ownTable()[i]) + "\n";
    return out;
}
// a project brings its own presets along: slots that are free on this computer get them (for this session).
// A slot that already holds one of your library sounds is never overwritten, so nothing in your library can be lost.
inline void ownFromProject(const char* text)
{
    if (!text) return;
    std::lock_guard<std::mutex> lk(ownMutex());
    std::string t = text; size_t a = 0;
    while (a < t.size()) {
        size_t b = t.find('\n', a); if (b == std::string::npos) b = t.size();
        const std::string line = t.substr(a, b - a);
        a = b + 1;
        if (line.empty()) continue;
        const int slot = std::atoi(line.c_str());
        if (slot < 0 || slot >= kOwnMax) continue;
        if (ownTable()[slot].state.load() == 0) ownFromLine(line, 2, false);   // not in this computer's library: use it here
    }
}

} // namespace lm
