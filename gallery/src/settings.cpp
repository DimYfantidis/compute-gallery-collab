#include "settings.hpp"

#include <cstdlib>
#include <fstream>

namespace fs = std::filesystem;
using nlohmann::json;

fs::path settingsPath()
{
    auto env = [](const char* name) -> fs::path {
        const char* value = std::getenv(name);
        return value && *value ? fs::path(value) : fs::path();
    };
#if defined(_WIN32)
    fs::path base = env("APPDATA");
#elif defined(__APPLE__)
    fs::path base = env("HOME") / "Library" / "Application Support";
#else
    fs::path base = env("XDG_CONFIG_HOME");
    if (base.empty())
        base = env("HOME") / ".config";
#endif
    return base / "compute-gallery" / "settings.json";
}

Settings loadSettings()
{
    Settings s;
    std::ifstream in(settingsPath());
    if (!in)
        return s;
    try
    {
        const json j = json::parse(in);
        s.cl_platform = j.value("cl_platform", "");
        s.cl_device = j.value("cl_device", "");
        s.selected_demo = j.value("selected_demo", "");
        s.window_width = j.value("window_width", 0);
        s.window_height = j.value("window_height", 0);
        s.embed_demos = j.value("embed_demos", true);
        s.animations = j.value("animations", true);
        s.ui_scale = j.value("ui_scale", 0.0f);
        s.welcome_dismissed = j.value("welcome_dismissed", false);
        if (auto it = j.find("options"); it != j.end() && it->is_object())
            for (const auto& [demo, values] : it->items())
                if (values.is_object())
                    for (const auto& [option, value] : values.items())
                        s.options[demo][option] = value;
    }
    catch (const std::exception&)
    {
        return Settings{}; // a damaged file is not worth failing over
    }
    return s;
}

void saveSettings(const Settings& s)
{
    const fs::path path = settingsPath();
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);

    json j = {
        {"cl_platform", s.cl_platform},
        {"cl_device", s.cl_device},
        {"selected_demo", s.selected_demo},
        {"window_width", s.window_width},
        {"window_height", s.window_height},
        {"embed_demos", s.embed_demos},
        {"animations", s.animations},
        {"ui_scale", s.ui_scale},
        {"welcome_dismissed", s.welcome_dismissed},
        {"options", json::object()},
    };
    for (const auto& [demo, values] : s.options)
        for (const auto& [option, value] : values)
            j["options"][demo][option] = value;

    std::ofstream out(path);
    out << j.dump(4) << '\n';
}
