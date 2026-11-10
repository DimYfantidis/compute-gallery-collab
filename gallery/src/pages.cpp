// The gallery's pages: the sidebar, Discover, a demo's page, System, Settings and Help.
// The running-demo stage is in stage.cpp.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <set>

#include "app.hpp" // first: glad must come before GLFW

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "icons.hpp"
#include "logo.hpp"
#include "widgets.hpp"

#ifndef CG_VERSION
#define CG_VERSION "dev"
#endif

namespace
{
ImVec4 vec(ImU32 c)
{
    return ImGui::ColorConvertU32ToFloat4(c);
}

ImTextureID texId(const Texture& t)
{
    return static_cast<ImTextureID>(static_cast<uintptr_t>(t.id));
}

bool containsIgnoreCase(const std::string& haystack, const char* needle)
{
    if (!*needle)
        return true;
    auto lower = [](std::string s) {
        for (char& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    return lower(haystack).find(lower(needle)) != std::string::npos;
}

bool visible(const App& app, const Demo& d)
{
    if (app.category != "All" && d.category != app.category)
        return false;
    std::string text = d.title + " " + d.summary + " " + d.category;
    for (const std::string& t : d.tags)
        text += " " + t;
    return containsIgnoreCase(text, app.search);
}

// Picture of a demo: its thumbnail, or a gradient in its category colour with its title.
void demoArt(App& app, ImDrawList* dl, const Demo& d, ImVec2 p0, ImVec2 p1, float rounding, ImDrawFlags corners,
             float zoom = 1.0f)
{
    if (const Texture* t = thumbnail(app, d))
    {
        ui::imageCover(dl, texId(*t), t->width, t->height, p0, p1, rounding, corners, zoom);
        return;
    }
    const ImU32 c = categoryColor(d.category);
    ui::gradient(dl, p0, p1, withAlpha(c, 0.35f), withAlpha(palette::accent_2, 0.12f), false, rounding, corners);
    ImFont* font = app.fonts.title;
    const float base = fontPx(font);
    const float w = font->CalcTextSizeA(base, FLT_MAX, 0, d.title.c_str()).x;
    const float size = std::min(base, base * (p1.x - p0.x) * 0.8f / std::max(w, 1.0f));
    const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0, d.title.c_str());
    dl->AddText(font, size, ImVec2((p0.x + p1.x - ts.x) * 0.5f, (p0.y + p1.y - ts.y) * 0.5f), withAlpha(IM_COL32_WHITE, 0.55f),
                d.title.c_str());
}

// Rows of keycaps and what they do; the text is centred on the keys.
void keysTable(const App& app, const char* id, const std::vector<std::pair<std::string, std::string>>& rows)
{
    const float em = ImGui::GetFontSize();
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_None))
        return;
    ImGui::TableSetupColumn("keys", ImGuiTableColumnFlags_WidthFixed, em * 11.0f);
    ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthStretch);
    const float key_h = fontPx(app.fonts.small) * 1.75f;
    for (const auto& [keys, action] : rows)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ui::keys(app.fonts, keys);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (key_h - ImGui::GetTextLineHeight()) * 0.5f);
        ui::paragraph(action);
    }
    ImGui::EndTable();
}

const char* requirementLabel(const std::string& r)
{
    if (r == "opencl")
        return "OpenCL";
    if (r == "opengl")
        return "OpenGL";
    return r.c_str();
}

void pageHeader(const App& app, const char* title, const char* subtitle)
{
    ImGui::PushFont(app.fonts.title);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    if (subtitle)
        ui::paragraph(subtitle);
    ImGui::Dummy(ImVec2(0, ImGui::GetFontSize() * 0.6f));
}

// ----------------------------------------------------------------------------
// Sidebar
// ----------------------------------------------------------------------------

void drawSidebar(App& app, float width, float height)
{
    const float em = ImGui::GetFontSize();
    const float pad = em * 1.2f;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    const bool alive = app.settings.animations && !app.software_rendering;
    drawLogo(dl, ImVec2(origin.x + pad, origin.y + pad * 1.2f), em * 2.6f, app.fonts, alive ? static_cast<float>(app.now) : 1.4f);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + pad * 0.7f, origin.y + pad * 1.2f + em * 4.4f));

    struct Item
    {
        Page page;
        const char* icon;
        const char* label;
    };
    const Item items[] = {
        {Page::Discover, ICON_LAYOUT_GRID, "Discover"},
        {Page::System, ICON_GAUGE, "System"},
        {Page::Settings, ICON_SLIDERS_HORIZONTAL, "Settings"},
        {Page::Help, ICON_BOOK_OPEN, "Help"},
    };
    const float item_w = width - pad * 1.4f;
    for (const Item& item : items)
    {
        const bool active = app.page == item.page || (item.page == Page::Discover && app.page == Page::Demo);
        ImGui::PushID(item.label);
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1(p0.x + item_w, p0.y + em * 2.45f);
        if (ImGui::InvisibleButton("##nav", ImVec2(item_w, em * 2.45f)))
            openPage(app, item.page);
        const float hover = ui::animate(ImGui::GetID("hover"), ImGui::IsItemHovered() ? 1.0f : 0.0f);
        const float on = ui::animate(ImGui::GetID("active"), active ? 1.0f : 0.0f);
        ImGui::PopID();

        dl->AddRectFilled(p0, p1, mix(IM_COL32(255, 255, 255, static_cast<int>(12 * hover)), withAlpha(palette::accent, 0.14f), on),
                          em * 0.6f);
        if (on > 0.01f)
            dl->AddRectFilled(ImVec2(p0.x, p0.y + em * 0.55f), ImVec2(p0.x + 3.0f, p1.y - em * 0.55f), withAlpha(palette::accent, on),
                              2.0f);
        const ImU32 icon_col = mix(palette::muted, palette::accent, on);
        const ImU32 text_col = mix(mix(palette::muted, palette::text, hover), IM_COL32_WHITE, on);
        const float y = p0.y + (em * 2.45f - em) * 0.5f;
        dl->AddText(app.fonts.medium, fontPx(app.fonts.medium), ImVec2(p0.x + em * 0.95f, y), icon_col, item.icon);
        dl->AddText(app.fonts.medium, fontPx(app.fonts.medium), ImVec2(p0.x + em * 2.6f, y), text_col, item.label);
        ImGui::Dummy(ImVec2(0, em * 0.15f));
    }

    // System summary at the bottom.
    const float card_h = em * 6.4f;
    const ImVec2 c0(origin.x + pad * 0.7f, origin.y + height - card_h - pad);
    const ImVec2 c1(c0.x + item_w, c0.y + card_h);
    dl->AddRectFilled(c0, c1, palette::surface, em * 0.7f);
    dl->AddRect(c0, c1, palette::border, em * 0.7f);
    const float sp = fontPx(app.fonts.small);
    const float tw = item_w - em * 1.6f;
    auto line = [&](float y, const char* icon, const std::string& text, ImU32 col) {
        dl->AddText(app.fonts.small, sp, ImVec2(c0.x + em * 0.8f, y), palette::accent, icon);
        ImGui::PushFont(app.fonts.small);
        const std::string t = ui::ellipsize(text, tw - em * 1.3f);
        ImGui::PopFont();
        dl->AddText(app.fonts.small, sp, ImVec2(c0.x + em * 2.0f, y), col, t.c_str());
    };
    const std::string renderer = app.sys.gl_renderer.substr(0, app.sys.gl_renderer.find('/'));
    std::string cl = app.sys.cl_devices.empty() ? "No OpenCL device"
                     : app.cl_choice >= 0     ? app.sys.cl_devices[app.cl_choice].name
                                              : std::format("OpenCL: {} device{}", app.sys.cl_devices.size(),
                                                            app.sys.cl_devices.size() == 1 ? "" : "s");
    line(c0.y + em * 0.8f, ICON_MONITOR, renderer, palette::text);
    line(c0.y + em * 2.15f, ICON_MICROCHIP, cl, palette::text);
    line(c0.y + em * 3.5f, ICON_CONTAINER, app.environment, palette::muted);
    line(c0.y + em * 4.85f, ICON_INFO, std::string("Version ") + CG_VERSION, palette::faint);
}

