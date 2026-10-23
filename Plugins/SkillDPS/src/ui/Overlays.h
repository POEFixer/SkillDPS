// In-game drawing: the number on each skill-bar icon and the movable DPS list.
// Both draw from data prepared at refresh time; nothing here reads the game.
#pragma once

#include "../Settings.h"
#include "../game/SkillBar.h"

#include <string>
#include <vector>

namespace SkillDPSUi {

struct IconLabel {
    SkillDPSGame::BarSlot slot;
    char text[24] = {};
    bool average = false;     // "~" average-hit value
};

struct ListRow {
    std::string name;
    std::string owner;        // empty for the player's own skills
    char value[24] = {};
    bool average = false;
};

// Labels centred on the icons, under every ImGui window (the menu covers them).
void DrawIconLabels(const std::vector<IconLabel>& labels, const SkillDPSConfig::Settings& s, float uiScale);

// The DPS list as a small translucent panel, an informational window of the host
// (`overlay` = ctx()->Overlay): the host places it, remembers its place in and out
// of the game and draws its gear menu (lock, Game overlay mode, reset position,
// close). In game it can be dragged only while `movable` (the plugin's settings
// are open); out of the game always. Close clears s.showList.
void DrawListPanel(const PluginSDK::OverlayService& overlay, const std::vector<ListRow>& rows,
                   SkillDPSConfig::Settings& s, bool movable);

// Diagnostics aid: outlines of the detected slots with their index.
void DrawSlotOutlines(const std::vector<SkillDPSGame::BarSlot>& slots);

} // namespace SkillDPSUi
