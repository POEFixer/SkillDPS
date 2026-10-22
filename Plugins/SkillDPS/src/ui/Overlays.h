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

// The DPS list as a small translucent panel. While `movable` (the plugin's
// settings are open) it can be dragged; returns true when it moved, with the
// new position written into the settings. `forcePosition` places it at the
// saved position this frame even while movable (after "Reset position").
bool DrawListPanel(const std::vector<ListRow>& rows, SkillDPSConfig::Settings& s, bool movable,
                   bool forcePosition = false);

// Diagnostics aid: outlines of the detected slots with their index.
void DrawSlotOutlines(const std::vector<SkillDPSGame::BarSlot>& slots);

} // namespace SkillDPSUi