// ----------------------------------------------------------------------------
// Discover
// ----------------------------------------------------------------------------

void drawWelcome(App& app)
{
    if (app.settings.welcome_dismissed)
        return;
    ui::beginCard("welcome");
    ui::sectionTitle(app.fonts, ICON_SPARKLES, "Welcome to Compute Gallery");
    ui::paragraph("Each card is a small program that shows what GPUs and parallel computing can do. Open one to read "
                  "how it works and adjust its settings, then press Launch: it runs right here in this window, and "
                  "Esc brings you back. System shows what this computer offers the demos.");
    ImGui::Dummy(ImVec2(0, 4));
    if (ui::ghostButton(ICON_CHECK "  Got it"))
        app.settings.welcome_dismissed = true;
    ui::endCard();
    ImGui::Dummy(ImVec2(0, ImGui::GetFontSize() * 0.4f));
}

void drawHero(App& app, const Demo& d, float width)
{
    const float em = ImGui::GetFontSize();
    const float height = std::clamp(width * 0.36f, em * 16.0f, em * 23.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + width, p0.y + height);
    const float rounding = em * 1.1f;

    ui::shadow(dl, p0, p1, rounding);
    const Texture* art = thumbnail(app, d);
    if (d.window == Demo::Window::None && art)
    {
        // Console output as art reads best at its own size, on the right.
        dl->AddRectFilled(p0, p1, IM_COL32(10, 13, 18, 255), rounding);
        const float h = std::min(height - em * 2.0f, static_cast<float>(art->height));
        const float w = h * art->width / art->height;
        const ImVec2 a0(p1.x - w - em * 1.5f, p0.y + (height - h) * 0.5f);
        dl->AddImageRounded(texId(*art), a0, ImVec2(a0.x + w, a0.y + h), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, em * 0.6f);
    }
    else
        demoArt(app, dl, d, p0, p1, rounding, ImDrawFlags_RoundCornersAll, 1.02f);
    ui::gradient(dl, p0, ImVec2(p0.x + width * 0.75f, p1.y), withAlpha(palette::background, 0.94f),
                 withAlpha(palette::background, 0.0f), false, rounding, ImDrawFlags_RoundCornersLeft);
    ui::gradient(dl, ImVec2(p0.x, p1.y - height * 0.35f), p1, withAlpha(palette::background, 0.0f),
                 withAlpha(palette::background, 0.75f), true, rounding, ImDrawFlags_RoundCornersBottom);
    dl->AddRect(p0, p1, palette::border, rounding);

    const float pad = em * 2.2f;
    const float text_w = width * 0.48f;
    ImGui::SetCursorScreenPos(ImVec2(p0.x + pad, p0.y + pad));
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, vec(categoryColor(d.category)));
    ImGui::PushFont(app.fonts.medium);
    ImGui::Text(ICON_SPARKLES "  Featured  " ICON_CHEVRON_RIGHT "  %s", d.category.c_str());
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::PushFont(app.fonts.display);
    ImGui::PushTextWrapPos(p0.x + pad + text_w - ImGui::GetWindowPos().x);
    ImGui::TextWrapped("%s", d.title.c_str());
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, vec(IM_COL32(205, 212, 224, 255)));
    ImGui::TextWrapped("%s", d.summary.c_str());
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, em * 0.5f));

    if (d.available())
    {
        if (ui::primaryButton(ICON_PLAY "  Launch", ImVec2(em * 8.5f, em * 2.7f)))
            launch(app, d);
    }
    else
    {
        ui::primaryButton(ICON_PLAY "  Launch", ImVec2(em * 8.5f, em * 2.7f), false);
    }
    ImGui::SameLine(0, em * 0.7f);
    if (ui::ghostButton("Details  " ICON_CHEVRON_RIGHT, ImVec2(0, em * 2.7f)))
        openPage(app, Page::Demo);
    if (!d.available())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, vec(IM_COL32(255, 160, 150, 255)));
        ImGui::Text(ICON_CIRCLE_ALERT "  %s", d.unavailable_reason.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::EndGroup();
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y));
    ImGui::Dummy(ImVec2(width, em * 1.4f));
}

void drawToolbar(App& app, float width)
{
    const float em = ImGui::GetFontSize();
    std::vector<std::string> categories{"All"};
    std::set<std::string> seen;
    for (const Demo& d : app.catalog.demos)
        if (seen.insert(d.category).second)
            categories.push_back(d.category);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (const std::string& c : categories)
    {
        const bool active = app.category == c;
        ImGui::PushID(c.c_str());
        const ImVec2 ts = ImGui::CalcTextSize(c.c_str());
        const ImVec2 size(ts.x + em * 1.6f, em * 2.2f);
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##cat", size))
            app.category = c;
        const float hover = ui::animate(ImGui::GetID("h"), ImGui::IsItemHovered() ? 1.0f : 0.0f);
        const float on = ui::animate(ImGui::GetID("a"), active ? 1.0f : 0.0f);
        ImGui::PopID();
        const ImVec2 p1(p0.x + size.x, p0.y + size.y);
        dl->AddRectFilled(p0, p1, mix(IM_COL32(255, 255, 255, static_cast<int>(10 + 12 * hover)), IM_COL32(255, 255, 255, 235), on),
                          size.y * 0.5f);
        dl->AddText(ImVec2(p0.x + em * 0.8f, p0.y + (size.y - ts.y) * 0.5f), mix(palette::text, palette::on_accent, on), c.c_str());
        ImGui::SameLine(0, em * 0.45f);
    }

    const float search_w = em * 15.0f;
    const float tools_w = search_w + em * 0.5f + ImGui::GetFrameHeight();
    if (ImGui::GetContentRegionAvail().x < tools_w)
        ImGui::NewLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - tools_w));
    ImGui::SetNextItemWidth(search_w);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, em * 1.1f);
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F))
        ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("##search", ICON_SEARCH "   Search demos", app.search, sizeof(app.search));
    ImGui::PopStyleVar();
    ImGui::SameLine(0, em * 0.5f);
    if (ui::iconButton(ICON_REFRESH_CW, "Read the demo folders again (F5)"))
    {
        reloadCatalog(app);
        toast(app, ICON_REFRESH_CW, std::format("{} demos found", app.catalog.demos.size()), palette::accent);
    }
    (void)width;
    ImGui::Dummy(ImVec2(0, em * 0.4f));
}

