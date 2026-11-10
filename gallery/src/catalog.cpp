#include "catalog.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using nlohmann::json;

const char* currentPlatform() noexcept
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

static std::string str(const json& j, const char* key, const std::string& fallback = {})
{
    auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : fallback;
}

static std::vector<std::string> strings(const json& j, const char* key)
{
    std::vector<std::string> out;
    auto it = j.find(key);
    if (it != j.end() && it->is_array())
        for (const auto& v : *it)
            if (v.is_string())
                out.push_back(v.get<std::string>());
    return out;
}

static fs::path findOnPath(const std::string& name)
{
    const char* path = std::getenv("PATH");
    if (!path)
        return {};
#if defined(_WIN32)
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif
    std::string_view rest = path;
    while (!rest.empty())
    {
        const size_t end = rest.find(separator);
        const std::string_view dir = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);
        std::error_code ec;
        if (!dir.empty())
            if (const fs::path candidate = fs::path(dir) / name; fs::is_regular_file(candidate, ec))
                return candidate;
    }
    return {};
}

static double number(const json& j, const char* key, double fallback)
{
    auto it = j.find(key);
    return (it != j.end() && it->is_number()) ? it->get<double>() : fallback;
}

static DemoOption parseOption(const json& o)
{
    DemoOption opt;
    opt.id = str(o, "id");
    if (opt.id.empty())
        throw std::runtime_error("an option has no \"id\"");
    opt.label = str(o, "label", opt.id);
    opt.help = str(o, "help");
    opt.args = strings(o, "args");
    opt.env = str(o, "env");

    const std::string type = str(o, "type", "int");
    if (type == "int" || type == "float")
    {
        opt.type = type == "int" ? DemoOption::Type::Int : DemoOption::Type::Float;
        opt.min = number(o, "min", 0.0);
        opt.max = number(o, "max", opt.type == DemoOption::Type::Int ? 100.0 : 1.0);
        if (opt.max < opt.min)
            std::swap(opt.min, opt.max);
        opt.default_number = std::clamp(number(o, "default", opt.min), opt.min, opt.max);
    }
    else if (type == "bool")
    {
        opt.type = DemoOption::Type::Bool;
        auto it = o.find("default");
        opt.default_flag = it != o.end() && it->is_boolean() && it->get<bool>();
    }
    else if (type == "choice")
    {
        opt.type = DemoOption::Type::Choice;
        opt.choices = strings(o, "choices");
        if (opt.choices.empty())
            throw std::runtime_error(std::format("option \"{}\" has no \"choices\"", opt.id));
        opt.default_choice = str(o, "default", opt.choices.front());
        if (std::find(opt.choices.begin(), opt.choices.end(), opt.default_choice) == opt.choices.end())
            opt.default_choice = opt.choices.front();
    }
    else
        throw std::runtime_error(std::format("option \"{}\" has unknown type \"{}\"", opt.id, type));
    return opt;
}

static Demo parseDemo(const json& j, const fs::path& manifest_dir, const fs::path& app_dir)
{
    Demo d;
    d.id = manifest_dir.filename().string();
    d.title = str(j, "title", d.id);
    d.category = str(j, "category", "Other");
    d.author = str(j, "author");
    d.summary = str(j, "summary");
    d.description = str(j, "description", d.summary);
    d.how_it_works = strings(j, "how_it_works");
    if (d.how_it_works.empty() && !str(j, "how_it_works").empty())
        d.how_it_works.push_back(str(j, "how_it_works")); // a single paragraph may be a plain string
    d.highlights = strings(j, "highlights");
    d.tags = strings(j, "tags");
    if (const std::string window = str(j, "window", "embed"); window == "none")
        d.window = Demo::Window::None;
    else if (window == "separate")
        d.window = Demo::Window::Separate;
    else if (window != "embed")
        throw std::runtime_error(std::format("\"window\" must be \"embed\", \"separate\" or \"none\", not \"{}\"", window));
    d.args = strings(j, "args");
    d.requires_ = strings(j, "requires");
    d.platforms = strings(j, "platforms");

    if (auto it = j.find("env"); it != j.end() && it->is_object())
        for (const auto& [k, v] : it->items())
            if (v.is_string())
                d.env.emplace_back(k, v.get<std::string>());

    // "controls": [["WASD", "orbit"], ...]
    if (auto it = j.find("controls"); it != j.end() && it->is_array())
        for (const auto& row : *it)
            if (row.is_array() && row.size() == 2 && row[0].is_string() && row[1].is_string())
                d.controls.emplace_back(row[0].get<std::string>(), row[1].get<std::string>());

    if (auto it = j.find("options"); it != j.end() && it->is_array())
        for (const auto& o : *it)
            d.options.push_back(parseOption(o));

    std::string exe = str(j, "executable");
#if defined(_WIN32)
    if (!exe.empty() && fs::path(exe).extension() != ".exe")
        exe += ".exe";
#endif
    bool from_path = false;
    if (!exe.empty())
    {
        d.executable = fs::weakly_canonical(app_dir / exe);
        // A bare name that is not shipped with the gallery is a system program (an interpreter).
        if (exe.find_first_of("/\\") == std::string::npos && !fs::exists(d.executable))
            if (fs::path found = findOnPath(exe); !found.empty())
                d.executable = found, from_path = true;
    }

    // Default working directory: the program's folder, or for a system program the
    // manifest's folder, where its scripts live.
    const std::string cwd = str(j, "working_directory");
    if (!cwd.empty())
        d.working_directory = fs::weakly_canonical(app_dir / cwd);
    else
        d.working_directory = from_path ? manifest_dir : d.executable.parent_path();

    if (const std::string thumb = str(j, "thumbnail"); !thumb.empty() && fs::exists(manifest_dir / thumb))
        d.thumbnail = manifest_dir / thumb;

    // Reasons that do not depend on the hardware; main() adds the OpenCL/OpenGL ones.
    if (!d.platforms.empty() &&
        std::find(d.platforms.begin(), d.platforms.end(), currentPlatform()) == d.platforms.end())
        d.unavailable_reason = std::format("Not available on {}", currentPlatform());
    else if (exe.empty())
        d.unavailable_reason = "Manifest has no \"executable\"";
    else if (!fs::exists(d.executable))
        d.unavailable_reason =
            exe.find_first_of("/\\") == std::string::npos
                ? std::format("{} is not in this build or on the PATH", exe)
                : std::format("Not included in this build ({} missing)", fs::path(exe).filename().string());

    return d;
}

Catalog loadCatalog(const fs::path& demos_dir, const fs::path& app_dir)
{
    Catalog catalog;
    std::error_code ec;
    if (!fs::is_directory(demos_dir, ec))
    {
        catalog.errors.push_back(std::format("Demo folder not found: {}", demos_dir.string()));
        return catalog;
    }

    for (const auto& entry : fs::directory_iterator(demos_dir, ec))
    {
        const fs::path manifest = entry.path() / "demo.json";
        if (!entry.is_directory() || !fs::exists(manifest))
            continue;
        try
        {
            std::ifstream in(manifest);
            catalog.demos.push_back(parseDemo(json::parse(in, nullptr, true, /*ignore_comments=*/true),
                                              entry.path(), app_dir));
        }
        catch (const std::exception& e)
        {
            catalog.errors.push_back(std::format("{}: {}", manifest.string(), e.what()));
        }
    }

    // Stable order: category, then title.
    std::sort(catalog.demos.begin(), catalog.demos.end(), [](const Demo& a, const Demo& b) {
        return std::tie(a.category, a.title) < std::tie(b.category, b.title);
    });
    return catalog;
}
