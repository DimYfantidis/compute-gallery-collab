// Compute Gallery: shows every demo described in demos/*/demo.json, explains it and runs it
// (inside this window where the system allows), with its console output at hand.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "app.hpp"
#include "background.hpp"
#include "widgets.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

static fs::path executableDir()
{
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(n);
    return fs::path(buffer).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    _NSGetExecutablePath(buffer.data(), &size);
    return fs::canonical(buffer.c_str()).parent_path();
#else
    return fs::canonical("/proc/self/exe").parent_path();
#endif
}

// Folder holding demos/, assets/ and the demo programs: next to the executable, or inside a
// macOS .app bundle, where the program is in Contents/MacOS and the rest in Contents/Resources.
static fs::path appRoot()
{
    const fs::path dir = executableDir();
#if defined(__APPLE__)
    if (dir.filename() == "MacOS" && dir.parent_path().filename() == "Contents")
        return dir.parent_path() / "Resources";
#endif
    return dir;
}

// ----------------------------------------------------------------------------
// Command line
// ----------------------------------------------------------------------------

struct Options
{
    fs::path demos_dir;
    std::string select;
    std::string launch;
    std::string page;
    fs::path screenshot;
    int width = 1440;
    int height = 900;
    bool size_given = false;
    bool list = false;
    bool exit_when_done = false;
};

static void printUsage()
{
    std::puts(
        "Usage: compute-gallery [options]\n"
        "  --list             print the demos and whether they can run here, then exit\n"
        "  --demos <dir>      read demo manifests from <dir> (default: <app>/demos)\n"
        "  --select <id>      show this demo (in the hero and on the demo page)\n"
        "  --page <name>      open discover, demo, system, settings or help\n"
        "  --launch <id>      launch a demo at startup\n"
        "  --exit-when-done   with --launch: quit when the demo exits, with its exit code\n"
        "  --screenshot <png> save a screenshot of the gallery and exit\n"
        "  --size <W>x<H>     initial window size (default 1440x900)");
}

static bool parseOptions(int argc, char** argv, Options& o)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto value = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--list") o.list = true;
        else if (a == "--demos") o.demos_dir = value();
        else if (a == "--select") o.select = value();
        else if (a == "--page") o.page = value();
        else if (a == "--launch") o.launch = value();
        else if (a == "--exit-when-done") o.exit_when_done = true;
        else if (a == "--screenshot") o.screenshot = value();
        else if (a == "--size")
        {
            if (std::sscanf(value().c_str(), "%dx%d", &o.width, &o.height) != 2)
                return false;
            o.size_given = true;
        }
        else return false;
    }
    return true;
}

static int printList(const App& app)
{
    std::printf("Compute Gallery: %zu demo(s) in %s\n", app.catalog.demos.size(), app.demos_dir.string().c_str());
    if (app.sys.cl_devices.empty())
        std::printf("OpenCL: %s\n", app.sys.cl_error.c_str());
    for (const ClDevice& dev : app.sys.cl_devices)
        std::printf("OpenCL: [%u:%u] %s %s (%s)\n", dev.platform_index, dev.device_index, dev.gpu ? "GPU" : "CPU",
                    dev.name.c_str(), dev.platform.c_str());
    std::printf("\n");
    for (const Demo& d : app.catalog.demos)
        std::printf("  %-6s %-16s %-26s %s\n", d.available() ? "ready" : "--", d.id.c_str(), d.title.c_str(),
                    d.available() ? d.category.c_str() : d.unavailable_reason.c_str());
    for (const std::string& err : app.catalog.errors)
        std::printf("  error: %s\n", err.c_str());
    return app.catalog.errors.empty() ? EXIT_SUCCESS : EXIT_FAILURE;
}

// ----------------------------------------------------------------------------
// Window
// ----------------------------------------------------------------------------

