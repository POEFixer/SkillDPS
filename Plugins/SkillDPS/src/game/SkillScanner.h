// Reads the game's own evaluated per-skill numbers (DPS, average hit) for the
// player and, optionally, friendly minions and totems.
#pragma once

#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SkillDPSGame {

// Runtime stat keys (Stats.dat row + 1) the host documents for EnumerateSkillStats.
inline constexpr int kStatAps      = 691;
inline constexpr int kStatDps      = 692;
inline constexpr int kStatBaseCast = 694;
inline constexpr int kStatCps      = 695;
inline constexpr int kStatAvgHit   = 1982;
inline constexpr int kStatAvgUse   = 1983;
inline constexpr int kStatShowAvg  = 2079;

struct SkillStats {
    bool  hasDps = false;
    bool  hasAvg = false;
    float dps = 0.f;
    float aps = 0.f;
    float cps = 0.f;
    float avgHit = 0.f;
    float avgUse = 0.f;
    int   baseCastMs = 0;
    bool  showAvg = false;

    // The number the game shows: DPS, or the average hit for skills the game
    // presents as "average damage" (and for skills with no DPS at all).
    bool  UsesAverage() const { return (showAvg || !hasDps) && hasAvg; }
    float Average() const { return avgHit > 0.f ? avgHit : avgUse; }
    bool  HasValue() const { return hasDps || UsesAverage(); }
    float Value() const { return UsesAverage() ? Average() : (hasDps ? dps : 0.f); }
};

struct SkillRow {
    bool        isPlayer = false;
    std::string owner;          // "Player" or the minion's metadata name
    std::string name;
    uintptr_t   detailsAddr = 0;
    int         setCount = 0;
    int         slotIndex = -1;
    bool        onBar = false;
    std::string rawSet0;        // filled only when asked (diagnostics)
    SkillStats  s;
};

template <typename StatVec>
SkillStats ParseSet0(const StatVec& stats, int* outSetCount, std::string* outRaw) {
    SkillStats r;
    int maxSet = -1;
    for (const auto& e : stats) {
        if (e.SetIndex > maxSet) maxSet = e.SetIndex;
        if (e.SetIndex != 0) continue;
        if (outRaw) {
            *outRaw += std::to_string(e.StatId);
            *outRaw += ':';
            *outRaw += std::to_string(e.Value);
            *outRaw += ' ';
        }
        switch (e.StatId) {
            case kStatDps:      r.dps = e.Value / 100.f; r.hasDps = true; break;
            case kStatAps:      r.aps = e.Value / 100.f; break;
            case kStatCps:      r.cps = e.Value / 100.f; break;
            case kStatAvgHit:   r.avgHit = e.Value / 100.f; r.hasAvg = true; break;
            case kStatAvgUse:   r.avgUse = e.Value / 100.f; r.hasAvg = true; break;
            case kStatBaseCast: r.baseCastMs = e.Value; break;
            case kStatShowAvg:  r.showAvg = e.Value != 0; break;
            default: break;
        }
    }
    if (outSetCount) *outSetCount = maxSet + 1;
    return r;
}

// Every active skill of the player (and of friendly minions when asked).
// Minions come from a monster-only snapshot, so loot and other entities are
// never materialised. withRaw fills SkillRow::rawSet0 (diagnostics only).
std::vector<SkillRow> CollectSkillRows(const PluginSDK::Context* ctx, bool includeMinions, bool withRaw);

// One row per (owner, name), keeping the highest value.
std::vector<SkillRow> DedupeBest(const std::vector<SkillRow>& rows);

// "Player" suffix of the host's internal skill names removed, for display.
std::string DisplayName(const std::string& internalName);

} // namespace SkillDPSGame
