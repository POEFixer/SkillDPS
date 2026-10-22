// Skill DPS: draws the game's own per-skill DPS numbers on the skill-bar icons,
// plus an optional movable list of every evaluated skill (minions included).
//
// Originally created by Omer Faruk ARPA; maintained by POEFixer since
// October 2026. Licensed under the GNU General Public License v3.0.
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../../../POEFixer/imgui/IconsFontAwesome6.h"
#include "../../../POEFixer/plugin_sdk/PluginUi.h"

#include "Settings.h"
#include "game/BarLogic.h"
#include "game/SkillBar.h"
#include "game/SkillScanner.h"
#include "ui/Diagnostics.h"
#include "ui/Overlays.h"

#include <imgui.h>
#include <imgui_internal.h>   // ImTextInitClassifiers (see BindImGui)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

// Plugin info: the host checks it before CreatePlugin; the publish tool reads it.
POEFIXER_PLUGIN_INFO(
    "SkillDPS",
    "Skill DPS",
    "1.0.1",
    "Omer Faruk ARPA, POEFixer",
    "Shows the game's own DPS for each skill on your skill-bar icons, plus an optional movable list of every evaluated skill including minions.")

namespace {

using Clock = std::chrono::steady_clock;
using SkillDPSConfig::LabelPlace;

} // namespace

class SkillDPSPlugin : public PluginSDK::Plugin {
public:
    const char* GetName() const override { return "Skill DPS"; }

    bool WantsOverlay() const override {
        return m_loaded && (m_settings.showOnIcons || m_settings.showList);
    }

    void OnEnable(bool /*isGameAttached*/) override {
        if (!HostCompatible()) {
            ctx()->Log.Error("[SkillDPS] this POEFixer version is too old for Skill DPS");
            return;
        }
        BindImGui();
        m_settings = {};
        m_settings.Load(DirectoryPath());
        m_loaded = true;
        m_dirty = false;
        ClearModel();
        m_tracker.Reset();
        m_areaCounter = ~uint64_t{0};
        ctx()->Log.Info("[SkillDPS] enabled");
    }

    void OnDisable() override {
        SaveSettings();
        ClearModel();
        m_tracker.Reset();
        m_diag = {};
    }

    void SaveSettings() override {
        if (m_loaded && m_settings.Save(DirectoryPath())) m_dirty = false;
    }

    void DrawUI() override {
        if (!m_loaded) return;
        BindImGui();
        const auto now = Clock::now();
        if (m_dirty && now - m_dirtySince > std::chrono::seconds(1)) SaveSettings();

        const bool settingsOpen = ImGui::GetFrameCount() - m_settingsFrame <= 1;
        if (!settingsOpen) m_diagOpen = false;

        // Nothing while loading or out of game; nothing while the game is in
        // the background unless the POEFixer menu is up (to preview changes).
        if (!ctx()->Game.IsInGame()) {
            if (m_hasModel) ClearModel();
            return;
        }
        if (!ctx()->Game.IsForeground() && !ctx()->Game.IsMenuVisible()) return;

        // A skill-bar search runs a little every frame instead of all at once;
        // when it ends, refresh at once so the labels appear without waiting.
        // (Not across an area change: Refresh resets the search first.)
        if (m_tracker.Searching() && ctx()->Game.GetAreaChangeCounter() == m_areaCounter) {
            m_tracker.Advance(ctx(), now);
            if (!m_tracker.Searching()) m_nextRefresh = {};
        }
        if (now >= m_nextRefresh) {
            m_nextRefresh = now + std::chrono::milliseconds(m_settings.scanIntervalMs);
            Refresh(now);
        }
        if (m_settings.showOnIcons) SkillDPSUi::DrawIconLabels(m_icons, m_settings, m_uiScale);
        if (m_settings.showList) {
            if (SkillDPSUi::DrawListPanel(m_list, m_settings, settingsOpen, m_forceListPos)) MarkDirty();
            m_forceListPos = false;
        }
        if (settingsOpen && m_diagOpen && m_diag.highlightSlots) SkillDPSUi::DrawSlotOutlines(m_tracker.Slots());
    }

