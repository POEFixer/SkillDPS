// Finds the skill bar's icon boxes on screen and which skill each one shows.
//
// Finding the bar walks the game's UI tree (thousands of nodes), so it runs
// once and is then cached: later refreshes only re-read the bar container's
// few children. A full walk happens again only on an area change, or when the
// cached container no longer looks like a bar, with a growing pause between
// failed attempts. The walk is spread over frames (kWalkBudgetMs per frame,
// see Advance) so it never stalls one frame.
//
// Which skill an icon shows is read from the slot's own UI element (the host
// has no API for it): a pointer at a fixed offset that equals one of the
// player's skill addresses. The address is only trusted when it matches a skill
// the host reports, so a stale offset shows nothing rather than wrong numbers,
// and when no slot matches at all for a while the offset is searched again
// (the slot that agrees with the most icons wins).
#pragma once

#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "BarLogic.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SkillDPSGame {

struct BarSlot {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    uintptr_t addr = 0;
};

class SkillBarTracker {
public:
    using Clock = std::chrono::steady_clock;

    static constexpr uintptr_t kDefaultSlotOffset = 0x2F8;   // game version of 2026-07
    static constexpr size_t    kSlotScanBytes = 0x800;
    static constexpr double    kWalkBudgetMs = 1.5;           // UI-tree walk time per frame

    void Reset();   // area change / plugin enable

    // Refreshes the visible slot rectangles (cheap when the container is cached).
    // Without a container it starts the bar search and runs its first share.
    void Update(const PluginSDK::Context* ctx, Clock::time_point now);

    // True while a bar search is spread over frames: call Advance once per frame
    // until it returns to false (the slots are then ready for the next Update).
    bool Searching() const { return m_walk.active; }
    void Advance(const PluginSDK::Context* ctx, Clock::time_point now);
    const std::vector<BarSlot>& Slots() const { return m_slots; }

    // Skill address shown by each slot (0 = none / unknown), resolved against
    // the player's current skill addresses; re-discovers the offset if needed.
    std::vector<uintptr_t> ResolveSlotSkills(const PluginSDK::Context* ctx,
                                             const std::unordered_set<uint64_t>& playerSkills,
                                             Clock::time_point now);

    uintptr_t Container() const { return m_container; }
    uintptr_t SlotOffset() const { return m_offset; }
    bool      OffsetRediscovered() const { return m_offset != kDefaultSlotOffset; }
    int       FullScans() const { return m_fullScans; }
    double    LastFullScanMs() const { return m_lastScanMs; }
    bool      BarHidden() const { return m_hidden; }

    // Diagnostics: one full walk, returning the short-text label candidates.
    static std::vector<LabelRect> CollectLabelCandidates(const PluginSDK::Context* ctx);

private:
    struct Walk {
        std::vector<LabelRect> labels;
        std::unordered_map<uintptr_t, uintptr_t> parentOf;
    };
    // A depth-first walk of the UI tree that can stop and resume between frames.
    struct WalkState {
        struct Node { uintptr_t addr; int depth; };
        bool active = false;
        std::vector<Node> stack;
        Walk walk;
        int visited = 0;
        float minY = 0.f, scale = 1.f;
        double spentMs = 0.0;
    };
    static void BeginWalk(const PluginSDK::Context* ctx, WalkState& st, float minY, float scale);
    // Processes nodes until the walk ends (true) or budgetMs is used up (false;
    // budgetMs <= 0 means no limit).
    static bool StepWalk(const PluginSDK::Context* ctx, WalkState& st, double budgetMs);
    static Walk WalkUi(const PluginSDK::Context* ctx, float minY, float scale);
    static uintptr_t ContainerFromWalk(const PluginSDK::Context* ctx, const Walk& walk, float scale);
    static std::vector<BarSlot> ReadSlots(const PluginSDK::Context* ctx, uintptr_t container, float scale);
    void FinishSearch(const PluginSDK::Context* ctx, Clock::time_point now);

    WalkState m_walk;
    uintptr_t m_container = 0;
    std::vector<BarSlot> m_slots;
    bool m_hidden = false;
    Clock::time_point m_nextFullScan{};
    std::chrono::milliseconds m_backoff{0};
    int m_fullScans = 0;
    double m_lastScanMs = 0.0;

    uintptr_t m_offset = kDefaultSlotOffset;
    int m_missStreak = 0;
    Clock::time_point m_nextDiscovery{};
};

} // namespace SkillDPSGame
