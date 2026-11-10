#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "catalog.hpp"

class ChildProcess;
struct RunLog;

// Runs one demo at a time as a child process tree and collects its console output.
// Nothing here blocks: output is read on a detached thread and the process is polled.
class DemoRunner
{
public:
    DemoRunner();
    ~DemoRunner(); // stops whatever is still running
    DemoRunner(const DemoRunner&) = delete;
    DemoRunner& operator=(const DemoRunner&) = delete;

    // Starts `demo` with extra arguments (from its options) and environment variables (its
    // options, the chosen OpenCL device). Returns false, with the reason in the log, on failure.
    bool start(const Demo& demo, const std::vector<std::string>& extra_args,
               const std::vector<std::pair<std::string, std::string>>& extra_env);

    // Asks the demo to quit; it is killed if still running after a grace period.
    // Calling it again while stopping kills it immediately.
    void stop();

    // Call once per frame: notices exits and enforces the grace period.
    void poll();

    bool running() const noexcept { return child_ != nullptr; }
    // Whether process `pid` belongs to the running demo (used to find its window).
    bool ownsProcess(long pid) const;
    bool stopping() const noexcept { return stop_requested_ && running(); }
    const std::string& demoId() const noexcept { return demo_id_; }
    bool hasExited() const noexcept { return exited_; }
    int exitCode() const noexcept { return exit_code_; }
    double secondsRunning() const;

    // Changes whenever the log does, so the console only copies it when needed.
    uint64_t logVersion() const;
    void copyLog(std::string& out) const;
    std::string log() const;
    void clearLog();

private:
    void finish();

    std::unique_ptr<ChildProcess> child_;
    std::shared_ptr<RunLog> log_;
    std::string demo_id_;
    bool main_exited_ = false;
    bool stop_requested_ = false;
    bool exited_ = false;
    int exit_code_ = 0;
    std::optional<std::chrono::steady_clock::time_point> kill_at_;
    std::optional<std::chrono::steady_clock::time_point> tree_ended_at_;
    std::chrono::steady_clock::time_point started_{};
    std::chrono::steady_clock::time_point finished_{};
};