void drawCard(App& app, int index, ImVec2 size)
{
    const Demo& d = app.catalog.demos[index];
    const float em = ImGui::GetFontSize();
    const float rounding = em * 0.9f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::PushID(index);
    const ImVec2 base = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton("##card", size);
    const bool hovered = ImGui::IsItemHovered();
    const float hover = ui::animate(ImGui::GetID("hover"), hovered ? 1.0f : 0.0f);
    ImGui::PopID();

    const float lift = -4.0f * hover;
    const ImVec2 p0(base.x, base.y + lift);
    const ImVec2 p1(base.x + size.x, base.y + size.y + lift);
    const float image_h = size.x * 9.0f / 16.0f;
    const ImVec2 i1(p1.x, p0.y + image_h);

    ui::shadow(dl, p0, p1, rounding, 0.6f + 0.8f * hover);
    dl->AddRectFilled(p0, p1, palette::surface_solid, rounding);
    demoArt(app, dl, d, p0, i1, rounding, ImDrawFlags_RoundCornersTop, 1.0f + 0.05f * hover);
    if (!d.available())
        dl->AddRectFilled(p0, i1, IM_COL32(8, 10, 15, 160), rounding, ImDrawFlags_RoundCornersTop);
    ui::gradient(dl, ImVec2(p0.x, i1.y - image_h * 0.4f), i1, withAlpha(palette::surface_solid, 0.0f),
                 withAlpha(palette::surface_solid, 0.85f), true);

    // Category, and unavailable / running state, over the picture.
    float bx = p0.x + em * 0.7f;
    bx += ui::chip(dl, app.fonts, ImVec2(bx, p0.y + em * 0.7f), d.category.c_str(), IM_COL32(8, 10, 15, 190),
                   categoryColor(d.category)) + em * 0.35f;
    if (app.runner.running() && app.runner.demoId() == d.id)
        ui::chip(dl, app.fonts, ImVec2(bx, p0.y + em * 0.7f), ICON_PLAY " Running", palette::success, palette::on_accent);
    else if (!d.available())
        ui::chip(dl, app.fonts, ImVec2(bx, p0.y + em * 0.7f), ICON_CIRCLE_ALERT " Unavailable", withAlpha(palette::danger, 0.9f),
                 IM_COL32_WHITE);

    // Play button that appears on hover.
    const ImVec2 play_c((p0.x + p1.x) * 0.5f, p0.y + image_h * 0.5f);
    const float play_r = em * 1.6f;
    if (d.available() && hover > 0.01f)
    {
        dl->AddCircleFilled(play_c, play_r + 6.0f * hover, withAlpha(palette::accent, 0.25f * hover), 40);
        dl->AddCircleFilled(play_c, play_r, withAlpha(palette::accent, hover), 40);
        const ImVec2 ts = app.fonts.heading->CalcTextSizeA(fontPx(app.fonts.heading), FLT_MAX, 0, ICON_PLAY);
        dl->AddText(app.fonts.heading, fontPx(app.fonts.heading), ImVec2(play_c.x - ts.x * 0.42f, play_c.y - ts.y * 0.5f),
                    withAlpha(palette::on_accent, hover), ICON_PLAY);
    }

    // Title, summary, requirements.
    const float pad = em * 1.0f;
    ImVec2 tp(p0.x + pad, i1.y + em * 0.15f);
    dl->AddText(app.fonts.heading, fontPx(app.fonts.heading), tp, palette::text, d.title.c_str());
    tp.y += fontPx(app.fonts.heading) + em * 0.35f;
    const ImVec4 clip(tp.x, tp.y, p1.x - pad, p1.y - em * 2.4f);
    dl->AddText(app.fonts.body, fontPx(app.fonts.body), tp, palette::muted, d.summary.c_str(), nullptr, size.x - 2 * pad, &clip);
    float rx = p0.x + pad;
    for (const std::string& r : d.requires_)
        rx += ui::chip(dl, app.fonts, ImVec2(rx, p1.y - em * 2.0f), requirementLabel(r), IM_COL32(255, 255, 255, 14), palette::muted) +
              em * 0.35f;
    dl->AddRect(p0, p1, mix(palette::border, withAlpha(palette::accent, 0.7f), hover), rounding, 0, 1.0f + hover);

    if (clicked)
    {
        app.selected = index;
        const ImVec2 m = ImGui::GetIO().MousePos;
        const bool on_play = (m.x - play_c.x) * (m.x - play_c.x) + (m.y - play_c.y) * (m.y - play_c.y) <= play_r * play_r;
        if (on_play && d.available())
            launch(app, d);
        else
            openPage(app, Page::Demo);
    }
}

void drawDiscover(App& app)
{
    const float width = ImGui::GetContentRegionAvail().x;
    const float em = ImGui::GetFontSize();
    drawWelcome(app);
    if (const Demo* d = selectedDemo(app))
        drawHero(app, *d, width);
    drawToolbar(app, width);

    const float gap = em * 1.3f;
    const int columns = std::max(1, static_cast<int>((width + gap) / (em * 18.0f + gap)));
    const float card_w = std::floor((width - gap * (columns - 1)) / columns);
    const ImVec2 card(card_w, card_w * 9.0f / 16.0f + em * 7.2f);
    int shown = 0;
    for (int i = 0; i < static_cast<int>(app.catalog.demos.size()); ++i)
    {
        if (!visible(app, app.catalog.demos[i]))
            continue;
        if (shown % columns != 0)
            ImGui::SameLine(0, gap);
        else if (shown > 0)
            ImGui::Dummy(ImVec2(0, gap * 0.35f));
        drawCard(app, i, card);
        ++shown;
    }
    if (shown == 0)
        ui::paragraph(app.catalog.demos.empty()
                          ? std::format("No demos found. Each demo is a folder in {} with a demo.json file.",
                                        app.demos_dir.string())
                          : "No demo matches the filter.");
    for (const std::string& err : app.catalog.errors)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, vec(palette::warning));
        ImGui::TextWrapped(ICON_TRIANGLE_ALERT "  %s", err.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Dummy(ImVec2(0, em * 2.0f));
}

