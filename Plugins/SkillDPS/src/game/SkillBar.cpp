#include "SkillBar.h"

#include <algorithm>

namespace SkillDPSGame {

namespace {

constexpr int   kMaxNodes = 8000;
constexpr int   kMaxDepth = 40;
constexpr int   kMaxAscend = 8;
constexpr float kBottomFraction = 0.78f;   // the bar sits in the bottom 22 % of the screen
constexpr int   kMissesBeforeDiscovery = 3;

} // namespace

void SkillBarTracker::Reset() {
    m_walk = {};   // a search of the previous area's UI is dropped
    m_container = 0;
    m_slots.clear();
    m_hidden = false;
    m_nextFullScan = {};
    m_backoff = std::chrono::milliseconds(0);
    m_missStreak = 0;
    m_nextDiscovery = {};
}

void SkillBarTracker::BeginWalk(const PluginSDK::Context* ctx, WalkState& st, float minY, float scale) {
    st = {};
    st.active = true;
    st.minY = minY;
    st.scale = scale;
    st.stack.reserve(256);
    if (const uintptr_t root = ctx->Ui.GetGameUiRoot()) st.stack.push_back({root, 0});
}

bool SkillBarTracker::StepWalk(const PluginSDK::Context* ctx, WalkState& st, double budgetMs) {
    const float maxLabelW = 120.f * st.scale, maxLabelH = 60.f * st.scale;
    const auto t0 = Clock::now();
    while (!st.stack.empty() && st.visited < kMaxNodes) {
        // Every node costs several reads of the game's memory: stop when this
        // frame's share is used and go on from here next frame.
        if (budgetMs > 0.0 && std::chrono::duration<double, std::milli>(Clock::now() - t0).count() >= budgetMs)
            return false;
        const WalkState::Node n = st.stack.back();
        st.stack.pop_back();
        ++st.visited;
        if (n.depth > kMaxDepth || !n.addr || !ctx->Ui.IsVisible(n.addr)) continue;
        for (const uintptr_t c : ctx->Ui.GetChildren(n.addr)) {
            if (!c) continue;
            st.walk.parentOf[c] = n.addr;
            st.stack.push_back({c, n.depth + 1});
        }
        std::string text = ctx->Ui.GetText(n.addr);
        if (text.empty() || text.size() > 4) continue;
        LabelRect r;
        if (!ctx->Ui.ComputeScreenRect(n.addr, r.x, r.y, r.w, r.h)) continue;
        if (r.y < st.minY || r.w <= 0.f || r.h <= 0.f || r.w > maxLabelW || r.h > maxLabelH) continue;
        r.addr = n.addr;
        r.text = std::move(text);
        st.walk.labels.push_back(std::move(r));
    }
    return true;
}

SkillBarTracker::Walk SkillBarTracker::WalkUi(const PluginSDK::Context* ctx, float minY, float scale) {
    WalkState st;
    BeginWalk(ctx, st, minY, scale);
    StepWalk(ctx, st, 0.0);
    return std::move(st.walk);
}

std::vector<LabelRect> SkillBarTracker::CollectLabelCandidates(const PluginSDK::Context* ctx) {
    if (!ctx) return {};
    const auto screen = ctx->Game.GetScreenSize();
    if (screen.Height <= 0.f) return {};
    return WalkUi(ctx, screen.Height * kBottomFraction, UiScaleFor(screen.Height)).labels;
}

uintptr_t SkillBarTracker::ContainerFromWalk(const PluginSDK::Context* ctx, const Walk& walk, float scale) {
    const auto row = PickLabelRow(walk.labels, scale);
    if (row.empty()) return 0;

    // From a key label up to the icon-sized slot element; its parent holds the slots.
    uintptr_t cur = row.front().addr;
    for (int hop = 0; cur && hop < kMaxAscend; ++hop) {
        float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
        if (ctx->Ui.ComputeScreenRect(cur, x, y, w, h) && IsIconSized(w, h, scale)) break;
        const auto it = walk.parentOf.find(cur);
        cur = it == walk.parentOf.end() ? 0 : it->second;
    }
    if (!cur) return 0;
    const auto it = walk.parentOf.find(cur);
    return it == walk.parentOf.end() ? 0 : it->second;
}

std::vector<BarSlot> SkillBarTracker::ReadSlots(const PluginSDK::Context* ctx, uintptr_t container, float scale) {
    std::vector<BarSlot> out;
    std::vector<uintptr_t> children;
    for (const uintptr_t c : ctx->Ui.GetChildren(container))
        if (c && ctx->Ui.IsVisible(c)) children.push_back(c);
    if (children.size() < kMinSlots || children.size() > 64) return out;
    const auto rects = ctx->Ui.ComputeScreenRects(children);
    for (size_t i = 0; i < children.size(); ++i) {
        BarSlot s;
        s.addr = children[i];
        if (rects[i].ok) {
            s.x = rects[i].x; s.y = rects[i].y; s.w = rects[i].w; s.h = rects[i].h;
        } else if (!ctx->Ui.ComputeScreenRect(children[i], s.x, s.y, s.w, s.h)) {
            continue;
        }
        if (IsIconSized(s.w, s.h, scale)) out.push_back(s);
    }
    if (out.size() < kMinSlots) out.clear();
    return out;
}

void SkillBarTracker::Update(const PluginSDK::Context* ctx, Clock::time_point now) {
    if (!ctx) return;
    const auto screen = ctx->Game.GetScreenSize();
    const float scale = UiScaleFor(screen.Height);

    if (m_container) {
        if (!ctx->Ui.IsVisible(m_container)) {
            // Bar hidden (full-screen panel, cut-scene): keep the container, draw nothing.
            m_slots.clear();
            m_hidden = true;
            return;
        }
        m_hidden = false;
        m_slots = ReadSlots(ctx, m_container, scale);
        if (!m_slots.empty()) return;
        m_container = 0;   // no longer a bar: find it again
    }

    m_slots.clear();
    // A search already under way is advanced once per frame by the caller.
    if (m_walk.active || now < m_nextFullScan || screen.Height <= 0.f) return;
    BeginWalk(ctx, m_walk, screen.Height * kBottomFraction, scale);
    Advance(ctx, now);   // this frame's share
}

void SkillBarTracker::Advance(const PluginSDK::Context* ctx, Clock::time_point now) {
    if (!ctx || !m_walk.active) return;
    const auto t0 = Clock::now();
    const bool done = StepWalk(ctx, m_walk, kWalkBudgetMs);
    m_walk.spentMs += std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    if (done) FinishSearch(ctx, now);
}

void SkillBarTracker::FinishSearch(const PluginSDK::Context* ctx, Clock::time_point now) {
    const float scale = m_walk.scale;
    m_container = ContainerFromWalk(ctx, m_walk.walk, scale);
    m_lastScanMs = m_walk.spentMs;   // walk time summed over its frames
    ++m_fullScans;
    m_walk = {};
    if (m_container) {
        m_slots = ReadSlots(ctx, m_container, scale);
        if (m_slots.empty()) m_container = 0;
    }
    if (m_container) {
        m_backoff = std::chrono::milliseconds(0);
    } else {
        m_backoff = std::clamp(m_backoff * 2, std::chrono::milliseconds(1000), std::chrono::milliseconds(8000));
        m_nextFullScan = now + m_backoff;
    }
}

std::vector<uintptr_t> SkillBarTracker::ResolveSlotSkills(const PluginSDK::Context* ctx,
                                                          const std::unordered_set<uint64_t>& playerSkills,
                                                          Clock::time_point now) {
    std::vector<uintptr_t> out(m_slots.size(), 0);
    if (!ctx || m_slots.empty() || playerSkills.empty()) return out;

    int matches = 0;
    for (size_t i = 0; i < m_slots.size(); ++i) {
        uint64_t v = 0;
        if (!ctx->Memory.Read(m_slots[i].addr + m_offset, &v, sizeof(v))) continue;
        if (v && playerSkills.count(v)) {
            out[i] = static_cast<uintptr_t>(v);
            ++matches;
        }
    }
    m_missStreak = matches == 0 ? m_missStreak + 1 : 0;
    if (m_missStreak < kMissesBeforeDiscovery || now < m_nextDiscovery) return out;

    // No slot matched for a while although the player has skills: the slot
    // layout moved. Vote over the first bytes of every slot element.
    m_nextDiscovery = now + std::chrono::seconds(10);
    std::vector<std::vector<uint8_t>> bytes;
    bytes.reserve(m_slots.size());
    for (const auto& s : m_slots) {
        std::vector<uint8_t> b(kSlotScanBytes);
        if (ctx->Memory.Read(s.addr, b.data(), b.size())) bytes.push_back(std::move(b));
    }
    const int minVotes = std::min<int>(2, static_cast<int>(playerSkills.size()));
    const int found = VoteSlotOffset(bytes, playerSkills, kDefaultSlotOffset, minVotes);
    if (found >= 0 && static_cast<uintptr_t>(found) != m_offset) {
        m_offset = static_cast<uintptr_t>(found);
        m_missStreak = 0;
    }
    return out;
}

} // namespace SkillDPSGame
