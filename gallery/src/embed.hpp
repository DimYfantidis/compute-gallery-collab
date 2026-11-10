#pragma once

#include <functional>
#include <memory>

struct GLFWwindow;

// Puts a demo's window inside the gallery's window, so a demo runs in the same window and
// the gallery is back when it exits. Windows: the demo window becomes a borderless child
// window. X11: it is reparented into the gallery window. Elsewhere (macOS, Wayland) the
// platform does not allow it and supported() is false.
class WindowEmbedder
{
public:
    explicit WindowEmbedder(GLFWwindow* host);
    ~WindowEmbedder();
    WindowEmbedder(const WindowEmbedder&) = delete;
    WindowEmbedder& operator=(const WindowEmbedder&) = delete;

    bool supported() const;

    // Looks for a visible top-level window of a process for which `owns(pid)` is true and
    // embeds it. Call once per frame until it returns true.
    bool tryEmbed(const std::function<bool(long)>& owns);

    bool embedded() const;

    // The part of the gallery window (window coordinates, as ImGui uses) the demo covers.
    void setArea(int x, int y, int width, int height);

    // Gives the demo keyboard focus, so its keys (Esc) reach it.
    void focus();

    // Forgets the window; call when the demo has ended.
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