// ----------------------------------------------------------------------------
// A demo's page
// ----------------------------------------------------------------------------

void drawSettingsCard(App& app, const Demo& d)
{
    if (d.options.empty())
        return;
    const bool running = app.runner.running() && app.runner.demoId() == d.id;
    ui::beginCard("settings");
    ui::sectionTitle(app.fonts, ICON_SLIDERS_HORIZONTAL, "Settings");
    for (const DemoOption& o : d.options)
    {
        ImGui::PushID(o.id.c_str());
        OptionValue& v = optionValue(app, d, o);
        switch (o.type)
        {
        case DemoOption::Type::Int:
        case DemoOption::Type::Float:
        {
            ImGui::PushFont(app.fonts.medium);
            ImGui::TextUnformatted(o.label.c_str());
            ImGui::PopFont();
            ImGui::SetNextItemWidth(-FLT_MIN);
            // Wide positive ranges (e.g. 100 to 100000 bodies) are easier on a log scale.
            const ImGuiSliderFlags flags =
                ImGuiSliderFlags_AlwaysClamp | (o.min > 0 && o.max / o.min >= 100 ? ImGuiSliderFlags_Logarithmic : 0);
            if (o.type == DemoOption::Type::Int)
            {
                int value = static_cast<int>(std::llround(v.number));
                if (ImGui::SliderInt("##v", &value, static_cast<int>(o.min), static_cast<int>(o.max), "%d", flags))
                    v.number = value;
            }
            else
            {
                float value = static_cast<float>(v.number);
                if (ImGui::SliderFloat("##v", &value, static_cast<float>(o.min), static_cast<float>(o.max), "%.3g", flags))
                    v.number = value;
            }
            break;
        }
        case DemoOption::Type::Bool:
            ui::toggle("##v", &v.flag);
            ImGui::SameLine();
            ImGui::PushFont(app.fonts.medium);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(o.label.c_str());
            ImGui::PopFont();
            break;
        case DemoOption::Type::Choice:
            ImGui::PushFont(app.fonts.medium);
            ImGui::TextUnformatted(o.label.c_str());
            ImGui::PopFont();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##v", v.choice.c_str()))
            {
                for (const std::string& c : o.choices)
                    if (ImGui::Selectable(c.c_str(), c == v.choice))
                        v.choice = c;
                ImGui::EndCombo();
            }
            break;
        }
        if (!o.help.empty())
        {
            ImGui::PushFont(app.fonts.small);
            ui::paragraph(o.help);
            ImGui::PopFont();
        }
        ImGui::PopID();
        ImGui::Dummy(ImVec2(0, 2));
    }
    if (ui::ghostButton(ICON_ROTATE_CCW "  Reset to defaults"))
        for (const DemoOption& o : d.options)
            optionValue(app, d, o) = defaultValue(o);
    if (running)
    {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Applies on the next launch");
    }
    ui::endCard();
}

