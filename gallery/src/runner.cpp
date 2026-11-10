#include "runner.hpp"

#include <atomic>
#include <format>
#include <mutex>
#include <string_view>
#include <thread>

#include "process.hpp"

using Clock = std::chrono::steady_clock;

// How long a demo gets to quit after being asked, before it is killed.
constexpr auto GRACE_PERIOD = std::chrono::seconds(3);

// Output of one run. Shared with its reader thread, which may outlive the run.
struct RunLog
{
    void append(std::string_view chunk)
    {
        std::lock_guard lock(mutex);
        text += chunk;
        // Keep the console responsive for very chatty programs: drop the oldest whole lines.
        constexpr size_t MAX_LOG = 512 * 1024;
        if (text.size() > MAX_LOG)
        {
            const size_t cut = text.find('\n', text.size() - MAX_LOG);
            text.erase(0, cut == std::string::npos ? text.size() - MAX_LOG : cut + 1);
        }
        version = ++counter;
    }

    mutable std::mutex mutex;
    std::string text;
    uint64_t version = ++counter;
    std::atomic<bool> closed{false}; // the reader saw end-of-output
    static inline std::atomic<uint64_t> counter{0};
};

DemoRunner::DemoRunner() : log_(std::make_shared<RunLog>()) {}

DemoRunner::~DemoRunner() = default; // ~ChildProcess kills the tree; readers end with it

bool DemoRunner::start(const Demo& demo, const std::vector<std::string>& extra_args,
                       const std::vector<std::pair<std::string, std::string>>& extra_env)
{
    if (child_)
        return false;

    log_ = std::make_shared<RunLog>();
    demo_id_ = demo.id;
    main_exited_ = stop_requested_ = exited_ = false;
    exit_code_ = 0;
    kill_at_.reset();
    tree_ended_at_.reset();

    LaunchSpec spec{demo.executable, demo.args, demo.working_directory, demo.env};
    spec.args.insert(spec.args.end(), extra_args.begin(), extra_args.end());
    spec.env.insert(spec.env.end(), extra_env.begin(), extra_env.end());

    std::string command = spec.executable.string();
    for (const std::string& a : spec.args)
        command += " " + a;
    log_->append(std::format("$ {}\n  (in {})\n\n", command, spec.working_directory.string()));

    std::string error;
    child_ = ChildProcess::start(spec, error);
    if (!child_)
    {
        log_->append(error + "\n");
        exited_ = true;
        exit_code_ = -1;
        return false;
    }
    started_ = Clock::now();

    // Detached: it ends when the last process holding the pipe exits, and only touches the
    // pipe and log it shares ownership of, so it never needs to be waited for.
    std::thread([pipe = child_->output(), log = log_] {
        char buffer[4096];
        size_t n;
        while ((n = pipe->read(buffer, sizeof(buffer))) > 0)
            log->append(std::string_view(buffer, n));
        log->closed = true;
    }).detach();
    return true;
}

void DemoRunner::stop()
{
    if (!child_)
        return;
    if (stop_requested_ || !child_->requestStop())
        child_->kill(); // second press, or nothing to ask politely
    else
        kill_at_ = Clock::now() + GRACE_PERIOD;
    stop_requested_ = true;
}

void DemoRunner::poll()
{
    if (!child_)
        return;
    const auto now = Clock::now();

    int code = 0;
    if (!main_exited_ && child_->poll(code))
    {
        main_exited_ = true;
        exit_code_ = code;
        finished_ = now;
        // Helpers the demo started do not outlive it.
        if (!kill_at_ && child_->treeAlive())
        {
            if (child_->requestStop())
                kill_at_ = now + GRACE_PERIOD;
            else
                child_->kill();
        }
    }
    if (kill_at_ && now >= *kill_at_)
    {
        child_->kill();
        kill_at_.reset();
    }
    // Done once every process is gone and its last output has been read. The output ends
    // within moments of that, unless something escaped the tree and still holds the pipe.
    if (main_exited_ && !child_->treeAlive())
    {
        if (!tree_ended_at_)
            tree_ended_at_ = now;
        if (log_->closed || now - *tree_ended_at_ > std::chrono::seconds(1))
            finish();
    }
}

void DemoRunner::finish()
{
    child_.reset();
    exited_ = true;
    log_->append(stop_requested_ ? std::format("\n[stopped from the gallery after {:.1f} s]\n", secondsRunning())
                                 : std::format("\n[exited with code {} after {:.1f} s]\n", exit_code_, secondsRunning()));
}

bool DemoRunner::ownsProcess(long pid) const
{
    return child_ && child_->owns(pid);
}

double DemoRunner::secondsRunning() const
{
    const auto end = main_exited_ || exited_ ? finished_ : Clock::now();
    return std::chrono::duration<double>(end - started_).count();
}

uint64_t DemoRunner::logVersion() const
{
    std::lock_guard lock(log_->mutex);
    return log_->version;
}

void DemoRunner::copyLog(std::string& out) const
{
    std::lock_guard lock(log_->mutex);
    out = log_->text;
}

std::string DemoRunner::log() const
{
    std::string text;
    copyLog(text);
    return text;
}

void DemoRunner::clearLog()
{
    std::lock_guard lock(log_->mutex);
    log_->text.clear();
    log_->version = ++RunLog::counter;
}
