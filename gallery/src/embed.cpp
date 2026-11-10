#include "embed.hpp"

#include <algorithm>

#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#elif defined(__linux__)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

struct Area
{
    int x = 0, y = 0, w = 0, h = 0;
    bool operator==(const Area&) const = default;
};

#if defined(_WIN32)

// ----------------------------------------------------------------------------
// Windows: the demo's window becomes a borderless window kept exactly over the stage area
// and just above the gallery. Deliberately neither a child nor an owned window: both make
// Windows share the two programs' input state, and GLFW (in either program) then reads the
// other program's window data and crashes.
// ----------------------------------------------------------------------------

struct WindowEmbedder::Impl
{
    HWND host = nullptr;
    HWND child = nullptr;
    Area area;    // in the gallery window's client coordinates
    Area applied; // in screen coordinates
    bool hidden = false;

    void apply()
    {
        if (!child)
            return;
        // Hidden with the gallery when it is minimised, back when it is restored.
        if (IsIconic(host) != hidden)
        {
            hidden = !hidden;
            ShowWindow(child, hidden ? SW_HIDE : SW_SHOWNOACTIVATE);
        }
        if (hidden)
            return;
        // Whenever the gallery comes to the front, the demo goes right back on top of it.
        if (GetForegroundWindow() == host)
            SetWindowPos(host, child, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        POINT origin{area.x, area.y};
        ClientToScreen(host, &origin); // follows the gallery when it moves
        const Area screen{origin.x, origin.y, area.w, area.h};
        if (!(screen == applied))
        {
            SetWindowPos(child, HWND_TOP, screen.x, screen.y, screen.w, screen.h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            applied = screen;
        }
    }
};

WindowEmbedder::WindowEmbedder(GLFWwindow* host) : impl_(std::make_unique<Impl>())
{
    impl_->host = glfwGetWin32Window(host);
}

WindowEmbedder::~WindowEmbedder() = default;

bool WindowEmbedder::supported() const
{
    return impl_->host != nullptr;
}

bool WindowEmbedder::tryEmbed(const std::function<bool(long)>& owns)
{
    if (embedded())
        return true;

    struct Search
    {
        const std::function<bool(long)>* owns;
        HWND host;
        HWND best = nullptr;
        long best_area = 0;
    } search{&owns, impl_->host};

    EnumWindows(
        [](HWND hwnd, LPARAM param) -> BOOL {
            auto* s = reinterpret_cast<Search*>(param);
            if (hwnd == s->host || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER))
                return TRUE; // not a main window (dialogs and tooltips are owned)
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (!(*s->owns)(static_cast<long>(pid)))
                return TRUE;
            RECT r{};
            GetWindowRect(hwnd, &r);
            const long area = (r.right - r.left) * (r.bottom - r.top);
            if (area > s->best_area)
                s->best = hwnd, s->best_area = area;
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    if (!search.best)
        return false;

    HWND child = search.best;
    // Frameless, and a tool window: no title bar and no taskbar or Alt+Tab entry of its own.
    LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CHILD);
    style |= WS_POPUP;
    SetWindowLongPtrW(child, GWL_STYLE, style);
    LONG_PTR ex = GetWindowLongPtrW(child, GWL_EXSTYLE);
    ex &= ~static_cast<LONG_PTR>(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME);
    ex |= WS_EX_TOOLWINDOW;
    SetWindowLongPtrW(child, GWL_EXSTYLE, ex);
    SetWindowPos(child, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    impl_->child = child;
    impl_->applied = {};
    impl_->hidden = false;
    impl_->apply();
    focus();
    return true;
}

bool WindowEmbedder::embedded() const
{
    if (impl_->child && !IsWindow(impl_->child))
        impl_->child = nullptr; // the demo replaced or closed its window
    return impl_->child != nullptr;
}

void WindowEmbedder::setArea(int x, int y, int width, int height)
{
    impl_->area = {x, y, width, height};
    impl_->apply();
}

void WindowEmbedder::focus()
{
    // The gallery is in the foreground (its Launch was just pressed), so it may pass that on.
    if (embedded())
        SetForegroundWindow(impl_->child);
}

void WindowEmbedder::reset()
{
    impl_->child = nullptr;
}

#elif defined(__linux__)

// ----------------------------------------------------------------------------
// X11: the demo's window is reparented into the gallery's
// ----------------------------------------------------------------------------

// Other programs' windows can vanish at any moment (SDL replaces its window while starting),
// and Xlib's default reaction to a request on a vanished window is to end the program. While
// one of these is alive, such errors are ignored; the previous handler is restored after.
class XErrorTrap
{
public:
    explicit XErrorTrap(Display* dpy) : dpy_(dpy)
    {
        XSync(dpy_, False);
        previous_ = XSetErrorHandler([](Display*, XErrorEvent*) { return 0; });
    }
    ~XErrorTrap()
    {
        XSync(dpy_, False);
        XSetErrorHandler(previous_);
    }

private:
    Display* dpy_;
    XErrorHandler previous_;
};

struct WindowEmbedder::Impl
{
    Display* dpy = nullptr;
    Window host = 0;
    Window child = 0;
    Atom net_wm_pid = 0;
    Area area;
    Area applied;
    bool focused = false;
    int attempts = 0;

    long windowPid(Window w)
    {
        Atom type;
        int format;
        unsigned long count, after;
        unsigned char* data = nullptr;
        long pid = -1;
        if (XGetWindowProperty(dpy, w, net_wm_pid, 0, 1, False, XA_CARDINAL, &type, &format, &count, &after, &data) ==
                Success &&
            data && count == 1)
            pid = static_cast<long>(*reinterpret_cast<unsigned long*>(data));
        if (data)
            XFree(data);
        return pid;
    }

    bool viewable(Window w)
    {
        XWindowAttributes a;
        return XGetWindowAttributes(dpy, w, &a) && a.map_state == IsViewable;
    }

    // Window managers put each application window in a frame, so look one level down too.
    void search(Window w, int depth, const std::function<bool(long)>& owns, Window& best, long& best_area)
    {
        Window root_ret, parent_ret, *children = nullptr;
        unsigned int n = 0;
        if (!XQueryTree(dpy, w, &root_ret, &parent_ret, &children, &n))
            return;
        for (unsigned int i = 0; i < n; ++i)
        {
            const Window c = children[i];
            if (c == host)
                continue;
            const long pid = windowPid(c);
            if (pid > 0 && owns(pid))
            {
                XWindowAttributes a;
                if (XGetWindowAttributes(dpy, c, &a) && a.map_state == IsViewable && !a.override_redirect &&
                    static_cast<long>(a.width) * a.height > best_area)
                    best = c, best_area = static_cast<long>(a.width) * a.height;
            }
            else if (depth < 2)
                search(c, depth + 1, owns, best, best_area);
        }
        if (children)
            XFree(children);
    }

    Window parentOf(Window w)
    {
        Window root_ret, parent_ret = 0, *children = nullptr;
        unsigned int n = 0;
        if (XQueryTree(dpy, w, &root_ret, &parent_ret, &children, &n) && children)
            XFree(children);
        return parent_ret;
    }

    void reparent()
    {
        // Withdraw it, as a program does to tell the window manager it no longer wants the
        // window managed (a plain unmap reads as minimising). The window manager then moves
        // it back to the root window, asynchronously: wait for that, or it takes it back.
        const Window root = DefaultRootWindow(dpy);
        XWithdrawWindow(dpy, child, DefaultScreen(dpy));
        XSync(dpy, False);
        for (int i = 0; i < 50 && parentOf(child) != root && parentOf(child) != host; ++i)
            usleep(10000);
        // A child of the gallery window is no longer the window manager's business.
        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        XChangeWindowAttributes(dpy, child, CWOverrideRedirect, &attrs);
        XReparentWindow(dpy, child, host, area.x, area.y);
        XMoveResizeWindow(dpy, child, area.x, area.y, std::max(area.w, 1), std::max(area.h, 1));
        XMapRaised(dpy, child);
        XSync(dpy, False);
        applied = area;
        focused = false;
    }
};


WindowEmbedder::WindowEmbedder(GLFWwindow* host) : impl_(std::make_unique<Impl>())
{
    if (glfwGetPlatform() != GLFW_PLATFORM_X11)
        return;
    impl_->dpy = glfwGetX11Display();
    impl_->host = glfwGetX11Window(host);
    impl_->net_wm_pid = XInternAtom(impl_->dpy, "_NET_WM_PID", False);
}

WindowEmbedder::~WindowEmbedder() = default;

bool WindowEmbedder::supported() const
{
    return impl_->dpy != nullptr;
}

bool WindowEmbedder::tryEmbed(const std::function<bool(long)>& owns)
{
    if (!supported())
        return false;
    if (embedded())
        return true;

    XErrorTrap trap(impl_->dpy);
    Window best = 0;
    long best_area = 0;
    impl_->search(DefaultRootWindow(impl_->dpy), 0, owns, best, best_area);
    if (!best)
        return false;

    impl_->child = best;
    impl_->attempts = 0;
    impl_->reparent();
    return true;
}

bool WindowEmbedder::embedded() const
{
    // Toolkits may replace their window right after showing it (SDL does when it sets up
    // OpenGL): if ours is gone, the next tryEmbed() looks for the new one.
    if (!impl_->child)
        return false;
    XErrorTrap trap(impl_->dpy);
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(impl_->dpy, impl_->child, &attrs))
        impl_->child = 0;
    return impl_->child != 0;
}

void WindowEmbedder::setArea(int x, int y, int width, int height)
{
    Impl& m = *impl_;
    m.area = {x, y, width, height};
    if (!m.child)
        return;
    XErrorTrap trap(m.dpy);
    // A window manager may still take the window back right after we reparented it.
    if (m.parentOf(m.child) != m.host)
    {
        if (++m.attempts <= 60)
            m.reparent();
        return;
    }
    if (!(m.area == m.applied))
    {
        XMoveResizeWindow(m.dpy, m.child, x, y, std::max(width, 1), std::max(height, 1));
        m.applied = m.area;
        XFlush(m.dpy);
    }
    if (!m.focused && m.viewable(m.child))
        focus();
}

void WindowEmbedder::focus()
{
    Impl& m = *impl_;
    if (!m.child)
        return;
    XErrorTrap trap(m.dpy);
    if (m.viewable(m.child))
    {
        XSetInputFocus(m.dpy, m.child, RevertToParent, CurrentTime);
        XFlush(m.dpy);
        m.focused = true;
    }
}

void WindowEmbedder::reset()
{
    impl_->child = 0;
}

#else

// ----------------------------------------------------------------------------
// macOS and others: windows of other processes cannot be embedded
// ----------------------------------------------------------------------------

struct WindowEmbedder::Impl
{
};

WindowEmbedder::WindowEmbedder(GLFWwindow*) : impl_(std::make_unique<Impl>()) {}
WindowEmbedder::~WindowEmbedder() = default;
bool WindowEmbedder::supported() const { return false; }
bool WindowEmbedder::tryEmbed(const std::function<bool(long)>&) { return false; }
bool WindowEmbedder::embedded() const { return false; }
void WindowEmbedder::setArea(int, int, int, int) {}
void WindowEmbedder::focus() {}
void WindowEmbedder::reset() {}

#endif
