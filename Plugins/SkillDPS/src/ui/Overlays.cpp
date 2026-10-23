#include "Overlays.h"

#include "../../../../POEFixer/plugin_sdk/PluginUi.h"
#include "../../../../POEFixer/plugin_sdk/PluginInfoWindow.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace SkillDPSUi {

void DrawIconLabels(const std::vector<IconLabel>& labels, const SkillDPSConfig::Settings& s, float uiScale) {
    if (labels.empty()) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImFont* font = ImGui::GetFont();
    const float size = std::round(ImGui::GetFontSize() * s.labelScale);
    const float padX = std::round(std::max(2.f, size * 0.30f));
    const float padY = std::round(std::max(1.f, size * 0.08f));
    const float inset = std::round(std::max(1.f, 2.f * uiScale));
    const ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(0.f, 0.f, 0.f, s.labelOpacity));

    for (const auto& l : labels) {
        const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.f, l.text);
        const float boxH = ts.y + padY * 2.f;
        const float x = std::floor(l.slot.x + l.slot.w * 0.5f - ts.x * 0.5f);
        const float y = s.labelPlace == SkillDPSConfig::LabelPlace::Top
            ? std::floor(l.slot.y + inset)
            : std::floor(l.slot.y + l.slot.h - inset - boxH);
        if (s.labelOpacity > 0.01f)
            dl->AddRectFilled(ImVec2(x - padX, y), ImVec2(x + ts.x + padX, y + boxH), bg, boxH * 0.35f);
        // Outlined so the number stays readable without a plate.
        PluginUi::TextOutlined(dl, font, size, ImVec2(x, y + padY),
                               ImGui::ColorConvertFloat4ToU32(l.average ? s.avgColor : s.dpsColor), l.text);
    }
}

// The list panel's contents (the window is open).
static void DrawListContents(const std::vector<ListRow>& rows, const SkillDPSConfig::Settings& s, bool movable) {
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * s.listScale);

    ImGui::TextColored(s.dpsColor, "Skill DPS");
    if (movable) {
        ImGui::SameLine();
        ImGui::TextDisabled("drag to move");
    }

    // Two measured columns: names, then values right-aligned.
    float nameW = 0.f, valueW = 0.f;
    char nameBuf[160];
    for (const auto& r : rows) {
        if (r.owner.empty()) std::snprintf(nameBuf, sizeof(nameBuf), "%s", r.name.c_str());
        else std::snprintf(nameBuf, sizeof(nameBuf), "%s (%s)", r.name.c_str(), r.owner.c_str());
        nameW = std::max(nameW, ImGui::CalcTextSize(nameBuf).x);
        valueW = std::max(valueW, ImGui::CalcTextSize(r.value).x);
    }
    const float colX = ImGui::GetCursorPosX() + nameW + ImGui::GetStyle().ItemSpacing.x * 2.f;

    if (rows.empty()) {
        ImGui::TextDisabled("No skill evaluated yet.");
        ImGui::TextDisabled("Select a skill in the character panel or use it.");
    }
    for (const auto& r : rows) {
        const bool minion = !r.owner.empty();
        if (minion) std::snprintf(nameBuf, sizeof(nameBuf), "%s (%s)", r.name.c_str(), r.owner.c_str());
        else std::snprintf(nameBuf, sizeof(nameBuf), "%s", r.name.c_str());
        const ImVec4 nameColor = minion ? ImVec4(0.62f, 0.80f, 1.0f, 1.0f) : ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
        ImGui::TextColored(nameColor, "%s", nameBuf);
        ImGui::SameLine();
        ImGui::SetCursorPosX(colX + valueW - ImGui::CalcTextSize(r.value).x);
        ImGui::TextColored(r.average ? s.avgColor : s.dpsColor, "%s", r.value);
    }
    ImGui::PopFont();
}

void DrawListPanel(const PluginSDK::OverlayService& overlay, const std::vector<ListRow>& rows,
                   SkillDPSConfig::Settings& s, bool movable) {
    const float em = ImGui::GetFontSize();

    // In game the panel is click-through unless the settings tab is open; out of
    // the game the host drops NoMove / NoInputs (it is always movable there).
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (!movable) flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;

    // The host owns the place; overlayX/Y (the place before POEFixer 366) only seed
    // its store once.
    PluginSDK::InfoWindowOptions o;
    o.id = "list";
    o.title = nullptr;   // NoDecoration: the gear sits in the top-right corner on hover
    o.flags = flags;
    o.defaultPos = ImVec2(20.f, 300.f);
    o.legacyPos = ImVec2(s.overlayX, s.overlayY);

    const float scaled = em * s.listScale;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, std::round(scaled * 0.4f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(std::round(scaled * 0.6f), std::round(scaled * 0.45f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(std::round(scaled * 0.5f), std::round(scaled * 0.15f)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.06f, s.listOpacity));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.f, 1.f, 1.f, movable ? 0.35f : 0.10f * s.listOpacity));
    // The panel is always dark: a fixed grey for secondary text, not the theme's
    // (a light theme's disabled text is too dark to read on it).
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.62f, 0.62f, 0.64f, 1.f));

    {
        PluginSDK::InfoWindow window(overlay, o, &s.showList);
        if (window) DrawListContents(rows, s, movable);
    }   // the window ends here; its style is popped only after it
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(4);
}

void DrawSlotOutlines(const std::vector<SkillDPSGame::BarSlot>& slots) {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    char buf[8];
    for (size_t i = 0; i < slots.size(); ++i) {
        const auto& s = slots[i];
        // ImGui 1.93 strokes a rectangle inside its bounds: whole-pixel corners
        // give a crisp 1 px frame on the slot's own edge.
        const ImVec2 a(std::floor(s.x), std::floor(s.y));
        const ImVec2 b(std::floor(s.x + s.w), std::floor(s.y + s.h));
        dl->AddRect(a, b, IM_COL32(120, 255, 120, 220), 0.f, 1.f);
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(i));
        PluginUi::TextOutlined(dl, ImVec2(a.x + 3.f, a.y + 2.f), IM_COL32(120, 255, 120, 255), buf);
    }
}

} // namespace SkillDPSUi
