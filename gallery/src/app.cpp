#include "app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>

#include <GLFW/glfw3.h>

#include "icons.hpp"

namespace fs = std::filesystem;

// ----------------------------------------------------------------------------
// Demos
// ----------------------------------------------------------------------------

int findDemo(const App& app, const std::string& id)
{
    for (size_t i = 0; i < app.catalog.demos.size(); ++i)
        if (app.catalog.demos[i].id == id)
            return static_cast<int>(i);
    return -1;
}

const Demo* selectedDemo(const App& app)
{
    if (app.catalog.demos.empty())
        return nullptr;
    return &app.catalog.demos[std::clamp(app.selected, 0, static_cast<int>(app.catalog.demos.size()) - 1)];
}

static bool needs(const Demo& d, const char* what)
{
    return std::find(d.requires_.begin(), d.requires_.end(), what) != d.requires_.end();
}

void applyHardwareAvailability(App& app)
{
    for (Demo& d : app.catalog.demos)
        if (d.available() && needs(d, "opencl") && app.sys.cl_devices.empty())
            d.unavailable_reason = app.sys.cl_error;
}

const Texture* thumbnail(App& app, const Demo& d)
{
    if (d.thumbnail.empty())
        return nullptr;
    auto it = app.thumbnails.find(d.id);
    if (it == app.thumbnails.end())
        it = app.thumbnails.emplace(d.id, loadTexture(d.thumbnail)).first;
    return it->second ? &it->second : nullptr;
}

// A remembered value if it still fits the option's definition, else the default.
static OptionValue rememberedValue(const App& app, const Demo& d, const DemoOption& o)
{
    OptionValue v = defaultValue(o);
    auto demo_it = app.settings.options.find(d.id);
    if (demo_it == app.settings.options.end())
        return v;
    auto it = demo_it->second.find(o.id);
    if (it == demo_it->second.end())
        return v;
    const nlohmann::json& j = it->second;
    switch (o.type)
    {
    case DemoOption::Type::Int:
    case DemoOption::Type::Float:
        if (j.is_number())
            v.number = std::clamp(j.get<double>(), o.min, o.max);
        break;
    case DemoOption::Type::Bool:
        if (j.is_boolean())
            v.flag = j.get<bool>();
        break;
    case DemoOption::Type::Choice:
        if (j.is_string() && std::find(o.choices.begin(), o.choices.end(), j.get<std::string>()) != o.choices.end())
            v.choice = j.get<std::string>();
        break;
    }
    return v;
}

OptionValue& optionValue(App& app, const Demo& d, const DemoOption& o)
{
    auto& values = app.options[d.id];
    auto it = values.find(o.id);
    if (it == values.end())
        it = values.emplace(o.id, rememberedValue(app, d, o)).first;
    return it->second;
}

// Copies the values shown in the UI into app.settings, in the types the manifest declares.
static void syncOptions(App& app)
{
    for (const auto& [demo_id, values] : app.options)
    {
        const int i = findDemo(app, demo_id);
        if (i < 0)
            continue;
        for (const DemoOption& o : app.catalog.demos[i].options)
        {
            auto it = values.find(o.id);
            if (it == values.end())
                continue;
            nlohmann::json& j = app.settings.options[demo_id][o.id];
            switch (o.type)
            {
            case DemoOption::Type::Int: j = std::llround(it->second.number); break;
            case DemoOption::Type::Float: j = it->second.number; break;
            case DemoOption::Type::Bool: j = it->second.flag; break;
            case DemoOption::Type::Choice: j = it->second.choice; break;
            }
        }
    }
}

void reloadCatalog(App& app)
{
    const Demo* current = selectedDemo(app);
    const std::string selected = current ? current->id : std::string();
    for (auto& [id, tex] : app.thumbnails)
        destroyTexture(tex);
    app.thumbnails.clear();
    // Keep the values set so far, re-checked against the manifests as they are now.
    syncOptions(app);
    app.options.clear();
    app.catalog = loadCatalog(app.demos_dir, app.app_dir);
    applyHardwareAvailability(app);
    app.selected = std::max(0, findDemo(app, selected));
}

bool embedsInGallery(const App& app, const Demo& d)
{
    return d.window == Demo::Window::Embed && app.settings.embed_demos && app.embedder && app.embedder->supported();
}

