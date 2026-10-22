#include "BarLogic.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace SkillDPSGame {

std::vector<LabelRect> PickLabelRow(std::vector<LabelRect> cands, float scale) {
    std::vector<LabelRect> out;
    if (cands.size() < kMinSlots) return out;
    const float rowGap = 24.f * scale;
    const float clusterGap = 200.f * scale;

    std::sort(cands.begin(), cands.end(), [](const LabelRect& a, const LabelRect& b) { return a.y < b.y; });
    size_t bestBegin = 0, bestCount = 0;
    for (size_t i = 0; i < cands.size(); ++i) {
        size_t j = i;
        while (j < cands.size() && cands[j].y - cands[i].y <= rowGap) ++j;
        if (j - i > bestCount) {
            bestCount = j - i;
            bestBegin = i;
        }
    }
    if (bestCount < kMinSlots) return out;

    std::vector<LabelRect> row(cands.begin() + static_cast<ptrdiff_t>(bestBegin),
                               cands.begin() + static_cast<ptrdiff_t>(bestBegin + bestCount));
    std::sort(row.begin(), row.end(), [](const LabelRect& a, const LabelRect& b) { return a.x < b.x; });

    size_t clBegin = 0, clBestBegin = 0, clBestCount = 0;
    for (size_t i = 1; i <= row.size(); ++i) {
        const bool split = i == row.size() ||
            (row[i].x + row[i].w * 0.5f) - (row[i - 1].x + row[i - 1].w * 0.5f) > clusterGap;
        if (!split) continue;
        if (i - clBegin > clBestCount) {
            clBestCount = i - clBegin;
            clBestBegin = clBegin;
        }
        clBegin = i;
    }
    if (clBestCount < kMinSlots) return out;
    out.assign(row.begin() + static_cast<ptrdiff_t>(clBestBegin),
               row.begin() + static_cast<ptrdiff_t>(clBestBegin + clBestCount));
    return out;
}

bool IsIconSized(float w, float h, float scale) {
    const float lo = 30.f * scale, hi = 120.f * scale;
    return w >= lo && w <= hi && h >= lo && h <= hi;
}

void FormatCompact(float v, char* buf, size_t n) {
    if (!buf || n == 0) return;
    if (!std::isfinite(v) || v < 0.f) v = 0.f;
    // Thresholds sit where the next unit's rounding would print "1000".
    if (v >= 999.5e6f)      std::snprintf(buf, n, "%.2fB", v / 1e9f);
    else if (v >= 999.5e3f)  std::snprintf(buf, n, v >= 99.95e6f ? "%.0fM" : (v >= 9.995e6f ? "%.1fM" : "%.2fM"), v / 1e6f);
    else if (v >= 999.5f)    std::snprintf(buf, n, v >= 99.95e3f ? "%.0fk" : "%.1fk", v / 1e3f);
    else                     std::snprintf(buf, n, "%.0f", v);
}

int VoteSlotOffset(const std::vector<std::vector<uint8_t>>& slotBytes,
                   const std::unordered_set<uint64_t>& known, size_t hint, int minVotes) {
    if (known.empty() || slotBytes.empty()) return -1;
    size_t longest = 0;
    for (const auto& b : slotBytes) longest = std::max(longest, b.size());
    int bestVotes = 0;
    long long best = -1;
    for (size_t off = 0; off + 8 <= longest; off += 8) {
        int votes = 0;
        for (const auto& b : slotBytes) {
            if (off + 8 > b.size()) continue;
            uint64_t v = 0;
            std::memcpy(&v, b.data() + off, 8);
            if (v && known.count(v)) ++votes;
        }
        if (votes == 0) continue;
        const auto dist = [&](long long o) { return o < static_cast<long long>(hint) ? static_cast<long long>(hint) - o : o - static_cast<long long>(hint); };
        if (votes > bestVotes || (votes == bestVotes && dist(static_cast<long long>(off)) < dist(best))) {
            bestVotes = votes;
            best = static_cast<long long>(off);
        }
    }
    return bestVotes >= std::max(1, minVotes) ? static_cast<int>(best) : -1;
}

} // namespace SkillDPSGame
