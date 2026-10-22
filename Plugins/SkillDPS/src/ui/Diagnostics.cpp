#include "Diagnostics.h"

#include "../../../../POEFixer/core/FileIO.h"
#include "../../../../POEFixer/plugin_sdk/PluginUi.h"

#include <imgui.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace SkillDPSUi {

namespace {

const char* NameOf(const DiagnosticsInput& in, uintptr_t details) {
    if (!details || !in.rows) return nullptr;
    for (const auto& r : *in.rows)
        if (r.isPlayer && r.detailsAddr == details) return r.name.c_str();
    return nullptr;
}

void AppendLine(std::string& t, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    t += buf;
    t += '\n';
}

float TableHeight(size_t rows) {
    const float line = ImGui::GetTextLineHeightWithSpacing();
    const float h = line * (static_cast<float>(rows) + 1.5f);
    return std::min(h, line * 14.f);
}

void DrawSkillsTable(const DiagnosticsInput& in) {
    const auto& rows = *in.rows;
    ImGui::Text("Skills found: %d", static_cast<int>(rows.size()));
    PluginUi::HelpMarker("Every active skill of the player and of friendly minions with the\n"
                         "game's own numbers. DPS exists only where the game evaluated it\n"
                         "(the skill selected in the character panel, minions in combat).\n"
                         "'Raw' lists every stat id:value of the first stat set.");
    if (rows.empty()) return;
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
    if (!ImGui::BeginTable("###skills", 6, flags, ImVec2(0.f, TableHeight(rows.size())))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Owner", ImGuiTableColumnFlags_WidthStretch, 0.9f);
    ImGui::TableSetupColumn("Skill", ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableSetupColumn("Bar", ImGuiTableColumnFlags_WidthStretch, 0.4f);
    ImGui::TableSetupColumn("DPS", ImGuiTableColumnFlags_WidthStretch, 0.7f);
    ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_WidthStretch, 0.7f);
    ImGui::TableSetupColumn("Raw", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableHeadersRow();
    for (const auto& r : rows) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextUnformatted(r.owner.c_str());
        ImGui::TableNextColumn(); ImGui::TextUnformatted(r.name.c_str());
        ImGui::TableNextColumn();
        if (r.onBar) ImGui::Text("%d", r.slotIndex); else ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        if (r.s.hasDps) ImGui::Text("%.0f", r.s.dps); else ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        if (r.s.hasAvg) ImGui::Text("%.0f", r.s.Average()); else ImGui::TextDisabled("-");
        ImGui::TableNextColumn();
        ImGui::PushTextWrapPos(0.f);
        if (r.rawSet0.empty()) ImGui::TextDisabled("(none)"); else ImGui::TextUnformatted(r.rawSet0.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::EndTable();
}

void DrawSlotsTable(const DiagnosticsInput& in, DiagnosticsState& st) {
    const auto& slots = *in.slots;
    const auto* tr = in.tracker;
    ImGui::Text("Bar slots: %d", static_cast<int>(slots.size()));
    if (tr) {
        // Its own wrapped line: the numbers are long and the tab may be narrow.
        ImGui::PushTextWrapPos(0.f);
        ImGui::TextDisabled("Container 0x%llX, slot offset 0x%llX%s, full scans %d (last %.1f ms)%s",
                            static_cast<unsigned long long>(tr->Container()),
                            static_cast<unsigned long long>(tr->SlotOffset()),
                            tr->OffsetRediscovered() ? " (re-discovered)" : "",
                            tr->FullScans(), tr->LastFullScanMs(), tr->BarHidden() ? ", bar hidden" : "");
        ImGui::PopTextWrapPos();
    }
    PluginUi::Check("Outline the detected slots on screen", "outline", &st.highlightSlots,
                    "Draws a green box with its index around every slot the plugin\n"
                    "found, while this tab is open.");
    if (slots.empty()) return;
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
    if (!ImGui::BeginTable("###slots", 3, flags)) return;
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthStretch, 0.3f);
    ImGui::TableSetupColumn("Rectangle", ImGuiTableColumnFlags_WidthStretch, 1.2f);
    ImGui::TableSetupColumn("Skill", ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableHeadersRow();
    for (size_t i = 0; i < slots.size(); ++i) {
        const auto& s = slots[i];
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("%d", static_cast<int>(i));
        ImGui::TableNextColumn(); ImGui::Text("%.0f,%.0f  %.0fx%.0f", s.x, s.y, s.w, s.h);
        ImGui::TableNextColumn();
        const uintptr_t d = in.slotSkills && i < in.slotSkills->size() ? (*in.slotSkills)[i] : 0;
        const char* name = NameOf(in, d);
        if (name) ImGui::TextUnformatted(name); else ImGui::TextDisabled("(empty or unknown)");
    }
    ImGui::EndTable();
}

void DrawUiTree(const PluginSDK::Context* ctx, DiagnosticsState& st, bool inGame) {
    ImGui::BeginDisabled(!inGame);
    if (PluginUi::SubtleButton("Capture UI tree###capture")) {
        st.uiTree = CaptureUiTree(ctx, st.uiVisited, 600);
        st.uiCaptured = true;
    }
    ImGui::EndDisabled();
    PluginUi::Tip("Lists every visible UI element that has text, with its screen\n"
                  "rectangle. Useful when the bar is not found after a game update.\n"
                  "Walks the whole UI once, so it may take a moment.");
    if (!st.uiCaptured) return;
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%d nodes visited, %d with text", st.uiVisited, static_cast<int>(st.uiTree.size()));
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
    if (!ImGui::BeginTable("###uitree", 4, flags, ImVec2(0.f, TableHeight(st.uiTree.size())))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Depth", ImGuiTableColumnFlags_WidthStretch, 0.3f);
    ImGui::TableSetupColumn("String id", ImGuiTableColumnFlags_WidthStretch, 1.4f);
    ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Rectangle", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(st.uiTree.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const auto& f = st.uiTree[static_cast<size_t>(i)];
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%d", f.depth);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(f.stringId.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(f.text.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%.0f,%.0f %.0fx%.0f", f.x, f.y, f.w, f.h);
        }
    }
    ImGui::EndTable();
}

} // namespace

std::vector<UiNodeRow> CaptureUiTree(const PluginSDK::Context* ctx, int& visited, size_t maxRows) {
    std::vector<UiNodeRow> found;
    visited = 0;
    if (!ctx) return found;
    const uintptr_t root = ctx->Ui.GetGameUiRoot();
    if (!root) return found;
    struct Node { uintptr_t addr; int depth; };
    std::vector<Node> stack{{root, 0}};
    while (!stack.empty() && visited < 8000 && found.size() < maxRows) {
        const Node n = stack.back();
        stack.pop_back();
        ++visited;
        if (n.depth > 40 || !n.addr || !ctx->Ui.IsVisible(n.addr)) continue;
        std::string sid = ctx->Ui.GetStringId(n.addr);
        std::string text = ctx->Ui.GetText(n.addr);
        if (!sid.empty() || (!text.empty() && text.size() < 64)) {
            UiNodeRow f;
            f.depth = n.depth;
            f.stringId = std::move(sid);
            f.text = std::move(text);
            ctx->Ui.ComputeScreenRect(n.addr, f.x, f.y, f.w, f.h);
            found.push_back(std::move(f));
        }
        for (const uintptr_t c : ctx->Ui.GetChildren(n.addr))
            if (c) stack.push_back({c, n.depth + 1});
    }
    return found;
}

void DrawDiagnosticsBody(const PluginSDK::Context* ctx, const DiagnosticsInput& in, DiagnosticsState& st,
                         const std::filesystem::path& pluginDir) {
    if (!in.inGame) PluginUi::Hint("Enter the game to see live data.");

    if (ImGui::TreeNodeEx("Skills###treeSkills", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (in.rows) DrawSkillsTable(in);
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Skill bar###treeBar", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (in.slots) DrawSlotsTable(in, st);
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("UI tree###treeUi")) {
        DrawUiTree(ctx, st, in.inGame);
        ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!in.inGame);
    if (PluginUi::SubtleButton("Write debug_dump.txt###dump"))
        st.dumpResult = WriteDebugDump(ctx, in, pluginDir, st.dumpPath) ? 1 : -1;
    ImGui::EndDisabled();
    PluginUi::Tip("Writes a text snapshot of everything above (plus a memory search\n"
                  "of the slot elements) next to the plugin, for a bug report.");
    if (st.dumpResult != 0) {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        if (st.dumpResult > 0) ImGui::TextDisabled("Written to %s", st.dumpPath.c_str());
        else ImGui::TextColored(PluginUi::Colors().Error, "Could not write the file.");
    }
}

bool WriteDebugDump(const PluginSDK::Context* ctx, const DiagnosticsInput& in,
                    const std::filesystem::path& pluginDir, std::string& outPath) {
    try {
        if (!ctx || pluginDir.empty()) return false;
        std::string t;
        const auto screen = ctx->Game.GetScreenSize();
        AppendLine(t, "=== Skill DPS debug dump ===");
        AppendLine(t, "screen: %.0fx%.0f", screen.Width, screen.Height);

        const auto rows = SkillDPSGame::CollectSkillRows(ctx, true, true);
        AppendLine(t, "");
        AppendLine(t, "=== skills (owner | name | slot | bar | dps | aps | avg | sets) ===");
        AppendLine(t, "count: %d", static_cast<int>(rows.size()));
        for (const auto& r : rows) {
            AppendLine(t, "%s | %s | slot=%d | bar=%d | dps=%s%.0f | aps=%.2f | avg=%.0f | sets=%d | details=0x%llX",
                       r.owner.c_str(), r.name.c_str(), r.slotIndex, r.onBar ? 1 : 0, r.s.hasDps ? "" : "-",
                       r.s.dps, r.s.aps, r.s.Average(), r.setCount, static_cast<unsigned long long>(r.detailsAddr));
            if (!r.rawSet0.empty()) AppendLine(t, "    raw: %s", r.rawSet0.c_str());
        }

        AppendLine(t, "");
        AppendLine(t, "=== bar label candidates ===");
        for (const auto& c : SkillDPSGame::SkillBarTracker::CollectLabelCandidates(ctx))
            AppendLine(t, "'%s'  %.0f,%.0f %.0fx%.0f", c.text.c_str(), c.x, c.y, c.w, c.h);

        static const std::vector<SkillDPSGame::BarSlot> kNoSlots;
        const auto& slots = in.slots ? *in.slots : kNoSlots;
        const auto* tr = in.tracker;
        AppendLine(t, "");
        AppendLine(t, "=== bar container 0x%llX, slot offset 0x%llX ===",
                   static_cast<unsigned long long>(tr ? tr->Container() : 0),
                   static_cast<unsigned long long>(tr ? tr->SlotOffset() : 0));
        for (size_t i = 0; i < slots.size(); ++i) {
            uint64_t v = 0;
            if (tr) ctx->Memory.Read(slots[i].addr + tr->SlotOffset(), &v, sizeof(v));
            const char* name = nullptr;
            for (const auto& r : rows)
                if (r.isPlayer && r.detailsAddr == v) { name = r.name.c_str(); break; }
            AppendLine(t, "[%d] addr=0x%llX %.0f,%.0f %.0fx%.0f -> %s", static_cast<int>(i),
                       static_cast<unsigned long long>(slots[i].addr), slots[i].x, slots[i].y, slots[i].w, slots[i].h,
                       name ? name : "(none)");
        }

        AppendLine(t, "");
        AppendLine(t, "=== slot element memory search (offsets holding a player skill address) ===");
        std::unordered_map<uint64_t, std::string> known;
        for (const auto& r : rows)
            if (r.isPlayer && r.detailsAddr) known[r.detailsAddr] = r.name;
        AppendLine(t, "player skill addresses: %d", static_cast<int>(known.size()));
        constexpr size_t kScan = SkillDPSGame::SkillBarTracker::kSlotScanBytes;
        std::vector<uint8_t> buf(kScan);
        for (size_t i = 0; i < slots.size(); ++i) {
            if (!ctx->Memory.Read(slots[i].addr, buf.data(), kScan)) {
                AppendLine(t, "slot[%d]: read failed", static_cast<int>(i));
                continue;
            }
            int reported = 0;
            for (size_t off = 0; off + 8 <= kScan && reported < 24; off += 8) {
                uint64_t v = 0;
                std::memcpy(&v, buf.data() + off, 8);
                const auto hit = known.find(v);
                if (hit == known.end()) continue;
                AppendLine(t, "slot[%d] +0x%03zx -> %s", static_cast<int>(i), off, hit->second.c_str());
                ++reported;
            }
            if (reported == 0) AppendLine(t, "slot[%d]: no match in the first 0x%zx bytes", static_cast<int>(i), kScan);
        }

        AppendLine(t, "");
        AppendLine(t, "=== visible UI nodes with id/text (depth | stringId | text | rect) ===");
        int visited = 0;
        for (const auto& n : CaptureUiTree(ctx, visited, 800))
            AppendLine(t, "%d | %s | %s | %.0f,%.0f %.0fx%.0f", n.depth, n.stringId.c_str(), n.text.c_str(), n.x, n.y, n.w, n.h);

        const auto path = pluginDir / "debug_dump.txt";
        if (!FileIO::AtomicWrite(path, t)) return false;
        outPath = "debug_dump.txt";
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace SkillDPSUi
