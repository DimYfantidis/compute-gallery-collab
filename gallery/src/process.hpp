#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct LaunchSpec
{
    std::filesystem::path executable;
    std::vector<std::string> args;
    std::filesystem::path working_directory;
    std::vector<std::pair<std::string, std::string>> env; // added to / overriding the gallery's
};

// Read end of the pipe carrying a child's stdout and stderr. Owned by whoever reads it, so
// reading can outlive the ChildProcess object.
class OutputPipe
{
public:
    ~OutputPipe();
    // Blocks until output is available; returns 0 once every writer has closed the pipe.
    size_t read(char* buffer, size_t size);

private:
    friend class ChildProcess;
    OutputPipe() = default;
#if defined(_WIN32)
    void* handle_ = nullptr;
#else
    int fd_ = -1;
#endif
};

// A demo and everything it starts: a process group on POSIX, a Job Object on Windows, so
// helper processes are stopped together with the demo. Standard input is /dev/null (NUL).
class ChildProcess
{
public:
    // Returns nullptr and sets `error` if the program could not be started.
    static std::unique_ptr<ChildProcess> start(const LaunchSpec& spec, std::string& error);
    ~ChildProcess(); // kills whatever is still running

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    std::shared_ptr<OutputPipe> output() const { return output_; }

    // Non-blocking: true once the demo's main process has exited, with its exit code.
    bool poll(int& exit_code);

    // Whether any process of the tree (main or helpers) is still running.
    bool treeAlive() const;

    // Whether process `pid` belongs to this tree (e.g. to find the demo's window).
    bool owns(long pid) const;

    // Asks the tree to quit: SIGTERM on POSIX, WM_CLOSE to its windows on Windows.
    // Returns false if there was nothing to ask (e.g. a windowless program on Windows).
    bool requestStop();

    // Ends the whole tree immediately.
    void kill();

private:
    ChildProcess() = default;
    std::shared_ptr<OutputPipe> output_;
    bool reaped_ = false;
    int exit_code_ = 0;
#if defined(_WIN32)
    void* process_ = nullptr;
    void* job_ = nullptr;
#else
    int pid_ = -1;
#endif
};
