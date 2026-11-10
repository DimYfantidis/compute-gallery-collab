// The stage: what the gallery shows while a demo runs. A slim bar on top (back, status,
// stop), and below it the demo's own window (embedded), a note that it runs in its own
// window, or its console output for programs without a window.

#include <algorithm>
#include <cmath>
#include <format>

#include "app.hpp" // first: glad must come before GLFW

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "icons.hpp"
#include "logo.hpp"
#include "widgets.hpp"

namespace
{
// How long to wait for a demo's window before saying it runs elsewhere.
constexpr double EMBED_PATIENCE = 12.0;

// Back was pressed while the demo ran: leave the stage once it has stopped.
bool g_leave_when_stopped = false;

const Demo* stageDemo(const App& app)
{
    const int i = findDemo(app, app.last_run_id);
    return i < 0 ? nullptr : &app.catalog.demos[i];
}

ImVec4 vec(ImU32 c)
{
    return ImGui::ColorConvertU32ToFloat4(c);
}
} // namespace

void updateStage(App& app)
{
    app.runner.poll();
    app.builder.poll();

    if (!app.build_reported && app.builder.hasExited())
    {
        app.build_reported = true;
        if (app.builder.exitCode() == 0)
        {
            reloadCatalog(app);
            toast(app, ICON_CIRCLE_CHECK, "Build finished", palette::success);
        }
        else
            toast(app, ICON_CIRCLE_X, "Build failed: see the output", palette::danger);
    }

    const Demo* d = stageDemo(app);
    static bool was_running = false;
    if (app.runner.running())
    {
        was_running = true;
        if (d && embedsInGallery(app, *d) && app.page == Page::Stage)
            app.embedder->tryEmbed([&](long pid) { return app.runner.ownsProcess(pid); });

#if defined(__APPLE__)
        // macOS cannot hold another program's window: step aside while it runs.
        if (d && d->window == Demo::Window::Embed && app.settings.embed_demos && !app.handed_off &&
            app.now - app.launched_at > 1.0)
        {
            glfwIconifyWindow(app.window);
            app.handed_off = true;
        }
#endif
        return;
    }

    if (!was_running)
        return;
    // The demo just ended (Esc, its window closed, Stop, or it finished).
    was_running = false;
    if (app.embedder)
        app.embedder->reset();
    if (app.handed_off)
    {
        glfwRestoreWindow(app.window);
        glfwFocusWindow(app.window);
        app.handed_off = false;
    }
    if (!d)
        return;
    const int code = app.runner.exitCode();
    toast(app, code == 0 ? ICON_CIRCLE_CHECK : ICON_CIRCLE_ALERT,
          code == 0 ? std::format("{} closed", d->title) : std::format("{} exited with code {}", d->title, code),
          code == 0 ? palette::success : palette::warning);
    // Programs with a window return to their page; console output stays up to be read.
    if (app.page == Page::Stage && (d->window != Demo::Window::None || g_leave_when_stopped))
        openPage(app, Page::Demo);
    g_leave_when_stopped = false;
}

