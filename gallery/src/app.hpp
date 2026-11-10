#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "catalog.hpp"
#include "embed.hpp"
#include "options.hpp"
#include "runner.hpp"
#include "settings.hpp"
#include "system_info.hpp"
#include "textures.hpp"
#include "theme.hpp"

struct GLFWwindow;

enum class Page
{
    Discover, // hero + all demos
    Demo,     // one demo: explanations, settings, launch
    Stage,    // a running demo (embedded, in its own window, or its console)
    System,   // diagnostics, like run.sh doctor
    Settings,
    Help,
};

struct Toast
{
    std::string icon;
    std::string text;
    ImU32 color;
    double born;
};

struct App
{
    std::filesystem::path app_dir;
    std::filesystem::path demos_dir;
    GLFWwindow* window = nullptr;

    Catalog catalog;
    SystemInfo sys;
    DemoRunner runner;
    Fonts fonts;
    std::map<std::string, Texture> thumbnails;
    std::map<std::string, std::map<std::string, OptionValue>> options; // demo id -> option id -> value
    Settings settings; // as loaded; values of demos not opened yet stay here untouched
    bool use_settings = false;

    // Navigation
    Page page = Page::Discover;
    double page_since = 0.0; // for the fade-in
    int selected = 0;        // demo shown on the Demo page and in the hero
    std::string category = "All";
    char search[128] = "";

    // Running demo
    std::string last_run_id;
    std::unique_ptr<WindowEmbedder> embedder;
    double launched_at = 0.0;
    bool stage_console = false; // console drawer open on the stage
    bool handed_off = false;    // gallery minimised while the demo runs in its own window

    // Rebuild (developer builds only)
    DemoRunner builder;
    bool build_reported = true;

    int cl_choice = -1; // -1 = let the demo pick (first GPU); else index into sys.cl_devices

    // Console: a copy of the log and where its lines start, refreshed only when it changes.
    struct LogView
    {
        std::string text;
        std::vector<size_t> lines;
        uint64_t version = ~uint64_t(0);
    } log_view, build_view;

    // Environment, detected at startup
    std::string environment;          // "Docker (windows-wsl)", "Native", ...
    std::filesystem::path build_dir;  // set when running from a CMake build folder
    bool software_rendering = false;

    std::vector<Toast> toasts;
    float pending_scale = 0.0f; // set when the window moves to a screen with another DPI
    double now = 0.0;
};

// --- demos -----------------------------------------------------------------
int findDemo(const App& app, const std::string& id);
const Demo* selectedDemo(const App& app);
void applyHardwareAvailability(App& app);
void reloadCatalog(App& app);
const Texture* thumbnail(App& app, const Demo& d);
OptionValue& optionValue(App& app, const Demo& d, const DemoOption& o);
// Starts the demo and opens the stage. Returns false (with a toast) if it could not start.
bool launch(App& app, const Demo& d);
// Whether this demo will be shown inside the gallery window.
bool embedsInGallery(const App& app, const Demo& d);

// --- navigation, feedback --------------------------------------------------
void openPage(App& app, Page page);
void toast(App& app, const char* icon, std::string text, ImU32 color);

// --- settings, environment ---------------------------------------------------
void storeSettings(App& app, GLFWwindow* window);
void detectEnvironment(App& app);
void startRebuild(App& app);

// --- drawing (pages.cpp, stage.cpp) ----------------------------------------
void drawGallery(App& app);
void drawStage(App& app, ImVec2 origin, ImVec2 size);
void updateStage(App& app); // once per frame, before drawing: exits, embedding, handoff

// The console view shared by the stage and the developer tools.
void drawLog(App& app, App::LogView& view, const DemoRunner& source, const char* id, ImVec2 size);
