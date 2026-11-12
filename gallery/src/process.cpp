#include "process.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <format>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <set>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#if defined(_WIN32)

// ----------------------------------------------------------------------------
// Windows: CreateProcessW inside a Job Object
// ----------------------------------------------------------------------------

static std::wstring widen(const std::string& s)
{
    if (s.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

static std::string windowsMessage(DWORD error)
{
    char* text = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                   error, 0, reinterpret_cast<char*>(&text), 0, nullptr);
    std::string message = text ? text : std::format("error {}", error);
    LocalFree(text);
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == '.'))
        message.pop_back();
    return message;
}

// Quotes one argument so that the child's C runtime splits the command line back into the
// same argv (backslashes are only special in front of a quote).
static void appendQuoted(std::wstring& cmd, const std::wstring& arg)
{
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
    {
        cmd += arg;
        return;
    }
    cmd += L'"';
    for (auto it = arg.begin();; ++it)
    {
        size_t backslashes = 0;
        while (it != arg.end() && *it == L'\\')
            ++it, ++backslashes;
        if (it == arg.end())
        {
            cmd.append(backslashes * 2, L'\\');
            break;
        }
        if (*it == L'"')
            cmd.append(backslashes * 2 + 1, L'\\');
        else
            cmd.append(backslashes, L'\\');
        cmd += *it;
    }
    cmd += L'"';
}

// The gallery's environment with `extra` set on top, as a CreateProcessW environment block.
static std::wstring environmentBlock(const std::vector<std::pair<std::string, std::string>>& extra)
{
    std::vector<std::wstring> entries;
    if (wchar_t* block = GetEnvironmentStringsW())
    {
        for (const wchar_t* p = block; *p; p += wcslen(p) + 1)
            entries.emplace_back(p);
        FreeEnvironmentStringsW(block);
    }
    for (const auto& [key, value] : extra)
    {
        const std::wstring wkey = widen(key);
        std::erase_if(entries, [&](const std::wstring& e) {
            const size_t eq = e.find(L'=', 1); // entries such as "=C:=C:\" start with '='
            return CompareStringOrdinal(e.c_str(), static_cast<int>(eq), wkey.c_str(), static_cast<int>(wkey.size()),
                                        TRUE) == CSTR_EQUAL;
        });
        entries.push_back(wkey + L"=" + widen(value));
    }
    std::wstring block;
    for (const std::wstring& e : entries)
        block += e + L'\0';
    block += L'\0';
    return block;
}

OutputPipe::~OutputPipe()
{
    if (handle_)
        CloseHandle(handle_);
}

size_t OutputPipe::read(char* buffer, size_t size)
{
    for (;;)
    {
        DWORD got = 0;
        if (!ReadFile(handle_, buffer, static_cast<DWORD>(size), &got, nullptr))
            return 0; // ERROR_BROKEN_PIPE: every writer is gone
        if (got > 0)
            return got;
    }
}

std::unique_ptr<ChildProcess> ChildProcess::start(const LaunchSpec& spec, std::string& error)
{
    SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE read_end = nullptr, write_end = nullptr;
    if (!CreatePipe(&read_end, &write_end, &inheritable, 0))
    {
        error = "Could not create a pipe: " + windowsMessage(GetLastError());
        return nullptr;
    }
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, OPEN_EXISTING, 0,
                             nullptr);

    // Pass exactly these two handles to the child, nothing else the gallery has open.
    SIZE_T attr_size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
    std::vector<char> attr_storage(attr_size);
    auto* attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_storage.data());
    InitializeProcThreadAttributeList(attrs, 1, 0, &attr_size);
    HANDLE inherit[2] = {nul, write_end};
    UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof(inherit), nullptr, nullptr);

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = nul;
    si.StartupInfo.hStdOutput = write_end;
    si.StartupInfo.hStdError = write_end;
    si.lpAttributeList = attrs;

    const std::wstring exe = spec.executable.wstring();
    std::wstring cmd;
    appendQuoted(cmd, exe);
    for (const std::string& arg : spec.args)
    {
        cmd += L' ';
        appendQuoted(cmd, widen(arg));
    }
    std::wstring env = environmentBlock(spec.env);
    const std::wstring cwd = spec.working_directory.wstring();

    // Suspended until it is in the job, so nothing it starts can escape.
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE,
                                   CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW |
                                       EXTENDED_STARTUPINFO_PRESENT,
                                   env.data(), cwd.empty() ? nullptr : cwd.c_str(), &si.StartupInfo, &pi);
    const DWORD create_error = GetLastError();
    DeleteProcThreadAttributeList(attrs);
    CloseHandle(write_end);
    if (nul != INVALID_HANDLE_VALUE)
        CloseHandle(nul);
    if (!ok)
    {
        CloseHandle(read_end);
        error = std::format("Could not start {}: {}", spec.executable.string(), windowsMessage(create_error));
        return nullptr;
    }

    // Killing the job ends the demo and everything it started; so does the gallery exiting.
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
        !AssignProcessToJobObject(job, pi.hProcess))
    {
        if (job)
            CloseHandle(job);
        job = nullptr; // still works, but only the main process can be stopped
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    std::unique_ptr<ChildProcess> child(new ChildProcess());
    child->process_ = pi.hProcess;
    child->job_ = job;
    child->output_.reset(new OutputPipe());
    child->output_->handle_ = read_end;
    return child;
}

