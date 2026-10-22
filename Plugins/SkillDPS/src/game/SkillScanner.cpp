#include "SkillScanner.h"

#include <algorithm>

namespace SkillDPSGame {

namespace {

std::string ShortAsciiName(const std::wstring& w) {
    const size_t slash = w.find_last_of(L"/\\");
    const std::wstring tail = slash == std::wstring::npos ? w : w.substr(slash + 1);
    std::string s;
    s.reserve(tail.size());
    for (wchar_t c : tail)
        if (c >= 32 && c < 127) s += static_cast<char>(c);
    return s.empty() ? "Minion" : s;
}

void CollectForActor(const PluginSDK::Context* ctx, uintptr_t actorAddr, bool isPlayer,
                     const std::string& owner, bool withRaw, std::vector<SkillRow>& out) {
    if (!actorAddr) return;
    const auto skills = ctx->Components.EnumerateActiveSkills(actorAddr);
    for (const auto& sk : skills) {
        if (!sk.SkillDetailsAddr) continue;
        // Same call, same frame: the details address is still valid here.
        const auto stats = ctx->Components.EnumerateSkillStats(sk.SkillDetailsAddr);
        SkillRow row;
        row.isPlayer = isPlayer;
        row.owner = owner;
        row.name = sk.Name;
        row.detailsAddr = sk.SkillDetailsAddr;
        row.slotIndex = sk.SkillSlotIndex;
        row.onBar = sk.IsOnSkillBar;
        row.s = ParseSet0(stats, &row.setCount, withRaw ? &row.rawSet0 : nullptr);
        out.push_back(std::move(row));
    }
}

} // namespace

std::vector<SkillRow> CollectSkillRows(const PluginSDK::Context* ctx, bool includeMinions, bool withRaw) {
    std::vector<SkillRow> out;
    if (!ctx) return out;
    if (includeMinions) {
        const auto snap = ctx->Game.GetSnapshot(PluginSDK::EntityType::Monster);
        if (snap.Player.Components.HasActor())
            CollectForActor(ctx, snap.Player.Components.Actor, true, "Player", withRaw, out);
        for (const auto& e : snap.Entities) {
            if (e.EntityState != PluginSDK::EntityState::MonsterFriendly || !e.Components.HasActor()) continue;
            CollectForActor(ctx, e.Components.Actor, false, ShortAsciiName(e.Path), withRaw, out);
        }
    } else {
        const auto player = ctx->Entities.GetPlayer();
        if (player.Components.HasActor())
            CollectForActor(ctx, player.Components.Actor, true, "Player", withRaw, out);
    }
    return out;
}

std::vector<SkillRow> DedupeBest(const std::vector<SkillRow>& rows) {
    std::vector<SkillRow> out;
    out.reserve(rows.size());
    for (const auto& r : rows) {
        auto it = std::find_if(out.begin(), out.end(), [&](const SkillRow& o) {
            return o.isPlayer == r.isPlayer && o.owner == r.owner && o.name == r.name;
        });
        if (it == out.end()) out.push_back(r);
        else if (r.s.Value() > it->s.Value()) *it = r;
    }
    return out;
}

std::string DisplayName(const std::string& internalName) {
    static const std::string kSuffix = "Player";
    if (internalName.size() > kSuffix.size() &&
        internalName.compare(internalName.size() - kSuffix.size(), kSuffix.size(), kSuffix) == 0)
        return internalName.substr(0, internalName.size() - kSuffix.size());
    return internalName;
}

} // namespace SkillDPSGame
