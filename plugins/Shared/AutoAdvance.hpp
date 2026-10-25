// Auto-advance: moves to the next section (chop / slice) every N bars or seconds.
// Shared by Grainfield and Flock. Bar mode follows the host transport when it plays,
// so the same bar always lands on the same section (arrangement-safe).
//
// Orders: FWD / PING / RAND move relative to the start section (the selected chop / slice under POS),
// LIST plays a user list of absolute section numbers (e.g. 26, 3, 8) and loops it.
#pragma once

#include <cmath>
#include <cstdint>
#include <climits>

namespace gf {

inline constexpr const char* kAdvEveryNames[] = { "1/2 BAR", "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS",
                                                  "1 SEC", "2 SEC", "4 SEC", "8 SEC", "15 SEC", "30 SEC" };
inline constexpr float kAdvBars[] = { 0.5f, 1, 2, 4, 8, 16, 0, 0, 0, 0, 0, 0 };
inline constexpr float kAdvSecs[] = { 0, 0, 0, 0, 0, 0, 1, 2, 4, 8, 15, 30 };
constexpr int kNumAdvEvery = 12;
inline constexpr const char* kAdvOrderNames[] = { "FWD", "PING", "RAND", "LIST" };
constexpr int kNumAdvOrders = 4;
constexpr int kAdvOrderList = 3;
constexpr int kAdvListMax = 16;

class AutoAdvance {
public:
    int section = 0;        // FWD/PING/RAND: offset from the start section; LIST: absolute section (0-based)
    int listPos = 0;        // LIST: which entry plays now
    bool listMode = false;
    float progress = 0.0f;  // 0..1 until the next jump

    void restart() { lastIdx = INT64_MIN; secs = 0; beat = 0; section = 0; listPos = 0; progress = 0; }

    // Call every few samples (both plugins tick every 32). Returns true when the section changed.
    //  hostPlaying/hostBeat: transport; sounding: notes or Play are active; dt: block length in seconds
    //  list/listLen: LIST order entries (1-based section numbers); holds: how many EVERY-units each entry lasts (1 = same length)
    bool tick(bool on, int every, int order, int count, bool hostPlaying, double hostBeat, double bpm,
              double beatsPerBar, bool sounding, double dt, const float* list = nullptr, int listLen = 0, const float* holds = nullptr)
    {
        listMode = order == kAdvOrderList && list && listLen > 0;
        if (!on || count <= 1) { progress = 0; if (!on) { section = 0; listPos = 0; lastIdx = INT64_MIN; } return false; }
        if (!sounding && !hostPlaying) { secs = 0; beat = 0; lastIdx = INT64_MIN; progress = 0; }
        every = every < 0 ? 0 : (every >= kNumAdvEvery ? kNumAdvEvery - 1 : every);
        int64_t idx = 0;
        if (sounding || hostPlaying) {
            if (kAdvBars[every] > 0) {
                const double len = kAdvBars[every] * (beatsPerBar > 0 ? beatsPerBar : 4.0);
                double b;
                if (hostPlaying) b = hostBeat;
                else { b = beat; beat += dt * bpm / 60.0; }     // time at the start of this tick
                const double pos = b / len;
                idx = (int64_t)std::floor(pos + 1e-9);
                progress = (float)(pos - std::floor(pos));
            } else {
                const double len = kAdvSecs[every];
                idx = (int64_t)std::floor(secs / len + 1e-9);
                progress = (float)(std::fmod(secs, len) / len);
                secs += dt;
            }
            lastIdx = idx;
        }
        if (idx < 0) idx = 0;
        int ns;
        if (listMode) {
            if (holds) {
                // each entry lasts hold[i] units: walk through the cycle to find the entry and the progress inside it
                int64_t total = 0;
                for (int i = 0; i < listLen; ++i) total += holdOf(holds, i);
                int64_t u = idx % total;
                const float frac = progress;
                int e = 0;
                while (e < listLen - 1 && u >= holdOf(holds, e)) { u -= holdOf(holds, e); ++e; }
                listPos = e;
                progress = (float)(((double)u + frac) / (double)holdOf(holds, e));
            } else listPos = (int)(idx % listLen);
            const int v = (int)std::lround(list[listPos]) - 1;
            ns = ((v % count) + count) % count;
        } else { listPos = 0; ns = sectionFor(idx, order, count); }
        const bool changed = ns != section;
        section = ns;
        return changed;
    }

    // how far to move from the start section 'base' (both 0-based)
    int offsetFor(int base, int count) const {
        if (count <= 1) return 0;
        if (!listMode) return section % count;
        return (((section - base) % count) + count) % count;
    }

    static int sectionFor(int64_t idx, int order, int count) {
        if (count <= 1) return 0;
        if (idx < 0) idx = 0;
        if (order == 1) { const int64_t per = 2 * (int64_t)(count - 1); const int64_t m = idx % per; return (int)(m < count ? m : per - m); }
        if (order == 2) {
            // shuffle bag: every section once per round, new order each round, never the same one twice in a row
            if (count == 2) return (int)(idx % 2);
            if (count > 64) { const int r = (int)(hash((uint32_t)idx) % (uint32_t)count), q = (int)(hash((uint32_t)(idx - 1)) % (uint32_t)count); return r == q ? (r + 1) % count : r; }
            const int64_t round = idx / count, at = idx % count;
            int perm[64]; shuffled(perm, count, round);
            if (round > 0) { int prev[64]; shuffled(prev, count, round - 1); if (perm[0] == prev[count - 1]) { const int t = perm[0]; perm[0] = perm[1]; perm[1] = t; } }
            return round == 0 && at == 0 ? 0 : perm[at];
        }
        return (int)(idx % count);
    }

    static int holdOf(const float* holds, int i) { const int h = (int)std::lround(holds[i]); return h < 1 ? 1 : (h > 64 ? 64 : h); }

private:
    int64_t lastIdx = INT64_MIN;
    double secs = 0, beat = 0;
    static void shuffled(int* p, int count, int64_t round) {
        for (int i = 0; i < count; ++i) p[i] = i;
        uint32_t h = hash((uint32_t)round * 2654435761U + 12345U);
        for (int i = count - 1; i > 0; --i) { h = hash(h + (uint32_t)i); const int j = (int)(h % (uint32_t)(i + 1)); const int t = p[i]; p[i] = p[j]; p[j] = t; }
        if (round == 0) for (int i = 1; i < count; ++i) if (p[i] == 0) { p[i] = p[0]; p[0] = 0; break; }   // start on the chosen chop
    }
    static uint32_t hash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
};

} // namespace gf