    void DrawSettings() override {
        BindImGui();
        if (!HostCompatible()) {
            PluginUi::Hint("This POEFixer version is too old for Skill DPS. Update POEFixer.");
            return;
        }
        m_settingsFrame = ImGui::GetFrameCount();
        ImGui::PushID("SkillDPS");
        ImGui::Spacing();
        DrawSectionShow();
        ImGui::Spacing();
        DrawSectionIcons();
        ImGui::Spacing();
        DrawSectionList();
        ImGui::Spacing();
        DrawSectionDiagnostics();
        ImGui::PopID();
    }

private:
    SkillDPSConfig::Settings m_settings;
    bool m_loaded = false;
    bool m_dirty = false;
    Clock::time_point m_dirtySince{};

    // Refresh results (render thread only).
    SkillDPSGame::SkillBarTracker m_tracker;
    std::vector<SkillDPSGame::SkillRow> m_rowsAll;
    std::vector<uintptr_t> m_slotSkills;
    std::vector<SkillDPSUi::IconLabel> m_icons;
    std::vector<SkillDPSUi::ListRow> m_list;
    bool m_hasModel = false;
    bool m_rowsHaveRaw = false;
    float m_uiScale = 1.f;
    uint64_t m_areaCounter = ~uint64_t{0};
    Clock::time_point m_nextRefresh{};

    int  m_settingsFrame = -10;
    bool m_diagOpen = false;
    bool m_forceListPos = false;   // "Reset position" pending for the list panel
    SkillDPSUi::DiagnosticsState m_diag;

    void BindImGui() const {
        if (ctx()->ImGuiContext) ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        // This DLL has its own copy of ImGui's word-wrap character table, which only
        // a font atlas build in this module would fill: unfilled, every wrapped text
        // (hints, tooltips) breaks inside words. Cheap after the first call.
        ImTextInitClassifiers();
    }

    void MarkDirty() {
        if (!m_dirty) m_dirtySince = Clock::now();
        m_dirty = true;
    }

    void ClearModel() {
        m_rowsAll.clear();
        m_slotSkills.clear();
        m_icons.clear();
        m_list.clear();
        m_hasModel = false;
        m_nextRefresh = {};
    }

    // --- Refresh (every scan interval) -----------------------------------------
    void Refresh(Clock::time_point now) {
        const uint64_t area = ctx()->Game.GetAreaChangeCounter();
        if (area != m_areaCounter) {
            m_areaCounter = area;
            m_tracker.Reset();   // the bar is found again in the new area
        }
        m_uiScale = SkillDPSGame::UiScaleFor(ctx()->Game.GetScreenSize().Height);
        m_rowsAll = SkillDPSGame::CollectSkillRows(ctx(), m_settings.showMinions, m_diagOpen);
        m_rowsHaveRaw = m_diagOpen;
        m_hasModel = true;

        m_icons.clear();
        m_slotSkills.clear();
        const bool needBar = m_settings.showOnIcons || m_diagOpen;
        if (needBar) {
            m_tracker.Update(ctx(), now);
            std::unordered_set<uint64_t> player;
            for (const auto& r : m_rowsAll)
                if (r.isPlayer && r.detailsAddr) player.insert(r.detailsAddr);
            m_slotSkills = m_tracker.ResolveSlotSkills(ctx(), player, now);
            if (m_settings.showOnIcons) BuildIconLabels();
        }
        if (m_settings.showList) BuildList();
    }

    void BuildIconLabels() {
        const auto& slots = m_tracker.Slots();
        for (size_t i = 0; i < slots.size() && i < m_slotSkills.size(); ++i) {
            const uintptr_t details = m_slotSkills[i];
            if (!details) continue;
            const SkillDPSGame::SkillRow* best = nullptr;
            for (const auto& r : m_rowsAll) {
                if (!r.isPlayer || r.detailsAddr != details) continue;
                if (!best || r.s.Value() > best->s.Value()) best = &r;
            }
            if (!best) continue;
            float value = 0.f;
            bool average = false;
            if (best->s.HasValue()) {
                value = best->s.Value();
                average = best->s.UsesAverage();
            } else if (best->name.find("Totem") != std::string::npos) {
                // A totem skill has no number of its own: show what its totems deal.
                for (const auto& r : m_rowsAll)
                    if (!r.isPlayer && r.owner.find("Totem") != std::string::npos)
                        value = std::max(value, r.s.Value());
            }
            if (value <= 0.f) continue;
            SkillDPSUi::IconLabel label;
            label.slot = slots[i];
            label.average = average;
            char num[20];
            SkillDPSGame::FormatCompact(value, num, sizeof(num));
            std::snprintf(label.text, sizeof(label.text), average ? "~%s" : "%s", num);
            m_icons.push_back(label);
        }
    }

