#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// A setting the user can change before launching a demo, declared in its manifest:
//   {"id": "bodies", "label": "Bodies", "type": "int", "min": 100, "max": 20000,
//    "default": 1000, "args": ["--bodies", "{value}"], "env": "NBODY_COUNT"}
// Its value reaches the demo through `args` ("{value}" is replaced; for a bool the
// arguments are passed only when it is on) and/or the environment variable `env`.
struct DemoOption
{
    enum class Type { Int, Float, Bool, Choice };

    std::string id;
    std::string label;
    std::string help;
    Type type = Type::Int;
    double min = 0.0;
    double max = 1.0;
    double default_number = 0.0;
    bool default_flag = false;
    std::vector<std::string> choices;
    std::string default_choice;
    std::vector<std::string> args;
    std::string env;
};

// One entry of the gallery, read from demos/<id>/demo.json.
// The program it describes is an ordinary executable: the gallery only launches it.
struct Demo
{
    std::string id;           // folder name
    std::string title;
    std::string category;
    std::string author;
    std::string summary;      // one line, shown on the card
    std::string description;  // shown on the demo's page
    std::vector<std::string> how_it_works; // paragraphs explaining the technique
    std::vector<std::string> highlights;   // "what to look for" bullet points
    std::vector<std::string> tags;         // short labels, e.g. "OpenCL", "Instancing"

    // How the demo shows itself: in the gallery window (its window is embedded), in a
    // window of its own, or not at all (console output only, shown in the gallery).
    enum class Window { Embed, Separate, None };
    Window window = Window::Embed;

    std::filesystem::path executable;        // resolved, absolute
    std::filesystem::path working_directory; // resolved, absolute
    std::vector<std::string> args;
    std::vector<std::pair<std::string, std::string>> env;
    std::vector<DemoOption> options;

    std::vector<std::string> requires_;      // "opencl", "opengl"
    std::vector<std::pair<std::string, std::string>> controls; // key(s), action
    std::vector<std::string> platforms;      // empty = all; else "windows", "linux", "macos"

    std::filesystem::path thumbnail;         // may be empty

    std::string unavailable_reason;          // empty = can be launched
    bool available() const noexcept { return unavailable_reason.empty(); }
};

struct Catalog
{
    std::vector<Demo> demos;
    std::vector<std::string> errors; // manifests that could not be read
};

// Reads every <demos_dir>/<id>/demo.json. An "executable" containing a path separator is
// relative to `app_dir` (the gallery's folder); a bare name ("python3") is looked up in
// `app_dir`, then on the PATH.
Catalog loadCatalog(const std::filesystem::path& demos_dir, const std::filesystem::path& app_dir);

const char* currentPlatform() noexcept;