void drawDemoPage(App& app)
{
    const Demo* dp = selectedDemo(app);
    if (!dp)
    {
        openPage(app, Page::Discover);
        return;
    }
    const Demo& d = *dp;
    const float em = ImGui::GetFontSize();
    const float width = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (ui::ghostButton(ICON_ARROW_LEFT "  All demos"))
        openPage(app, Page::Discover);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Esc");
    ImGui::Dummy(ImVec2(0, em * 0.3f));

    // Banner with the title over the picture.
    const float banner_h = std::clamp(width * 0.3f, em * 13.0f, em * 20.0f);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + width, p0.y + banner_h);
    ui::shadow(dl, p0, p1, em * 1.1f);
    demoArt(app, dl, d, p0, p1, em * 1.1f, ImDrawFlags_RoundCornersAll);
    ui::gradient(dl, ImVec2(p0.x, p0.y + banner_h * 0.3f), p1, withAlpha(palette::background, 0.0f),
                 withAlpha(palette::background, 0.92f), true, em * 1.1f, ImDrawFlags_RoundCornersBottom);
    dl->AddRect(p0, p1, palette::border, em * 1.1f);
    float cx = p0.x + em * 1.8f;
    const float title_px = fontPx(app.fonts.display);
    const float title_y = p1.y - em * 1.6f - title_px - em * 2.6f;
    cx += ui::chip(dl, app.fonts, ImVec2(cx, title_y - em * 2.0f), d.category.c_str(), IM_COL32(8, 10, 15, 200),
                   categoryColor(d.category)) + em * 0.4f;
    for (const std::string& t : d.tags)
        cx += ui::chip(dl, app.fonts, ImVec2(cx, title_y - em * 2.0f), t.c_str(), IM_COL32(8, 10, 15, 200), palette::text) +
              em * 0.4f;
    dl->AddText(app.fonts.display, title_px, ImVec2(p0.x + em * 1.8f, title_y), palette::text, d.title.c_str());
    const std::string byline = d.author.empty() ? d.summary : std::format("{}   ·   by {}", d.summary, d.author);
    const ImVec4 clip(p0.x, p0.y, p1.x - em * 1.8f, p1.y);
    dl->AddText(app.fonts.body, fontPx(app.fonts.body), ImVec2(p0.x + em * 1.8f, title_y + title_px + em * 0.6f),
                IM_COL32(205, 212, 224, 255), byline.c_str(), nullptr, width - em * 3.6f, &clip);
    ImGui::Dummy(ImVec2(width, banner_h + em * 1.2f));

    // Explanations on the left, launch / settings / controls on the right.
    const bool two_columns = width > em * 42.0f;
    if (ImGui::BeginTable("demo", two_columns ? 2 : 1, ImGuiTableFlags_SizingStretchProp))
    {
        if (two_columns)
        {
            ImGui::TableSetupColumn("about", ImGuiTableColumnFlags_WidthStretch, 1.55f);
            ImGui::TableSetupColumn("side", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        }
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::PushTextWrapPos(0.0f);

        ui::beginCard("about");
        ui::sectionTitle(app.fonts, ICON_INFO, "About");
        ui::paragraph(d.description);
        if (!d.how_it_works.empty())
        {
            ImGui::Dummy(ImVec2(0, em * 0.4f));
            ui::sectionTitle(app.fonts, ICON_LAYERS, "How it works");
            for (const std::string& p : d.how_it_works)
                ui::paragraph(p);
        }
        if (!d.highlights.empty())
        {
            ImGui::Dummy(ImVec2(0, em * 0.4f));
            ui::sectionTitle(app.fonts, ICON_EYE, "What to look for");
            for (const std::string& h : d.highlights)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, vec(palette::accent));
                ImGui::TextUnformatted(ICON_CHECK);
                ImGui::PopStyleColor();
                ImGui::SameLine(0, em * 0.6f);
                ui::paragraph(h);
            }
        }
        ui::endCard();
        ImGui::PopTextWrapPos();

        if (two_columns)
            ImGui::TableSetColumnIndex(1);
        else
            ImGui::Dummy(ImVec2(0, em * 0.6f));

        // Launch
        ui::beginCard("launch");
        const bool this_running = app.runner.running() && app.runner.demoId() == d.id;
        const bool other_running = app.runner.running() && !this_running;
        const ImVec2 button(ImGui::GetContentRegionAvail().x, em * 2.9f);
        if (this_running)
        {
            if (ui::primaryButton(ICON_MONITOR "  Return to the demo", button))
                openPage(app, Page::Stage);
        }
        else if (ui::primaryButton(ICON_PLAY "  Launch", button, d.available() && !other_running))
            launch(app, d);
        ImGui::Dummy(ImVec2(0, 2));
        if (!d.available())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, vec(IM_COL32(255, 160, 150, 255)));
            ImGui::TextWrapped(ICON_CIRCLE_ALERT "  %s", d.unavailable_reason.c_str());
            ImGui::PopStyleColor();
        }
        else if (other_running)
            ui::paragraph(ICON_CLOCK "  Another demo is running. Stop it first.");
        else
        {
            const char* where = d.window == Demo::Window::None ? ICON_TERMINAL "  Output appears in this window. Esc brings you back."
                                : embedsInGallery(app, d)
                                    ? ICON_MONITOR "  Opens inside this window. Esc returns to the gallery."
                                    : ICON_LAYERS "  Opens in its own window. Close it or press Esc there to return.";
            ImGui::PushFont(app.fonts.small);
            ui::paragraph(where);
            ImGui::PopFont();
        }
        if (app.last_run_id == d.id && app.runner.hasExited() && !app.runner.running())
        {
            ImGui::PushFont(app.fonts.small);
            ui::paragraph(std::format(ICON_CLOCK "  Last run: exit code {} after {:.1f} s", app.runner.exitCode(),
                                      app.runner.secondsRunning()));
            ImGui::PopFont();
        }
        ui::endCard();
        ImGui::Dummy(ImVec2(0, em * 0.2f));

        drawSettingsCard(app, d);
        if (!d.options.empty())
            ImGui::Dummy(ImVec2(0, em * 0.2f));

        if (!d.controls.empty())
        {
            ui::beginCard("controls");
            ui::sectionTitle(app.fonts, ICON_KEYBOARD, "Controls");
            keysTable(app, "keys", d.controls);
            ui::endCard();
            ImGui::Dummy(ImVec2(0, em * 0.2f));
        }

        if (!d.requires_.empty())
        {
            ui::beginCard("requires");
            ui::sectionTitle(app.fonts, ICON_CPU, "Needs");
            for (const std::string& r : d.requires_)
            {
                if (r == "opencl")
                    ui::statusRow(app.fonts, app.sys.cl_devices.empty() ? palette::danger : palette::success, "OpenCL",
                                  app.sys.cl_devices.empty() ? app.sys.cl_error
                                                             : std::format("{} device{} available", app.sys.cl_devices.size(),
                                                                           app.sys.cl_devices.size() == 1 ? "" : "s"));
                else if (r == "opengl")
                    ui::statusRow(app.fonts, app.software_rendering ? palette::warning : palette::success, "OpenGL 3.3",
                                  app.sys.gl_renderer);
                else
                    ui::statusRow(app.fonts, palette::muted, r.c_str(), "");
            }
            ui::endCard();
        }
        ImGui::EndTable();
    }
    ImGui::Dummy(ImVec2(0, em * 2.0f));
}

// ----------------------------------------------------------------------------
// System
// ----------------------------------------------------------------------------