    void BuildList() {
        m_list.clear();
        for (const auto& r : SkillDPSGame::DedupeBest(m_rowsAll)) {
            if (!r.s.HasValue()) continue;
            SkillDPSUi::ListRow row;
            row.name = SkillDPSGame::DisplayName(r.name);
            if (!r.isPlayer) row.owner = r.owner;
            row.average = r.s.UsesAverage();
            char num[20];
            SkillDPSGame::FormatCompact(r.s.Value(), num, sizeof(num));
            std::snprintf(row.value, sizeof(row.value), "%s %s", num, row.average ? "avg" : "DPS");
            m_list.push_back(std::move(row));
        }
        // The player's own skills first, minions after.
        std::stable_sort(m_list.begin(), m_list.end(), [](const SkillDPSUi::ListRow& a, const SkillDPSUi::ListRow& b) {
            return a.owner.empty() && !b.owner.empty();
        });
    }

    // --- Settings tab -------------------------------------------------------------
    // A setting that changes what is read takes effect at once, not at the next tick.
    void MarkDirtyAndRefresh() {
        MarkDirty();
        m_nextRefresh = {};
    }

    void DrawSectionShow() {
        PluginUi::BeginCard("###show");
        PluginUi::Heading(ICON_FA_GAUGE_HIGH, "What to show");
        PluginUi::HelpMarker("The numbers are the game's own skill evaluation, the same ones the\n"
                             "skills panel shows. The game only evaluates the skill selected in\n"
                             "the character panel and skills in active use (totems, minions), so\n"
                             "a skill without a number has not been evaluated yet: select it in\n"
                             "the character panel or use it once.");
        if (PluginUi::Check("Numbers on the skill-bar icons", "icons", &m_settings.showOnIcons,
                            "Shows each skill's DPS on its icon. Skills the game shows as\n"
                            "average damage per hit get a '~' in front."))
            MarkDirtyAndRefresh();
        if (PluginUi::Check("DPS list panel", "list", &m_settings.showList,
                            "A small panel listing every evaluated skill with its number,\n"
                            "including minions' skills. Drag it while this tab is open."))
            MarkDirtyAndRefresh();
        if (PluginUi::Check("Include minions and totems", "minions", &m_settings.showMinions,
                            "Adds friendly minions' skills to the list and lets a totem skill\n"
                            "show the damage of the totems it placed."))
            MarkDirtyAndRefresh();

        ImGui::Spacing();
        const PluginUi::LabelColumn col({"Update every"});
        if (PluginUi::SliderIntRow(col, "Update every", "###interval", &m_settings.scanIntervalMs,
                                   SkillDPSConfig::kScanIntervalMinMs, SkillDPSConfig::kScanIntervalMaxMs, "%d ms",
                                   "How often the numbers are read from the game. Lower is more\n"
                                   "current, higher costs less time. 250 ms suits most players."))
            MarkDirty();
        PluginUi::EndCard();
    }

    // A percentage slider row over a factor stored as a float (1.0 = 100 %).
    static bool PercentRow(const PluginUi::LabelColumn& col, const char* label, const char* id, float* factor,
                           int minPct, int maxPct, const char* tip) {
        int pct = static_cast<int>(std::lround(*factor * 100.f));
        if (!PluginUi::SliderIntRow(col, label, id, &pct, minPct, maxPct, "%d%%", tip)) return false;
        *factor = static_cast<float>(pct) / 100.f;
        return true;
    }

    // ImGui draws colour swatches undimmed inside BeginDisabled (on purpose, and
    // PluginUi::Swatch does too): a disabled row shows a faded, read-only copy so
    // the whole block reads as off.
    static bool FadingSwatch(const char* id, ImVec4& color, bool enabled) {
        if (enabled) return PluginUi::Swatch(id, color);
        const ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
        constexpr float kFade = 0.6f;
        ImVec4 faded(color.x + (bg.x - color.x) * kFade, color.y + (bg.y - color.y) * kFade,
                     color.z + (bg.z - color.z) * kFade, color.w);
        PluginUi::Swatch(id, faded);
        return false;
    }

