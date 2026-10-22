#include "Settings.h"

#include "../../../POEFixer/core/FileIO.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <system_error>

namespace SkillDPSConfig {

namespace {

template <typename T>
void ReadBool(const nlohmann::json& j, const char* key, T& dst) {
    const auto it = j.find(key);
    if (it != j.end() && it->is_boolean()) dst = it->get<bool>();
}

void ReadFloat(const nlohmann::json& j, const char* key, float& dst, float lo, float hi) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_number()) return;
    const float v = it->get<float>();
    if (std::isfinite(v)) dst = std::clamp(v, lo, hi);
}

void ReadColor(const nlohmann::json& j, const char* key, ImVec4& dst) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_array() || it->size() != 4) return;
    float c[4];
    for (size_t i = 0; i < 4; ++i) {
        if (!(*it)[i].is_number()) return;
        const float v = (*it)[i].get<float>();
        if (!std::isfinite(v)) return;
        c[i] = std::clamp(v, 0.f, 1.f);
    }
    dst = ImVec4(c[0], c[1], c[2], c[3]);
}

} // namespace

void Settings::FromJson(const std::string& text) {
    const nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return;
    ReadBool(j, "show_on_icons", showOnIcons);
    ReadBool(j, "show_list", showList);
    ReadBool(j, "show_minions", showMinions);
    ReadFloat(j, "label_scale", labelScale, 0.7f, 1.8f);
    ReadFloat(j, "label_opacity", labelOpacity, 0.f, 1.f);
    if (const auto it = j.find("label_place"); it != j.end() && it->is_number_integer())
        labelPlace = it->get<int>() == 1 ? LabelPlace::Bottom : LabelPlace::Top;
    ReadColor(j, "dps_color", dpsColor);
    ReadColor(j, "avg_color", avgColor);
    ReadFloat(j, "overlay_x", overlayX, 0.f, 7680.f);
    ReadFloat(j, "overlay_y", overlayY, 0.f, 4320.f);
    ReadFloat(j, "list_scale", listScale, 0.7f, 1.8f);
    ReadFloat(j, "list_opacity", listOpacity, 0.f, 1.f);
    if (const auto it = j.find("scan_interval_ms"); it != j.end() && it->is_number_integer())
        scanIntervalMs = std::clamp(it->get<int>(), kScanIntervalMinMs, kScanIntervalMaxMs);
    // The original master switch is now the host's Enabled box. A saved
    // "enabled": false (show nothing) is honoured once by switching both
    // displays off; the key is not written again. "debug_mode" is gone:
    // diagnostics live in a collapsed card.
    if (const auto it = j.find("enabled"); it != j.end() && it->is_boolean() && !it->get<bool>()) {
        showOnIcons = false;
        showList = false;
    }
}

std::string Settings::ToJson() const {
    nlohmann::json j;
    j["version"] = 2;
    j["show_on_icons"] = showOnIcons;
    j["show_list"] = showList;
    j["show_minions"] = showMinions;
    j["label_scale"] = labelScale;
    j["label_opacity"] = labelOpacity;
    j["label_place"] = static_cast<int>(labelPlace);
    j["dps_color"] = {dpsColor.x, dpsColor.y, dpsColor.z, dpsColor.w};
    j["avg_color"] = {avgColor.x, avgColor.y, avgColor.z, avgColor.w};
    j["overlay_x"] = overlayX;
    j["overlay_y"] = overlayY;
    j["list_scale"] = listScale;
    j["list_opacity"] = listOpacity;
    j["scan_interval_ms"] = scanIntervalMs;
    return j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
}

void Settings::Load(const std::filesystem::path& pluginDir) {
    try {
        const auto path = FilePath(pluginDir);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) return;
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return;
        FromJson(std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
    } catch (...) {
    }
}

bool Settings::Save(const std::filesystem::path& pluginDir) const {
    try {
        if (pluginDir.empty()) return false;
        std::error_code ec;
        std::filesystem::create_directories(pluginDir / "config", ec);
        return FileIO::AtomicWrite(FilePath(pluginDir), ToJson());
    } catch (...) {
        return false;
    }
}

} // namespace SkillDPSConfig