void drawSystem(App& app)
{
    const float em = ImGui::GetFontSize();
    pageHeader(app, "System", "What the gallery found on this computer, and what it means for the demos.");

    const bool cpu_only_cl = !app.sys.cl_devices.empty() &&
                             std::none_of(app.sys.cl_devices.begin(), app.sys.cl_devices.end(), [](const ClDevice& d) { return d.gpu; });
    const bool embed = app.embedder && app.embedder->supported();
    const bool in_docker = app.environment.rfind("Docker", 0) == 0;

    ui::beginCard("checks");
    ui::sectionTitle(app.fonts, ICON_CIRCLE_CHECK, "Checks");
    if (app.software_rendering)
        ui::statusRow(app.fonts, palette::warning, "OpenGL is software-rendered",
                      std::format("{} draws on the CPU, so graphics demos run slowly. Install your GPU's driver{}.",
                                  app.sys.gl_renderer, in_docker ? ", or use the Docker environment for your GPU (./run.sh --service ...)" : ""));
    else
        ui::statusRow(app.fonts, palette::success, "OpenGL runs on the GPU", app.sys.gl_renderer);

    if (app.sys.cl_devices.empty())
        ui::statusRow(app.fonts, palette::danger, "No OpenCL device", app.sys.cl_error + ". OpenCL demos are disabled.");
    else if (cpu_only_cl)
        ui::statusRow(app.fonts, palette::warning, "OpenCL runs on the CPU",
                      in_docker ? "Docker Desktop gives containers no GPU OpenCL driver on Windows and macOS, so OpenCL "
                                  "uses all CPU cores (PoCL). The native release package uses the GPU."
                                : "No OpenCL driver for a GPU was found; install your GPU vendor's driver for GPU speed.");
    else
        ui::statusRow(app.fonts, palette::success, "OpenCL runs on the GPU",
                      std::format("{} device{} available", app.sys.cl_devices.size(), app.sys.cl_devices.size() == 1 ? "" : "s"));

    if (embed && app.settings.embed_demos)
        ui::statusRow(app.fonts, palette::success, "Demos open inside the gallery", "Their windows are placed in this window; Esc returns.");
    else if (!embed)
        ui::statusRow(app.fonts, palette::accent, "Demos open in their own window",
                      "This window system does not let one program hold another's window (macOS, Wayland). The gallery "
                      "steps aside while a demo runs and comes back when it closes.");
    else
        ui::statusRow(app.fonts, palette::accent, "Demos open in their own window", "Switched off in Settings.");
    ui::endCard();
    ImGui::Dummy(ImVec2(0, em * 0.4f));

    const bool two = ImGui::GetContentRegionAvail().x > em * 44.0f;
    if (ImGui::BeginTable("cards", two ? 2 : 1, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ui::beginCard("graphics");
        ui::sectionTitle(app.fonts, ICON_MONITOR, "Graphics");
        if (ImGui::BeginTable("gl", 2, ImGuiTableFlags_SizingStretchProp))
        {
            auto row = [](const char* k, const std::string& v) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", k);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", v.c_str());
            };
            row("Renderer", app.sys.gl_renderer);
            row("OpenGL", app.sys.gl_version);
            float sx = 1, sy = 1;
            glfwGetWindowContentScale(app.window, &sx, &sy);
            row("Display scale", std::format("{:.0f}%", sx * 100));
            const int platform = glfwGetPlatform();
            row("Window system", platform == GLFW_PLATFORM_WIN32     ? "Windows"
                                 : platform == GLFW_PLATFORM_COCOA   ? "macOS (Cocoa)"
                                 : platform == GLFW_PLATFORM_X11     ? "X11"
                                 : platform == GLFW_PLATFORM_WAYLAND ? "Wayland"
                                                                     : "Other");
            ImGui::EndTable();
        }
        ui::endCard();

        ImGui::TableSetColumnIndex(two ? 1 : 0);
        if (!two)
            ImGui::Dummy(ImVec2(0, em * 0.4f));
        ui::beginCard("compute");
        ui::sectionTitle(app.fonts, ICON_MICROCHIP, "OpenCL devices");
        if (app.sys.cl_devices.empty())
            ui::paragraph(app.sys.cl_error);
        for (size_t i = 0; i < app.sys.cl_devices.size(); ++i)
        {
            const ClDevice& dev = app.sys.cl_devices[i];
            ui::tag(app.fonts, dev.gpu ? "GPU" : "CPU", dev.gpu ? palette::success : palette::accent);
            ImGui::SameLine();
            ImGui::PushFont(app.fonts.medium);
            ImGui::TextWrapped("%s", dev.name.c_str());
            ImGui::PopFont();
            ImGui::PushFont(app.fonts.small);
            ui::paragraph(std::format("{}  ·  {}{}", dev.platform, dev.version,
                                      static_cast<int>(i) == app.cl_choice ? "  ·  chosen in Settings" : ""));
            ImGui::PopFont();
        }
        ui::endCard();
        ImGui::EndTable();
    }
    ImGui::Dummy(ImVec2(0, em * 0.4f));

    ui::beginCard("environment");
    ui::sectionTitle(app.fonts, ICON_CONTAINER, "Environment");
    ui::paragraph(std::format("Running: {}", app.environment));
    ui::paragraph(std::format("Gallery folder: {}", app.app_dir.string()));
    ui::paragraph(std::format("Settings file: {}", settingsPath().string()));
    if (in_docker)
        ui::paragraph("Started by run.sh / run.cmd. Switching the Docker environment, rebuilding the image and the "
                      "text menu are there too: ./run.sh help.");
    ui::endCard();

    if (!app.build_dir.empty())
    {
        ImGui::Dummy(ImVec2(0, em * 0.4f));
        ui::beginCard("developer");
        ui::sectionTitle(app.fonts, ICON_HAMMER, "Developer");
        ui::paragraph("The gallery is running from a CMake build folder. Rebuild compiles changed demos and the gallery; "
                      "demo folders are read again when it finishes.");
        ImGui::Dummy(ImVec2(0, 2));
        if (app.builder.running())
            ui::primaryButton(ICON_HAMMER "  Building...", ImVec2(em * 11.0f, em * 2.5f), false);
        else if (ui::primaryButton(ICON_HAMMER "  Rebuild", ImVec2(em * 11.0f, em * 2.5f)))
            startRebuild(app);
        if (app.builder.running() || app.builder.hasExited())
        {
            ImGui::Dummy(ImVec2(0, 2));
            drawLog(app, app.build_view, app.builder, "build", ImVec2(0, em * 14.0f));
        }
        ui::endCard();
    }
    ImGui::Dummy(ImVec2(0, em * 2.0f));
}

// ----------------------------------------------------------------------------
// Settings
// ----------------------------------------------------------------------------

void settingRow(const App& app, const char* title, const char* description)
{
    ImGui::PushFont(app.fonts.medium);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::PushFont(app.fonts.small);
    ui::paragraph(description);
    ImGui::PopFont();
}