// Fonts are rasterised at the screen's density. macOS lays out in points (the framebuffer is
// already 2x), so there text is drawn back at 1/scale and sizes are not scaled.
static void applyScale(App& app, float scale)
{
    scale = std::max(0.75f, scale);
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    app.fonts = loadFonts(app.app_dir / "assets" / "fonts", scale);
    ImGui::GetStyle() = ImGuiStyle();
#if defined(__APPLE__)
    io.FontGlobalScale = 1.0f / scale;
    applyTheme(1.0f);
#else
    io.FontGlobalScale = 1.0f;
    applyTheme(scale);
#endif
}

// Title bar / taskbar icon. macOS takes it from the .app bundle and Wayland from the desktop
// file, so GLFW cannot (and need not) set it there.
static void setWindowIcon(GLFWwindow* window, const fs::path& icon_dir)
{
#if !defined(__APPLE__)
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND)
        return;
    std::vector<Image> images;
    for (int size : {16, 32, 48, 256})
        if (Image img = loadImage(icon_dir / std::format("compute-gallery-{}.png", size)); !img.pixels.empty())
            images.push_back(std::move(img));
    std::vector<GLFWimage> glfw_images;
    for (Image& img : images)
        glfw_images.push_back({img.width, img.height, img.pixels.data()});
    if (!glfw_images.empty())
        glfwSetWindowIcon(window, static_cast<int>(glfw_images.size()), glfw_images.data());
#else
    (void)window, (void)icon_dir;
#endif
}

static Page pageFromName(const std::string& name)
{
    if (name == "demo") return Page::Demo;
    if (name == "system") return Page::System;
    if (name == "settings") return Page::Settings;
    if (name == "help") return Page::Help;
    return Page::Discover;
}

