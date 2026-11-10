// Tests for the parts of the gallery that work without a window: launching and stopping
// demos (process trees, standard input, graceful stop), reading manifests and turning demo
// options into arguments. Run with ctest, or directly: gallery_tests
//
// Demos here are shell one-liners: /bin/sh -c on POSIX, cmd.exe /c on Windows.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "catalog.hpp"
#include "options.hpp"
#include "runner.hpp"

namespace fs = std::filesystem;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

static int failures = 0;

#define CHECK(cond)                                                                       \
    do                                                                                    \
    {                                                                                     \
        if (!(cond))                                                                      \
        {                                                                                 \
            std::fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
            ++failures;                                                                   \
        }                                                                                 \
    } while (0)

static Demo shellDemo(const std::string& command)
{
    Demo d;
    d.id = "test";
#if defined(_WIN32)
    const char* comspec = std::getenv("ComSpec");
    d.executable = comspec ? comspec : "C:\\Windows\\System32\\cmd.exe";
    d.args = {"/d", "/c", command};
#else
    d.executable = "/bin/sh";
    d.args = {"-c", command};
#endif
    d.working_directory = fs::current_path();
    return d;
}

// Polls the way the UI does until the run is over or `timeout` passes. Returns how long it
// took; `longest_poll` receives the slowest single poll(), which must never block.
static double runUntilDone(DemoRunner& runner, std::chrono::milliseconds timeout, double* longest_poll = nullptr)
{
    const auto start = Clock::now();
    double longest = 0.0;
    while (runner.running() && Clock::now() - start < timeout)
    {
        const auto t0 = Clock::now();
        runner.poll();
        longest = std::max(longest, std::chrono::duration<double, std::milli>(Clock::now() - t0).count());
        std::this_thread::sleep_for(10ms);
    }
    if (longest_poll)
        *longest_poll = longest;
    return std::chrono::duration<double>(Clock::now() - start).count();
}

static bool logHas(const DemoRunner& runner, const std::string& text)
{
    return runner.log().find(text) != std::string::npos;
}

static void test(const char* name, void (*body)())
{
    const int before = failures;
    body();
    std::printf("%s %s\n", failures == before ? "ok  " : "FAIL", name);
}

