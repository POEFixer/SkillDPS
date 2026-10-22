// The collapsed Diagnostics card: live skill numbers, detected bar slots, a
// UI-tree capture (on request only: it walks thousands of nodes) and the
// debug_dump.txt snapshot for bug reports.
#pragma once

#include "../../../../POEFixer/plugin_sdk/PluginSDK.h"
#include "../game/SkillBar.h"
#include "../game/SkillScanner.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SkillDPSUi {

struct UiNodeRow {
    int depth = 0;
    std::string stringId;
    std::string text;
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
};

struct DiagnosticsInput {
    const std::vector<SkillDPSGame::SkillRow>* rows = nullptr;      // last refresh (with raw stats)
    const std::vector<SkillDPSGame::BarSlot>* slots = nullptr;
    const std::vector<uintptr_t>* slotSkills = nullptr;            // per slot, 0 = none
    const SkillDPSGame::SkillBarTracker* tracker = nullptr;
    bool inGame = false;
};

struct DiagnosticsState {
    bool highlightSlots = false;
    bool uiCaptured = false;
    int  uiVisited = 0;
    std::vector<UiNodeRow> uiTree;
    int  dumpResult = 0;          // 1 written, -1 failed
    std::string dumpPath;
};

void DrawDiagnosticsBody(const PluginSDK::Context* ctx, const DiagnosticsInput& in, DiagnosticsState& st,
                         const std::filesystem::path& pluginDir);

std::vector<UiNodeRow> CaptureUiTree(const PluginSDK::Context* ctx, int& visited, size_t maxRows);

bool WriteDebugDump(const PluginSDK::Context* ctx, const DiagnosticsInput& in,
                    const std::filesystem::path& pluginDir, std::string& outPath);

} // namespace SkillDPSUi