int main(int argc, char** argv)
{
#if defined(_WIN32)
    // The gallery is a GUI program: unless its output is redirected, print to the terminal it
    // was started from (--list, errors).
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if ((out == nullptr || out == INVALID_HANDLE_VALUE) && AttachConsole(ATTACH_PARENT_PROCESS))
    {
        std::freopen("CONOUT$", "w", stdout);
        std::freopen("CONOUT$", "w", stderr);
    }
#endif
    Options opt;
    if (!parseOptions(argc, argv, opt))
    {
        printUsage();
        return EXIT_FAILURE;
    }

    App app;
    app.app_dir = appRoot();
    app.demos_dir = opt.demos_dir.empty() ? app.app_dir / "demos" : fs::absolute(opt.demos_dir);
    app.catalog = loadCatalog(app.demos_dir, app.app_dir);
    queryOpenCL(app.sys);
    applyHardwareAvailability(app);
    if (opt.list)
        return printList(app);

    // Scripted runs (screenshots, CI) neither read nor write the user's settings.
    app.use_settings = opt.screenshot.empty() && !opt.exit_when_done;
    if (app.use_settings)
    {
        app.settings = loadSettings();
        for (size_t i = 0; i < app.sys.cl_devices.size(); ++i)
            if (app.sys.cl_devices[i].platform == app.settings.cl_platform &&
                app.sys.cl_devices[i].name == app.settings.cl_device)
                app.cl_choice = static_cast<int>(i);
        app.selected = std::max(0, findDemo(app, app.settings.selected_demo));
        if (!opt.size_given && app.settings.window_width > 0 && app.settings.window_height > 0)
            opt.width = app.settings.window_width, opt.height = app.settings.window_height;
    }
    else
        app.settings.welcome_dismissed = !opt.screenshot.empty() && opt.page != "welcome";

    glfwSetErrorCallback([](int code, const char* msg) { std::fprintf(stderr, "GLFW error %d: %s\n", code, msg); });
#if defined(__linux__)
    // Demo windows can only be embedded on X11, so prefer it (XWayland in a Wayland session).
    if (std::getenv("DISPLAY") && glfwPlatformSupported(GLFW_PLATFORM_X11))
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
    if (!glfwInit())
        return EXIT_FAILURE;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(opt.width, opt.height, "Compute Gallery", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return EXIT_FAILURE;
    }
    app.window = window;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (!gladLoadGL(glfwGetProcAddress))
        return EXIT_FAILURE;
    queryOpenGL(app.sys);
    detectEnvironment(app);
    setWindowIcon(window, app.app_dir / "assets" / "icons");
    app.embedder = std::make_unique<WindowEmbedder>(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // layout is not user-adjustable; keep the install folder clean
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    float xscale = 1.0f, yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    applyScale(app, app.settings.ui_scale > 0 ? app.settings.ui_scale : std::max(xscale, yscale));
    glfwSetWindowUserPointer(window, &app);
    glfwSetWindowContentScaleCallback(window, [](GLFWwindow* w, float x, float y) {
        App* a = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (a->settings.ui_scale <= 0)
            a->pending_scale = std::max(x, y);
    });

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    auto background = std::make_unique<Background>(); // owns GL objects: released before the context

    if (findDemo(app, app.settings.selected_demo) < 0)
        for (size_t i = 0; i < app.catalog.demos.size(); ++i)
            if (app.catalog.demos[i].available() && app.catalog.demos[i].window != Demo::Window::None)
            {
                app.selected = static_cast<int>(i); // feature something to look at first
                break;
            }
    if (!opt.select.empty())
        app.selected = std::max(0, findDemo(app, opt.select));
    if (!opt.page.empty())
        app.page = pageFromName(opt.page);
    app.page_since = -10.0; // no fade-in for the first page
    bool launched = false;
    int exit_code = EXIT_SUCCESS;
    const bool scripted = !opt.screenshot.empty();

    for (int frame = 0; !glfwWindowShouldClose(window); ++frame)
    {
        app.now = glfwGetTime();
        // Smooth frames while something moves; otherwise sleep until input arrives (or the
        // log may have changed), so the gallery leaves the GPU to a running demo.
        const bool focused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
        const bool animated_bg = app.settings.animations && !app.software_rendering && app.page != Page::Stage && focused;
        if (scripted || animated_bg || ui::animating())
            glfwPollEvents();
        else
            glfwWaitEventsTimeout(0.1);

        // Moved to a screen with another scale, or the size was changed in Settings.
        if (app.pending_scale > 0.0f)
        {
            applyScale(app, app.pending_scale);
            app.pending_scale = 0.0f;
            ImGui_ImplOpenGL3_DestroyFontsTexture();
            ImGui_ImplOpenGL3_CreateFontsTexture();
        }

        updateStage(app);
        if (!opt.launch.empty() && !launched && frame == 1)
        {
            launched = true;
            const int i = findDemo(app, opt.launch);
            if (i < 0 || !app.catalog.demos[i].available())
            {
                std::fprintf(stderr, "Cannot launch '%s': %s\n", opt.launch.c_str(),
                             i < 0 ? "no such demo" : app.catalog.demos[i].unavailable_reason.c_str());
                exit_code = EXIT_FAILURE;
                if (opt.exit_when_done)
                    break;
            }
            else
                launch(app, app.catalog.demos[i]);
        }
        if (opt.exit_when_done && launched && app.runner.hasExited() && !app.runner.running())
        {
            std::fputs(app.runner.log().c_str(), stdout);
            exit_code = app.runner.exitCode();
            break;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ui::beginFrame();
        drawGallery(app);
        ImGui::Render();

        int fb_w = 0, fb_h = 0;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        background->draw(fb_w, fb_h, static_cast<float>(app.now), animated_bg);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Once thumbnails and layout have settled.
        if (scripted && frame >= 8 && app.now > 0.6)
        {
            exit_code = saveFramebufferPng(opt.screenshot, fb_w, fb_h) ? EXIT_SUCCESS : EXIT_FAILURE;
            break;
        }
        glfwSwapBuffers(window);
    }

    if (app.use_settings)
        storeSettings(app, window);
    for (auto& [id, tex] : app.thumbnails)
        destroyTexture(tex);
    app.embedder.reset();
    background.reset();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return exit_code;
}
