// Skill DPS settings. File: <plugin folder>/config/settings.json, the same file
// and keys as the original plugin (new keys are added; old ones still read).
#pragma once

#include <imgui.h>

#include <filesystem>
#include <string>

namespace SkillDPSConfig {

inline constexpr int kScanIntervalMinMs = 50;
inline constexpr int kScanIntervalMaxMs = 1000;

enum class LabelPlace : int { Top = 0, Bottom = 1 };

struct Settings {
    // What to show
    bool  showOnIcons = true;
    bool  showList = false;
    bool  showMinions = true;
    // Icon labels
    float labelScale = 1.0f;       // x the overlay font size
    float labelOpacity = 0.70f;    // background pill alpha
    LabelPlace labelPlace = LabelPlace::Top;
    ImVec4 dpsColor = ImVec4(1.00f, 0.86f, 0.47f, 1.0f);
    ImVec4 avgColor = ImVec4(0.62f, 0.85f, 1.00f, 1.0f);
    // DPS list panel
    float overlayX = 20.f;
    float overlayY = 300.f;
    float listScale = 1.0f;
    float listOpacity = 0.75f;
    // Refresh
    int   scanIntervalMs = 250;

    void Load(const std::filesystem::path& pluginDir);
    bool Save(const std::filesystem::path& pluginDir) const;
    std::string ToJson() const;
    void FromJson(const std::string& text);
    static std::filesystem::path FilePath(const std::filesystem::path& pluginDir) {
        return pluginDir / "config" / "settings.json";
    }
};

} // namespace SkillDPSConfig