void drawSettings(App& app)
{
    const float em = ImGui::GetFontSize();
    pageHeader(app, "Settings", "Remembered between runs.");

    ui::beginCard("compute");
    ui::sectionTitle(app.fonts, ICON_MICROCHIP, "OpenCL device");
    ui::paragraph("Which device OpenCL demos run on. Automatic picks the first GPU, or the first device if there is no GPU.");
    ImGui::Dummy(ImVec2(0, 2));
    auto option = [&](int index, const std::string& title, const std::string& detail, bool gpu) {
        ImGui::PushID(index);
        const bool on = app.cl_choice == index;
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const float w = ImGui::GetContentRegionAvail().x;
        const ImVec2 size(w, em * 3.3f);
        if (ImGui::InvisibleButton("##dev", size))
            app.cl_choice = index;
        const float hover = ui::animate(ImGui::GetID("h"), ImGui::IsItemHovered() ? 1.0f : 0.0f);
        ImGui::PopID();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p1(p0.x + size.x, p0.y + size.y);
        dl->AddRectFilled(p0, p1, on ? withAlpha(palette::accent, 0.12f) : IM_COL32(255, 255, 255, static_cast<int>(6 + 10 * hover)),
                          em * 0.6f);
        dl->AddRect(p0, p1, on ? withAlpha(palette::accent, 0.8f) : palette::border, em * 0.6f);
        const ImVec2 dot(p0.x + em * 1.1f, (p0.y + p1.y) * 0.5f);
        dl->AddCircle(dot, em * 0.42f, on ? palette::accent : palette::muted, 20, 1.5f);
        if (on)
            dl->AddCircleFilled(dot, em * 0.22f, palette::accent, 20);
        dl->AddText(app.fonts.medium, fontPx(app.fonts.medium), ImVec2(p0.x + em * 2.2f, p0.y + em * 0.55f), palette::text,
                    (std::string(index < 0 ? ICON_ZAP : gpu ? ICON_MONITOR : ICON_CPU) + "  " + title).c_str());
        dl->AddText(app.fonts.small, fontPx(app.fonts.small), ImVec2(p0.x + em * 2.2f, p0.y + em * 1.85f), palette::muted,
                    detail.c_str());
        ImGui::Dummy(ImVec2(0, em * 0.1f));
    };
    if (app.sys.cl_devices.empty())
        ui::paragraph(app.sys.cl_error);
    else
    {
        option(-1, "Automatic", "First GPU, else the first device", false);
        for (size_t i = 0; i < app.sys.cl_devices.size(); ++i)
        {
            const ClDevice& d = app.sys.cl_devices[i];
            option(static_cast<int>(i), d.name, std::format("{}  ·  {}", d.platform, d.version), d.gpu);
        }
    }
    ui::endCard();
    ImGui::Dummy(ImVec2(0, em * 0.4f));

    ui::beginCard("display");
    ui::sectionTitle(app.fonts, ICON_MONITOR, "Display");
    const bool embed_supported = app.embedder && app.embedder->supported();
    if (ImGui::BeginTable("rows", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("text", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("control", ImGuiTableColumnFlags_WidthFixed, em * 9.0f);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        settingRow(app, "Show demos inside the gallery",
                   embed_supported ? "A demo's window is placed in this window, and Esc returns to the gallery. Off: demos "
                                     "open in their own window."
                                   : "Not possible on this window system (macOS, Wayland): demos open in their own window.");
        ImGui::TableSetColumnIndex(1);
        ImGui::BeginDisabled(!embed_supported);
        ui::toggle("##embed", &app.settings.embed_demos);
        ImGui::EndDisabled();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        settingRow(app, "Animations",
                   app.software_rendering ? "The moving background and logo. Off while OpenGL is software-rendered, to "
                                            "keep the CPU free."
                                          : "The moving background and logo. They pause while a demo runs.");
        ImGui::TableSetColumnIndex(1);
        ui::toggle("##anim", &app.settings.animations);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        settingRow(app, "Interface size", "Automatic follows the screen's scaling.");
        ImGui::TableSetColumnIndex(1);
        static const float scales[] = {0.0f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f};
        const std::string current = app.settings.ui_scale > 0 ? std::format("{:.0f}%", app.settings.ui_scale * 100) : "Automatic";
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##scale", current.c_str()))
        {
            for (float s : scales)
            {
                const std::string label = s > 0 ? std::format("{:.0f}%", s * 100) : "Automatic";
                if (ImGui::Selectable(label.c_str(), s == app.settings.ui_scale))
                {
                    app.settings.ui_scale = s;
                    float x = 1, y = 1;
                    glfwGetWindowContentScale(app.window, &x, &y);
                    app.pending_scale = s > 0 ? s : std::max(x, y);
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndTable();
    }
    ui::endCard();
    ImGui::Dummy(ImVec2(0, em * 0.4f));

    ui::beginCard("data");
    ui::sectionTitle(app.fonts, ICON_PACKAGE, "Saved settings");
    ui::paragraph(std::format("Stored in {}", settingsPath().string()));
    ImGui::Dummy(ImVec2(0, 2));
    static double confirm_until = 0.0;
    const bool confirming = app.now < confirm_until;
    if (ui::dangerButton(confirming ? ICON_TRASH_2 "  Click again to reset" : ICON_TRASH_2 "  Reset all settings"))
    {
        if (confirming)
        {
            app.settings = Settings{};
            app.options.clear();
            app.cl_choice = -1;
            toast(app, ICON_ROTATE_CCW, "Settings reset", palette::accent);
            confirm_until = 0.0;
        }
        else
            confirm_until = app.now + 3.0;
    }
    ui::endCard();
    ImGui::Dummy(ImVec2(0, em * 2.0f));
}

// ----------------------------------------------------------------------------
// Help
// ----------------------------------------------------------------------------

void drawHelp(App& app)
{
    const float em = ImGui::GetFontSize();
    pageHeader(app, "Help", "How the gallery works, and how to add your own demo.");

    ui::beginCard("start");
    ui::sectionTitle(app.fonts, ICON_ROCKET, "Getting started");
    const char* steps[] = {
        "Pick a demo on Discover. Its page explains what it shows and how it works.",
        "Adjust its settings if it has any, then press Launch.",
        "The demo runs inside this window (or its own, where the system does not allow that). Its controls are on its page.",
        "Press Esc to leave the demo; you are back in the gallery. Programs without a window show their output here instead.",
    };
    for (int i = 0; i < 4; ++i)
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddCircleFilled(ImVec2(p.x + em * 0.7f, p.y + em * 0.68f), em * 0.7f, withAlpha(palette::accent, 0.18f), 24);
        const std::string n = std::to_string(i + 1);
        dl->AddText(app.fonts.medium, fontPx(app.fonts.medium), ImVec2(p.x + em * 0.7f - em * 0.25f, p.y + em * 0.15f), palette::accent,
                    n.c_str());
        ImGui::Indent(em * 2.0f);
        ui::paragraph(steps[i]);
        ImGui::Unindent(em * 2.0f);
        ImGui::Dummy(ImVec2(0, 2));
    }
    ui::endCard();
    ImGui::Dummy(ImVec2(0, em * 0.4f));

    ui::beginCard("keyboard");
    ui::sectionTitle(app.fonts, ICON_KEYBOARD, "Keyboard");
    keysTable(app, "k",
              {{"Enter", "Launch the demo whose page is open"},
               {"Esc", "Back: leave a demo, or return to Discover"},
               {"F5", "Read the demo folders again"},
               {"Ctrl F", "Search demos"}});
    ui::endCard();
    ImGui::Dummy(ImVec2(0, em * 0.4f));

    ui::beginCard("add");
    ui::sectionTitle(app.fonts, ICON_PACKAGE, "Adding a demo");
    ui::paragraph("A demo is any program. To show it here, add a folder with a demo.json file to gallery/demos (the "
                  "README has every field) and press F5:");
    ImGui::PushFont(app.fonts.mono);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, vec(IM_COL32(0, 0, 0, 90)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em, em * 0.8f));
    ImGui::BeginChild("snippet", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::TextUnformatted(R"({
    "title": "N-body on the GPU",
    "category": "Compute",
    "summary": "One line for the card.",
    "executable": "nbody/nbody",
    "requires": ["opencl", "opengl"],
    "controls": [["Esc", "Back to the gallery"]],
    "options": [{"id": "bodies", "type": "int", "min": 100,
                 "max": 100000, "default": 10000,
                 "args": ["--bodies", "{value}"]}]
})");
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ui::paragraph("For the smoothest result a demo quits on Esc and when its window is closed, and loads its data relative "
                  "to its own folder.");
    ui::endCard();
    ImGui::Dummy(ImVec2(0, em * 0.4f));

    ui::beginCard("faq");
    ui::sectionTitle(app.fonts, ICON_CIRCLE_HELP, "Questions");
    const std::pair<const char*, const char*> faq[] = {
        {"A demo opened in its own window",
         "Windows can only be placed inside the gallery on Windows and on Linux with X11. On macOS and Wayland, and for "
         "demos whose manifest asks for it, they open separately; the gallery comes back when they close."},
        {"OpenCL runs on the CPU",
         "Inside Docker on Windows and macOS there is no GPU OpenCL driver, so OpenCL uses the CPU cores. The native "
         "release package uses the GPU driver's OpenCL."},
        {"A demo is greyed out",
         "Its page says why: usually no OpenCL device, or the program is not part of this build or not for this system."},
        {"Where are my settings?", "In the file shown under Settings > Saved settings. Delete it to start fresh."},
    };
    for (const auto& [q, a] : faq)
    {
        ImGui::PushFont(app.fonts.medium);
        const bool open = ImGui::CollapsingHeader(q);
        ImGui::PopFont();
        if (open)
        {
            ImGui::Indent(em * 0.8f);
            ui::paragraph(a);
            ImGui::Unindent(em * 0.8f);
        }
    }
    ui::endCard();
    ImGui::Dummy(ImVec2(0, em * 0.4f));

    ui::beginCard("about");
    ui::sectionTitle(app.fonts, ICON_INFO, "About");
    ui::paragraph(std::format("Compute Gallery {}. Third-party components and their licenses: THIRD_PARTY_NOTICES.md "
                              "next to the program. Fonts: Inter, Cousine; icons: Lucide.",
                              CG_VERSION));
    ui::endCard();
    ImGui::Dummy(ImVec2(0, em * 2.0f));
}

void drawToasts(App& app)
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float em = ImGui::GetFontSize();
    float y = vp->Pos.y + vp->Size.y - em * 1.4f;
    std::erase_if(app.toasts, [&](const Toast& t) { return app.now - t.born > 4.0; });
    for (auto it = app.toasts.rbegin(); it != app.toasts.rend(); ++it)
    {
        const double age = app.now - it->born;
        const float alpha = static_cast<float>(std::min({1.0, age / 0.2, (4.0 - age) / 0.4}));
        const std::string text = it->icon + "   " + it->text;
        const ImVec2 ts = app.fonts.medium->CalcTextSizeA(fontPx(app.fonts.medium), FLT_MAX, 0, text.c_str());
        const ImVec2 p1(vp->Pos.x + vp->Size.x - em * 1.4f, y);
        const ImVec2 p0(p1.x - ts.x - em * 2.2f, p1.y - ts.y - em * 1.4f);
        ui::shadow(dl, p0, p1, em * 0.7f, alpha);
        dl->AddRectFilled(p0, p1, withAlpha(palette::surface_solid, alpha), em * 0.7f);
        dl->AddRect(p0, p1, withAlpha(it->color, 0.5f * alpha), em * 0.7f);
        dl->AddText(app.fonts.medium, fontPx(app.fonts.medium), ImVec2(p0.x + em * 1.1f, p0.y + em * 0.7f), withAlpha(palette::text, alpha),
                    text.c_str());
        y = p0.y - em * 0.6f;
        ui::keepAnimating();
    }
}
} // namespace

// ----------------------------------------------------------------------------
// Shared console view
// ----------------------------------------------------------------------------

void drawLog(App& app, App::LogView& view, const DemoRunner& source, const char* id, ImVec2 size)
{
    // Copy the log and index its lines only when it changed, not every frame.
    if (const uint64_t version = source.logVersion(); version != view.version)
    {
        view.version = version;
        source.copyLog(view.text);
        view.lines.assign(1, 0);
        for (size_t i = 0; i < view.text.size(); ++i)
            if (view.text[i] == '\n' && i + 1 < view.text.size())
                view.lines.push_back(i + 1);
    }
    const float em = ImGui::GetFontSize();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, vec(IM_COL32(0, 0, 0, 110)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 0.9f, em * 0.7f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, em * 0.6f);
    ImGui::BeginChild(id, size, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushFont(app.fonts.mono);
    const bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
    if (!view.text.empty())
    {
        // Only the lines in view are laid out, however long the log is.
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(view.lines.size()));
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const char* begin = view.text.data() + view.lines[i];
                const char* end = i + 1 < static_cast<int>(view.lines.size()) ? view.text.data() + view.lines[i + 1] - 1
                                                                               : view.text.data() + view.text.size();
                while (end > begin && (end[-1] == '\n' || end[-1] == '\r'))
                    --end;
                ImGui::TextUnformatted(begin, end);
            }
    }
    else
        ImGui::TextDisabled("No output yet.");
    if (at_bottom)
        ImGui::SetScrollHereY(1.0f);
    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    if (ImGui::BeginPopupContextItem(id))
    {
        if (ImGui::MenuItem(ICON_COPY "  Copy all"))
            ImGui::SetClipboardText(view.text.c_str());
        ImGui::EndPopup();
    }
}