int main()
{
    test("output and exit code are captured", [] {
        DemoRunner r;
        CHECK(r.start(shellDemo("echo hello && exit 3"), {}, {}));
        runUntilDone(r, 10s);
        CHECK(!r.running());
        CHECK(r.exitCode() == 3);
        CHECK(logHas(r, "hello"));
        CHECK(logHas(r, "[exited with code 3"));
    });

    test("standard input is empty, so reading it does not hang", [] {
        DemoRunner r;
#if defined(_WIN32)
        CHECK(r.start(shellDemo("set /p line= & echo after-input"), {}, {}));
#else
        CHECK(r.start(shellDemo("read line; echo after-input"), {}, {}));
#endif
        runUntilDone(r, 10s);
        CHECK(!r.running());
        CHECK(logHas(r, "after-input"));
    });

    test("stop ends the whole tree, even a child holding the output, without blocking", [] {
        DemoRunner r;
#if defined(_WIN32)
        CHECK(r.start(shellDemo("start /b ping -n 300 127.0.0.1 >nul & ping -n 300 127.0.0.1 >nul"), {}, {}));
#else
        CHECK(r.start(shellDemo("sleep 300 & sleep 300"), {}, {}));
#endif
        std::this_thread::sleep_for(500ms);
        r.stop();
        double longest = 0.0;
        runUntilDone(r, 10s, &longest);
        CHECK(!r.running());
        CHECK(longest < 100.0);
        CHECK(logHas(r, "[stopped from the gallery"));
    });

    test("helpers a demo leaves behind are ended when it exits", [] {
        DemoRunner r;
#if defined(_WIN32)
        CHECK(r.start(shellDemo("start /b ping -n 300 127.0.0.1 >nul & echo started"), {}, {}));
#else
        CHECK(r.start(shellDemo("sleep 300 & echo started"), {}, {}));
#endif
        double longest = 0.0;
        runUntilDone(r, 10s, &longest);
        CHECK(!r.running());
        CHECK(r.exitCode() == 0);
        CHECK(logHas(r, "started"));
        CHECK(longest < 100.0);
    });

#if !defined(_WIN32)
    test("stop asks first (SIGTERM), so a demo can clean up", [] {
        DemoRunner r;
        CHECK(r.start(shellDemo("trap 'echo cleanup; exit 0' TERM; while :; do sleep 0.1; done"), {}, {}));
        std::this_thread::sleep_for(500ms);
        r.stop();
        const double seconds = runUntilDone(r, 10s);
        CHECK(!r.running());
        CHECK(seconds < 2.5); // well before the 3 s grace period
        CHECK(logHas(r, "cleanup"));
    });

    test("a demo ignoring the request is killed after the grace period", [] {
        DemoRunner r;
        CHECK(r.start(shellDemo("trap '' TERM; while :; do sleep 0.1; done"), {}, {}));
        std::this_thread::sleep_for(500ms);
        r.stop();
        const double seconds = runUntilDone(r, 10s);
        CHECK(!r.running());
        CHECK(seconds > 2.5 && seconds < 6.0);
    });
#endif

    // Needs a GPU, so only when asked: CG_TEST_WINDOWED_DEMO=<path to bodies(.exe)>.
    if (const char* windowed = std::getenv("CG_TEST_WINDOWED_DEMO"))
    {
        static fs::path program;
        program = windowed;
        test("stop lets a windowed demo quit by itself", [] {
            DemoRunner r;
            Demo d;
            d.id = "windowed";
            d.executable = fs::absolute(program);
            d.working_directory = d.executable.parent_path();
            CHECK(r.start(d, {}, {}));
            std::this_thread::sleep_for(4s); // let the window open
            r.stop();
            const double seconds = runUntilDone(r, 10s);
            CHECK(!r.running());
            CHECK(seconds < 2.5);      // before the grace period: it was not killed...
            CHECK(r.exitCode() == 0);  // ...but closed normally
        });
    }

    test("a missing program fails to start with a reason", [] {
        DemoRunner r;
        Demo d = shellDemo("");
        d.executable = fs::current_path() / "no-such-program";
        CHECK(!r.start(d, {}, {}));
        CHECK(!r.running());
        CHECK(logHas(r, "Could not start"));
    });

    test("extra arguments and environment reach the demo", [] {
        DemoRunner r;
#if defined(_WIN32)
        Demo d = shellDemo("echo env:%CG_TEST_VALUE%");
#else
        Demo d = shellDemo("echo \"env:$CG_TEST_VALUE args:$1 $2\"");
        d.args.push_back("sh"); // $0
#endif
        CHECK(r.start(d, {"--bodies", "1000"}, {{"CG_TEST_VALUE", "42"}}));
        runUntilDone(r, 10s);
        CHECK(logHas(r, "env:42"));
        CHECK(logHas(r, "--bodies 1000"));
    });

    test("manifests: options, PATH lookup and errors", [] {
        const fs::path root = fs::temp_directory_path() / "compute-gallery-tests";
        fs::remove_all(root);
        fs::create_directories(root / "demos" / "opts");
        fs::create_directories(root / "demos" / "broken");
#if defined(_WIN32)
        const char* shell = "cmd";
#else
        const char* shell = "sh";
#endif
        std::ofstream(root / "demos" / "opts" / "demo.json") << R"({
            "title": "Options", "executable": ")" << shell << R"(",
            "options": [
                {"id": "n", "type": "int", "min": 10, "max": 100000, "default": 1000, "args": ["--bodies", "{value}"]},
                {"id": "dt", "type": "float", "min": 0, "max": 1, "default": 0.25, "args": ["--dt={value}"]},
                {"id": "vsync", "type": "bool", "default": false, "args": ["--vsync"], "env": "VSYNC"},
                {"id": "mode", "type": "choice", "choices": ["gpu", "cpu"], "default": "cpu", "env": "MODE"}
            ]
        })";
        std::ofstream(root / "demos" / "broken" / "demo.json")
            << R"({"executable": "x", "options": [{"id": "a", "type": "colour"}]})";

        const Catalog c = loadCatalog(root / "demos", root);
        CHECK(c.demos.size() == 1);
        CHECK(c.errors.size() == 1);
        if (c.demos.size() != 1)
            return;
        const Demo& d = c.demos[0];
        CHECK(d.available()); // found on the PATH
        CHECK(d.executable.is_absolute());
        CHECK(d.working_directory == root / "demos" / "opts");
        CHECK(d.options.size() == 4);

        std::vector<std::string> args;
        std::vector<std::pair<std::string, std::string>> env;
        appendOptionParameters(d, {}, args, env);
        CHECK((args == std::vector<std::string>{"--bodies", "1000", "--dt=0.25"}));
        CHECK((env == std::vector<std::pair<std::string, std::string>>{{"VSYNC", "0"}, {"MODE", "cpu"}}));

        std::map<std::string, OptionValue> values{{"n", {5000}}, {"vsync", {0, true}}, {"mode", {0, false, "gpu"}}};
        args.clear(), env.clear();
        appendOptionParameters(d, values, args, env);
        CHECK((args == std::vector<std::string>{"--bodies", "5000", "--dt=0.25", "--vsync"}));
        CHECK((env == std::vector<std::pair<std::string, std::string>>{{"VSYNC", "1"}, {"MODE", "gpu"}}));
        fs::remove_all(root);
    });

    std::printf("%s\n", failures == 0 ? "all tests passed" : "some tests FAILED");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
