#pragma once

#include <filesystem>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

// What the gallery remembers between runs. Stored as JSON in the user's settings folder:
// %APPDATA%\compute-gallery (Windows), ~/Library/Application Support/compute-gallery
// (macOS), $XDG_CONFIG_HOME or ~/.config/compute-gallery (Linux).
struct Settings
{
    // OpenCL device by name rather than index, which can change when drivers do. Empty = auto.
    std::string cl_platform;
    std::string cl_device;
    std::string selected_demo;
    int window_width = 0;  // 0 = not remembered
    int window_height = 0;
    bool embed_demos = true;   // show demos inside the gallery window where the OS allows it
    bool animations = true;    // animated background and logo (off anyway on software rendering)
    float ui_scale = 0.0f;     // 0 = follow the screen's scale
    bool welcome_dismissed = false;
    // Raw option values per demo id and option id; checked against the manifest on use.
    std::map<std::string, std::map<std::string, nlohmann::json>> options;
};

std::filesystem::path settingsPath();

// Missing or unreadable settings give defaults.
Settings loadSettings();

void saveSettings(const Settings& settings);