// ----------------------------------------------------------------------------
// Root window
// ----------------------------------------------------------------------------

void drawGallery(App& app)
{
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin("##gallery", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    const float em = ImGui::GetFontSize();

    if (app.page == Page::Stage)
        drawStage(app, vp->Pos, vp->Size);
    else
    {
        const float sidebar_w = em * 15.5f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + sidebar_w, vp->Pos.y + vp->Size.y), withAlpha(palette::sidebar, 0.86f));
        dl->AddLine(ImVec2(vp->Pos.x + sidebar_w, vp->Pos.y), ImVec2(vp->Pos.x + sidebar_w, vp->Pos.y + vp->Size.y), palette::border);

        ImGui::SetCursorScreenPos(vp->Pos);
        ImGui::BeginChild("sidebar", ImVec2(sidebar_w, vp->Size.y), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        drawSidebar(app, sidebar_w, vp->Size.y);
        ImGui::EndChild();

        // Pages fade in.
        const float t = std::clamp(static_cast<float>(app.now - app.page_since) / 0.22f, 0.0f, 1.0f);
        if (t < 1.0f)
            ui::keepAnimating();
        const float ease = 1.0f - (1.0f - t) * (1.0f - t);
        ImGui::SetCursorScreenPos(ImVec2(vp->Pos.x + sidebar_w, vp->Pos.y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 2.4f, em * 1.9f));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.25f + 0.75f * ease);
        ImGui::BeginChild("page", ImVec2(vp->Size.x - sidebar_w, vp->Size.y), ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (1.0f - ease) * em * 0.8f); // slight rise
        switch (app.page)
        {
        case Page::Discover: drawDiscover(app); break;
        case Page::Demo: drawDemoPage(app); break;
        case Page::System: drawSystem(app); break;
        case Page::Settings: drawSettings(app); break;
        case Page::Help: drawHelp(app); break;
        case Page::Stage: break;
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(2);

        // Keyboard: Esc goes back, Enter launches from a demo page, F5 refreshes.
        if (!ImGui::GetIO().WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape) && app.page != Page::Discover)
                openPage(app, Page::Discover);
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) && app.page == Page::Demo)
                if (const Demo* d = selectedDemo(app))
                    launch(app, *d);
            if (ImGui::IsKeyPressed(ImGuiKey_F5))
            {
                reloadCatalog(app);
                toast(app, ICON_REFRESH_CW, std::format("{} demos found", app.catalog.demos.size()), palette::accent);
            }
        }
    }
    drawToasts(app);
    ImGui::End();
}