    void DrawSectionIcons() {
        PluginUi::BeginCard("###iconsCard");
        PluginUi::Heading(ICON_FA_FIRE, "Icon numbers");
        PluginUi::HelpMarker("How the numbers on the skill-bar icons look.\n"
                             "Dimmed while \"Numbers on the skill-bar icons\" is off.");
        ImGui::BeginDisabled(!m_settings.showOnIcons);
        const PluginUi::LabelColumn col({"Size", "Background", "Position", "DPS colour", "Average colour"});
        if (PercentRow(col, "Size", "###labelSize", &m_settings.labelScale, 70, 180,
                       "Text size of the numbers, relative to POEFixer's font size."))
            MarkDirty();
        if (PercentRow(col, "Background", "###labelBg", &m_settings.labelOpacity, 0, 100,
                       "Opacity of the dark plate behind each number.\n0% draws the number with its dark outline only."))
            MarkDirty();
        col.Row("Position");
        int place = static_cast<int>(m_settings.labelPlace);
        if (PluginUi::Combo("###labelPlace", place, {"Top of the icon", "Bottom of the icon"})) {
            m_settings.labelPlace = place == 1 ? LabelPlace::Bottom : LabelPlace::Top;
            MarkDirty();
        }
        PluginUi::Tip("Where on the icon the number sits. Bottom keeps the key label\nvisible on most layouts.");
        col.RowNoWidth("DPS colour");
        if (FadingSwatch("###dpsColor", m_settings.dpsColor, m_settings.showOnIcons)) MarkDirty();
        PluginUi::Tip("Colour of DPS numbers.");
        col.RowNoWidth("Average colour");
        if (FadingSwatch("###avgColor", m_settings.avgColor, m_settings.showOnIcons)) MarkDirty();
        PluginUi::Tip("Colour of average-hit numbers (shown with '~').");
        ImGui::EndDisabled();
        PluginUi::EndCard();
    }

    void DrawSectionList() {
        PluginUi::BeginCard("###listCard");
        PluginUi::Heading(ICON_FA_LIST, "DPS list panel");
        PluginUi::HelpMarker("How the movable DPS list looks. While this tab is open the panel\n"
                             "can be dragged in game; its place is saved.\n"
                             "Dimmed while \"DPS list panel\" is off.");
        ImGui::BeginDisabled(!m_settings.showList);
        const PluginUi::LabelColumn col({"Size", "Background", "Position"});
        if (PercentRow(col, "Size", "###listSize", &m_settings.listScale, 70, 180,
                       "Text size of the panel, relative to POEFixer's font size."))
            MarkDirty();
        if (PercentRow(col, "Background", "###listBg", &m_settings.listOpacity, 0, 100,
                       "Opacity of the panel background."))
            MarkDirty();
        col.RowNoWidth("Position");
        if (PluginUi::SubtleButton(ICON_FA_ROTATE_LEFT "  Reset position###resetPos")) {
            m_settings.overlayX = 20.f;
            m_settings.overlayY = 300.f;
            m_forceListPos = true;   // the panel is placed there the next time it is drawn
            MarkDirty();
        }
        PluginUi::Tip("Moves the panel back to the left side of the screen.\n"
                      "To place it, drag the panel in game while this tab is open.");
        ImGui::EndDisabled();
        PluginUi::EndCard();
    }

    void DrawSectionDiagnostics() {
        PluginUi::BeginCard("###diagCard");
        m_diagOpen = ImGui::CollapsingHeader(ICON_FA_BUG "  Diagnostics###diag");
        if (m_diagOpen) {
            if (!m_rowsHaveRaw) m_nextRefresh = {};   // fetch the raw stats now, not at the next tick
            SkillDPSUi::DiagnosticsInput in;
            in.rows = &m_rowsAll;
            in.slots = &m_tracker.Slots();
            in.slotSkills = &m_slotSkills;
            in.tracker = &m_tracker;
            in.inGame = ctx()->Game.IsInGame();
            SkillDPSUi::DrawDiagnosticsBody(ctx(), in, m_diag, DirectoryPath());
        }
        PluginUi::EndCard();
    }
};

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new SkillDPSPlugin(); }

extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* p) { delete p; }