void drawStage(App& app, ImVec2 origin, ImVec2 size)
{
    const float em = ImGui::GetFontSize();
    const Demo* d = stageDemo(app);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool running = app.runner.running();

    // --- top bar --------------------------------------------------------------
    const float bar_h = em * 3.6f;
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + bar_h), withAlpha(palette::sidebar, 0.95f));
    dl->AddLine(ImVec2(origin.x, origin.y + bar_h), ImVec2(origin.x + size.x, origin.y + bar_h), palette::border);
    const float y = origin.y + (bar_h - ImGui::GetFrameHeight()) * 0.5f;

    ImGui::SetCursorScreenPos(ImVec2(origin.x + em, y));
    bool go_back = ui::ghostButton(ICON_ARROW_LEFT "  Gallery");
    ImGui::SetItemTooltip(running ? "Stop the demo and go back (Esc)" : "Back to the demo's page (Esc)");
    ImGui::SameLine(0, em * 1.2f);
    const ImVec2 mark_c(ImGui::GetCursorScreenPos().x + em * 0.9f, origin.y + bar_h * 0.5f);
    drawLogoMark(dl, mark_c, em * 1.8f, running ? static_cast<float>(app.now) : 0.0f);
    ImGui::SetCursorScreenPos(ImVec2(mark_c.x + em * 1.4f, origin.y + (bar_h - ImGui::GetTextLineHeight() * 2.1f) * 0.5f));
    ImGui::BeginGroup();
    ImGui::PushFont(app.fonts.medium);
    ImGui::TextUnformatted(d ? d->title.c_str() : "Demo");
    ImGui::PopFont();
    ImGui::PushFont(app.fonts.small);
    if (running)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, vec(app.runner.stopping() ? palette::warning : palette::success));
        ImGui::Text(app.runner.stopping() ? ICON_CLOCK "  Stopping..." : ICON_PLAY "  Running  ·  %.0f s", app.runner.secondsRunning());
        ImGui::PopStyleColor();
    }
    else
        ImGui::TextDisabled(ICON_CIRCLE_STOP "  Finished  ·  exit code %d  ·  %.1f s", app.runner.exitCode(), app.runner.secondsRunning());
    ImGui::PopFont();
    ImGui::EndGroup();

    // Right side: console toggle, then Stop or Run again.
    const float stop_w = em * 7.8f;
    ImGui::SetCursorScreenPos(ImVec2(origin.x + size.x - em - stop_w - ImGui::GetFrameHeight() - em * 0.6f, y));
    if (ui::iconButton(ICON_TERMINAL, app.stage_console ? "Hide the console" : "Show the console output"))
        app.stage_console = !app.stage_console;
    ImGui::SameLine(0, em * 0.6f);
    if (running)
    {
        if (ui::dangerButton(app.runner.stopping() ? ICON_CIRCLE_STOP "  Force stop" : ICON_SQUARE "  Stop", ImVec2(stop_w, 0)))
            app.runner.stop();
    }
    else if (d && d->available())
    {
        if (ui::primaryButton(ICON_ROTATE_CCW "  Run again", ImVec2(stop_w, ImGui::GetFrameHeight())))
            launch(app, *d);
    }

    // --- body -----------------------------------------------------------------
    const ImVec2 body0(origin.x, origin.y + bar_h + 1);
    const ImVec2 body1(origin.x + size.x, origin.y + size.y);
    const bool embedded = app.embedder && app.embedder->embedded();
    const bool console_only = d && d->window == Demo::Window::None;
    const bool show_console = console_only || app.stage_console;
    const float console_h = console_only ? body1.y - body0.y : std::max(em * 9.0f, (body1.y - body0.y) * 0.32f);
    const ImVec2 view1(body1.x, show_console ? body1.y - console_h : body1.y);

    if (embedded)
    {
        // The demo's window covers this area; the gallery draws nothing there.
        // Round the edges, not the size, so the demo meets the window's edges exactly.
        const int x0 = static_cast<int>(std::lround(body0.x)), y0 = static_cast<int>(std::lround(body0.y));
        const int x1 = static_cast<int>(std::lround(view1.x)), y1 = static_cast<int>(std::lround(view1.y));
        app.embedder->setArea(x0, y0, x1 - x0, y1 - y0);
    }
    else if (!console_only && view1.y - body0.y > em * 6.0f)
    {
        const bool waiting = running && d && embedsInGallery(app, *d) && app.now - app.launched_at < EMBED_PATIENCE;
        const ImVec2 c((body0.x + view1.x) * 0.5f, (body0.y + view1.y) * 0.5f - em * 2.0f);
        drawLogoMark(dl, c, em * 6.0f, static_cast<float>(app.now) * (running ? 1.5f : 0.0f));
        if (running)
            ui::keepAnimating();
        const char* title = !running  ? "The demo has finished"
                            : waiting ? "Starting..."
                                      : "Running in its own window";
        const char* detail = !running ? "Run it again, or go back to the gallery."
                             : waiting
                                 ? "Its window will appear here."
                                 : "Press Esc in its window or close it to come back, or use Stop above. On this system, or with "
                                   "this setting, demos cannot be shown inside the gallery.";
        const ImVec2 ts = app.fonts.heading->CalcTextSizeA(fontPx(app.fonts.heading), FLT_MAX, 0, title);
        dl->AddText(app.fonts.heading, fontPx(app.fonts.heading), ImVec2(c.x - ts.x * 0.5f, c.y + em * 4.0f), palette::text, title);
        const float wrap = std::min(em * 30.0f, size.x - em * 4.0f);
        const ImVec2 ds = app.fonts.body->CalcTextSizeA(fontPx(app.fonts.body), FLT_MAX, wrap, detail);
        dl->AddText(app.fonts.body, fontPx(app.fonts.body), ImVec2(c.x - ds.x * 0.5f, c.y + em * 5.8f), palette::muted, detail,
                    nullptr, wrap);
    }

    if (show_console)
    {
        ImGui::SetCursorScreenPos(ImVec2(body0.x + em, view1.y + em * 0.6f));
        drawLog(app, app.log_view, app.runner, "stage-log", ImVec2(size.x - em * 2.0f, body1.y - view1.y - em * 1.2f));
    }

    // Esc reaches the gallery only when the demo's window does not have the keyboard.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        go_back = true;
    if (go_back)
    {
        if (running)
        {
            app.runner.stop(); // the stage closes when it has ended
            g_leave_when_stopped = true;
        }
        else
            openPage(app, Page::Demo);
    }
}