bool launch(App& app, const Demo& d)
{
    if (!d.available() || app.runner.running())
        return false;

    std::vector<std::string> args;
    std::vector<std::pair<std::string, std::string>> env;
    for (const DemoOption& o : d.options)
        optionValue(app, d, o); // make sure remembered values are loaded
    appendOptionParameters(d, app.options[d.id], args, env);
    if (app.cl_choice >= 0 && app.cl_choice < static_cast<int>(app.sys.cl_devices.size()))
    {
        const ClDevice& dev = app.sys.cl_devices[app.cl_choice];
        env.emplace_back("CG_CL_PLATFORM", std::to_string(dev.platform_index));
        env.emplace_back("CG_CL_DEVICE", std::to_string(dev.device_index));
    }
#if defined(__linux__)
    // Windows can only be embedded on X11: ask GLFW and SDL programs for X11 even in a
    // Wayland session (through XWayland).
    if (embedsInGallery(app, d) && std::getenv("DISPLAY"))
    {
        env.emplace_back("XDG_SESSION_TYPE", "x11");
        env.emplace_back("SDL_VIDEODRIVER", "x11");
    }
#endif

    const bool started = app.runner.start(d, args, env);
    app.last_run_id = d.id;
    app.selected = findDemo(app, d.id);
    if (!started)
    {
        toast(app, ICON_CIRCLE_X, std::format("{} could not start", d.title), palette::danger);
        return false;
    }
    app.launched_at = app.now;
    app.stage_console = d.window == Demo::Window::None;
    if (app.embedder)
        app.embedder->reset();
    openPage(app, Page::Stage);
    if (app.use_settings)
        storeSettings(app, nullptr); // so a crash does not lose the choices just made
    return true;
}

// ----------------------------------------------------------------------------
// Navigation, feedback
// ----------------------------------------------------------------------------

void openPage(App& app, Page page)
{
    if (app.page != page)
        app.page_since = app.now;
    app.page = page;
}

void toast(App& app, const char* icon, std::string text, ImU32 color)
{
    app.toasts.push_back({icon, std::move(text), color, app.now});
    if (app.toasts.size() > 4)
        app.toasts.erase(app.toasts.begin());
}

// ----------------------------------------------------------------------------
// Settings, environment
// ----------------------------------------------------------------------------

void storeSettings(App& app, GLFWwindow* window)
{
    syncOptions(app);
    Settings& s = app.settings;
    s.cl_platform = s.cl_device = "";
    if (app.cl_choice >= 0 && app.cl_choice < static_cast<int>(app.sys.cl_devices.size()))
    {
        s.cl_platform = app.sys.cl_devices[app.cl_choice].platform;
        s.cl_device = app.sys.cl_devices[app.cl_choice].name;
    }
    if (const Demo* d = selectedDemo(app))
        s.selected_demo = d->id;
    if (window && !glfwGetWindowAttrib(window, GLFW_ICONIFIED) && !glfwGetWindowAttrib(window, GLFW_MAXIMIZED))
        glfwGetWindowSize(window, &s.window_width, &s.window_height);
    saveSettings(s);
}

void detectEnvironment(App& app)
{
    // run.sh passes the compose service it started as CG_ENVIRONMENT.
    if (const char* env = std::getenv("CG_ENVIRONMENT"); env && *env)
        app.environment = std::format("Docker ({})", env);
    else if (fs::exists("/.dockerenv"))
        app.environment = "Docker";
    else
        app.environment = "Native";

    // Running from a CMake build folder: the developer tools can rebuild it.
    if (fs::exists(app.app_dir / "CMakeCache.txt"))
        app.build_dir = app.app_dir;

    const std::string& r = app.sys.gl_renderer;
    app.software_rendering = r.find("llvmpipe") != std::string::npos || r.find("softpipe") != std::string::npos ||
                             r.find("Basic Render") != std::string::npos || r.find("GDI Generic") != std::string::npos;
}

void startRebuild(App& app)
{
    if (app.build_dir.empty() || app.builder.running())
        return;
    Demo cmake;
    cmake.id = "rebuild";
    const fs::path found = [] {
#if defined(_WIN32)
        const char* name = "cmake.exe";
        const char sep = ';';
#else
        const char* name = "cmake";
        const char sep = ':';
#endif
        const char* path = std::getenv("PATH");
        std::string rest = path ? path : "";
        while (!rest.empty())
        {
            const size_t end = rest.find(sep);
            const fs::path candidate = fs::path(rest.substr(0, end)) / name;
            if (fs::is_regular_file(candidate))
                return candidate;
            rest = end == std::string::npos ? "" : rest.substr(end + 1);
        }
        return fs::path();
    }();
    if (found.empty())
    {
        toast(app, ICON_TRIANGLE_ALERT, "CMake is not on the PATH", palette::warning);
        return;
    }
    cmake.executable = found;
    cmake.working_directory = app.build_dir;
    cmake.args = {"--build", app.build_dir.string()};
    app.builder.start(cmake, {}, {});
    app.build_reported = false;
}
