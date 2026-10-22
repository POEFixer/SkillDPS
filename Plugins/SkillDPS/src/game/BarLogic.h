// Pure helpers behind the skill-bar overlay: picking the key-label row of the
// skill bar out of UI candidates, the icon-size test, number formatting and the
// vote that finds where a bar slot keeps its skill pointer. No SDK, no game
// access: covered by tests/.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace SkillDPSGame {

// UI sizes are measured on a 1080-pixel-high screen and scaled with the game's
// height (the game scales its UI with the vertical resolution).
inline float UiScaleFor(float screenHeight) {
    const float s = screenHeight > 0.f ? screenHeight / 1080.f : 1.f;
    return s < 0.5f ? 0.5f : (s > 4.f ? 4.f : s);
}

struct LabelRect {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    uintptr_t addr = 0;
    std::string text;
};

// The largest horizontal run of short labels that sit on one row (the key
// labels "Q", "W", "LMB"... of the skill bar): rows are grouped within
// 24 px * scale vertically, then split where neighbours are more than
// 200 px * scale apart. Returns at least kMinSlots labels sorted by x, or none.
inline constexpr size_t kMinSlots = 3;
std::vector<LabelRect> PickLabelRow(std::vector<LabelRect> candidates, float scale);

// A skill-bar icon box: 30..120 px * scale on both sides.
bool IsIconSized(float w, float h, float scale);

// "950", "12.3k", "4.56M", "1.20B"; never "1000.0k".
void FormatCompact(float value, char* buf, size_t size);

// Given copies of the first bytes of several bar-slot UI elements and the set
// of the player's skill addresses, returns the 8-aligned offset at which the
// most slots hold one of those addresses (ties: the one nearest `hint`), or -1
// when fewer than `minVotes` slots agree.
int VoteSlotOffset(const std::vector<std::vector<uint8_t>>& slotBytes,
                   const std::unordered_set<uint64_t>& known, size_t hint, int minVotes);

} // namespace SkillDPSGame