ChildProcess::~ChildProcess()
{
    if (treeAlive())
        kill();
    if (job_)
        CloseHandle(job_);
    if (process_)
        CloseHandle(process_);
}

bool ChildProcess::poll(int& exit_code)
{
    if (!reaped_ && WaitForSingleObject(process_, 0) == WAIT_OBJECT_0)
    {
        DWORD code = 0;
        GetExitCodeProcess(process_, &code);
        exit_code_ = static_cast<int>(code);
        reaped_ = true;
    }
    exit_code = exit_code_;
    return reaped_;
}

bool ChildProcess::treeAlive() const
{
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
    if (job_ && QueryInformationJobObject(job_, JobObjectBasicAccountingInformation, &info, sizeof(info), nullptr))
        return info.ActiveProcesses > 0;
    return WaitForSingleObject(process_, 0) == WAIT_TIMEOUT;
}

bool ChildProcess::owns(long pid) const
{
    if (static_cast<DWORD>(pid) == GetProcessId(process_))
        return true;
    BOOL in_job = FALSE;
    if (!job_)
        return false;
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!p)
        return false;
    IsProcessInJob(p, job_, &in_job);
    CloseHandle(p);
    return in_job != FALSE;
}

bool ChildProcess::requestStop()
{
    std::set<DWORD> pids{GetProcessId(process_)};
    if (job_)
    {
        struct
        {
            JOBOBJECT_BASIC_PROCESS_ID_LIST list;
            ULONG_PTR more[255];
        } ids{};
        if (QueryInformationJobObject(job_, JobObjectBasicProcessIdList, &ids, sizeof(ids), nullptr))
            for (DWORD i = 0; i < ids.list.NumberOfProcessIdsInList; ++i)
                pids.insert(static_cast<DWORD>(ids.list.ProcessIdList[i]));
    }

    // Closing their windows is how GLFW and SDL programs expect to be asked to quit.
    struct Search
    {
        std::set<DWORD>* pids;
        int posted = 0;
    } search{&pids};
    // Top-level windows (a demo shown inside the gallery is one too: a popup over its stage),
    // and child windows, in case a demo's window lives inside another program's window.
    static constexpr WNDENUMPROC ask = [](HWND hwnd, LPARAM param) -> BOOL {
        auto* s = reinterpret_cast<Search*>(param);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (s->pids->count(pid) && IsWindowVisible(hwnd) && PostMessageW(hwnd, WM_CLOSE, 0, 0))
            ++s->posted;
        return TRUE;
    };
    EnumWindows(
        [](HWND hwnd, LPARAM param) -> BOOL {
            ask(hwnd, param);
            EnumChildWindows(hwnd, ask, param);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.posted > 0;
}

void ChildProcess::kill()
{
    if (job_)
        TerminateJobObject(job_, 1);
    else
        TerminateProcess(process_, 1);
}

#else

// ----------------------------------------------------------------------------
// POSIX: fork + execve into a new process group
// ----------------------------------------------------------------------------

static void setCloseOnExec(int fd)
{
    const int flags = fcntl(fd, F_GETFD);
    if (flags >= 0)
        fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

OutputPipe::~OutputPipe()
{
    if (fd_ >= 0)
        ::close(fd_);
}

size_t OutputPipe::read(char* buffer, size_t size)
{
    for (;;)
    {
        const ssize_t n = ::read(fd_, buffer, size);
        if (n >= 0)
            return static_cast<size_t>(n);
        if (errno != EINTR)
            return 0;
    }
}

std::unique_ptr<ChildProcess> ChildProcess::start(const LaunchSpec& spec, std::string& error)
{
    // Everything the child uses is prepared here: between fork() and execve() only
    // async-signal-safe calls are allowed, since the gallery has other threads.
    const std::string exe = spec.executable.string();
    const std::string cwd = spec.working_directory.string();

    std::vector<std::string> arg_storage{exe};
    arg_storage.insert(arg_storage.end(), spec.args.begin(), spec.args.end());
    std::vector<char*> argv;
    for (std::string& a : arg_storage)
        argv.push_back(a.data());
    argv.push_back(nullptr);

    std::vector<std::string> env_storage;
    for (char** e = environ; e && *e; ++e)
    {
        const std::string entry = *e;
        const std::string key = entry.substr(0, entry.find('='));
        if (std::none_of(spec.env.begin(), spec.env.end(), [&](const auto& kv) { return kv.first == key; }))
            env_storage.push_back(entry);
    }
    for (const auto& [key, value] : spec.env)
        env_storage.push_back(key + "=" + value);
    std::vector<char*> envp;
    for (std::string& e : env_storage)
        envp.push_back(e.data());
    envp.push_back(nullptr);

    // `output` carries the child's stdout/stderr; `status` reports a failed chdir/exec.
    int output[2], status[2];
    if (pipe(output) != 0)
    {
        error = std::format("Could not create a pipe: {}", std::strerror(errno));
        return nullptr;
    }
    if (pipe(status) != 0)
    {
        error = std::format("Could not create a pipe: {}", std::strerror(errno));
        ::close(output[0]), ::close(output[1]);
        return nullptr;
    }
    for (int fd : {output[0], output[1], status[0], status[1]})
        setCloseOnExec(fd);
    const int devnull = ::open("/dev/null", O_RDONLY | O_CLOEXEC);

    const pid_t pid = fork();
    if (pid == 0)
    {
        setpgid(0, 0); // own process group, so the whole tree can be signalled
        int failure[2] = {0, 0}; // {stage, errno}: 1 = chdir, 2 = exec
        if (!cwd.empty() && chdir(cwd.c_str()) != 0)
            failure[0] = 1, failure[1] = errno;
        else
        {
            dup2(devnull, STDIN_FILENO);
            dup2(output[1], STDOUT_FILENO);
            dup2(output[1], STDERR_FILENO);
            execve(exe.c_str(), argv.data(), envp.data());
            failure[0] = 2, failure[1] = errno;
        }
        [[maybe_unused]] const ssize_t ignored = write(status[1], failure, sizeof(failure));
        _exit(127);
    }

    ::close(output[1]);
    ::close(status[1]);
    if (devnull >= 0)
        ::close(devnull);
    if (pid < 0)
    {
        error = std::format("Could not start {}: {}", exe, std::strerror(errno));
        ::close(output[0]), ::close(status[0]);
        return nullptr;
    }
    setpgid(pid, pid); // also from here, so the group exists before it may be signalled

    // The status pipe closes on a successful execve; data in it means the start failed.
    int failure[2] = {0, 0};
    ssize_t got;
    do
        got = ::read(status[0], failure, sizeof(failure));
    while (got < 0 && errno == EINTR);
    ::close(status[0]);
    if (got == sizeof(failure))
    {
        waitpid(pid, nullptr, 0);
        ::close(output[0]);
        error = failure[0] == 1
                    ? std::format("Could not enter {}: {}", cwd, std::strerror(failure[1]))
                    : std::format("Could not start {}: {}", exe, std::strerror(failure[1]));
        return nullptr;
    }

    std::unique_ptr<ChildProcess> child(new ChildProcess());
    child->pid_ = pid;
    child->output_.reset(new OutputPipe());
    child->output_->fd_ = output[0];
    return child;
}

ChildProcess::~ChildProcess()
{
    if (!reaped_)
    {
        ::kill(-pid_, SIGKILL);
        waitpid(pid_, nullptr, 0);
    }
    else if (treeAlive())
        ::kill(-pid_, SIGKILL);
}

bool ChildProcess::poll(int& exit_code)
{
    int status = 0;
    if (!reaped_ && waitpid(pid_, &status, WNOHANG) == pid_)
    {
        reaped_ = true;
        exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1;
    }
    exit_code = exit_code_;
    return reaped_;
}

bool ChildProcess::treeAlive() const
{
    return ::kill(-pid_, 0) == 0 || errno == EPERM;
}

bool ChildProcess::owns(long pid) const
{
    return getpgid(static_cast<pid_t>(pid)) == pid_; // its process group is the demo's
}

bool ChildProcess::requestStop()
{
    return ::kill(-pid_, SIGTERM) == 0;
}

void ChildProcess::kill()
{
    ::kill(-pid_, SIGKILL);
}

#endif
