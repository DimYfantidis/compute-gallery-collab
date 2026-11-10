#include "widgets.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

#include <imgui_internal.h>

namespace ui
{
namespace
{
bool g_animating = false;

ImVec4 vec(ImU32 c)
{
    return ImGui::ColorConvertU32ToFloat4(c);
}

// Rounded rectangle filled with a left-to-right gradient.
void gradientRect(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 left, ImU32 right, float rounding)
{
    const int first = dl->VtxBuffer.Size;
    dl->AddRectFilled(p0, p1, IM_COL32_WHITE, rounding);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, first, dl->VtxBuffer.Size, p0, ImVec2(p1.x, p0.y), left, right);
}

// Centres `label` (icon + text) in p0..p1.
void centeredLabel(ImDrawList* dl, ImVec2 p0, ImVec2 p1, const char* label, ImU32 color)
{
    const char* end = ImGui::FindRenderedTextEnd(label);
    const ImVec2 ts = ImGui::CalcTextSize(label, end);
    dl->AddText(ImVec2(std::floor((p0.x + p1.x - ts.x) * 0.5f), std::floor((p0.y + p1.y - ts.y) * 0.5f)), color, label,
                end);
}
} // namespace

void beginFrame()
{
    g_animating = false;
}

bool animating()
{
    return g_animating;
}

void keepAnimating()
{
    g_animating = true;
}

void gradient(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 c0, ImU32 c1, bool vertical, float rounding, ImDrawFlags corners)
{
    const int first = dl->VtxBuffer.Size;
    dl->AddRectFilled(p0, p1, IM_COL32_WHITE, rounding, corners);
    const float span = vertical ? p1.y - p0.y : p1.x - p0.x;
    for (int i = first; i < dl->VtxBuffer.Size; ++i)
    {
        ImDrawVert& v = dl->VtxBuffer[i];
        const float t = std::clamp(((vertical ? v.pos.y - p0.y : v.pos.x - p0.x)) / std::max(span, 1.0f), 0.0f, 1.0f);
        v.col = mix(c0, c1, t);
    }
}

float animate(ImGuiID id, float target, float speed)
{
    float* value = ImGui::GetStateStorage()->GetFloatRef(id, target);
    const float dt = std::min(ImGui::GetIO().DeltaTime, 0.1f);
    *value += (target - *value) * std::min(1.0f, dt * speed);
    if (std::fabs(target - *value) < 0.002f)
        *value = target;
    else
        g_animating = true;
    return *value;
}

std::string ellipsize(std::string s, float max_width)
{
    if (ImGui::CalcTextSize(s.c_str()).x <= max_width)
        return s;
    while (!s.empty() && ImGui::CalcTextSize((s + "...").c_str()).x > max_width)
        s.pop_back();
    return s + "...";
}

static ImVec2 buttonSize(const char* label, ImVec2 size)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
    if (size.x <= 0.0f)
        size.x = ts.x + style.FramePadding.x * 2.6f;
    if (size.y <= 0.0f)
        size.y = ts.y + style.FramePadding.y * 2.0f;
    return size;
}

bool primaryButton(const char* label, ImVec2 size, bool enabled)
{
    size = buttonSize(label, size);
    ImGui::PushID(label);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + size.x, p0.y + size.y);
    ImGui::BeginDisabled(!enabled);
    const bool pressed = ImGui::InvisibleButton("##primary", size);
    ImGui::EndDisabled();
    const bool hovered = enabled && ImGui::IsItemHovered();
    const float hover = animate(ImGui::GetID("hover"), hovered ? 1.0f : 0.0f);
    ImGui::PopID();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float rounding = ImGui::GetStyle().FrameRounding;
    if (!enabled)
    {
        dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, 18), rounding);
        centeredLabel(dl, p0, p1, label, palette::faint);
        return false;
    }
    // Glow that grows on hover.
    for (int i = 1; i <= 3; ++i)
    {
        const float g = (2.0f + 3.0f * hover) * i;
        dl->AddRectFilled(ImVec2(p0.x - g, p0.y - g * 0.6f), ImVec2(p1.x + g, p1.y + g), withAlpha(palette::accent, 0.035f + 0.03f * hover),
                          rounding + g);
    }
    gradientRect(dl, p0, p1, mix(palette::accent, palette::accent_hover, hover), mix(palette::accent_2, IM_COL32(170, 158, 255, 255), hover),
                 rounding);
    centeredLabel(dl, p0, p1, label, palette::on_accent);
    return pressed;
}

bool dangerButton(const char* label, ImVec2 size)
{
    size = buttonSize(label, size);
    ImGui::PushID(label);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + size.x, p0.y + size.y);
    const bool pressed = ImGui::InvisibleButton("##danger", size);
    const float hover = animate(ImGui::GetID("hover"), ImGui::IsItemHovered() ? 1.0f : 0.0f);
    ImGui::PopID();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, p1, withAlpha(palette::danger, 0.16f + 0.14f * hover), ImGui::GetStyle().FrameRounding);
    dl->AddRect(p0, p1, withAlpha(palette::danger, 0.55f), ImGui::GetStyle().FrameRounding);
    centeredLabel(dl, p0, p1, label, mix(IM_COL32(255, 170, 165, 255), IM_COL32_WHITE, hover));
    return pressed;
}

bool ghostButton(const char* label, ImVec2 size)
{
    size = buttonSize(label, size);
    ImGui::PushID(label);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + size.x, p0.y + size.y);
    const bool pressed = ImGui::InvisibleButton("##ghost", size);
    const float hover = animate(ImGui::GetID("hover"), ImGui::IsItemHovered() ? 1.0f : 0.0f);
    ImGui::PopID();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float rounding = ImGui::GetStyle().FrameRounding;
    dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, static_cast<int>(12 + 18 * hover)), rounding);
    dl->AddRect(p0, p1, IM_COL32(255, 255, 255, static_cast<int>(22 + 26 * hover)), rounding);
    centeredLabel(dl, p0, p1, label, mix(palette::text, IM_COL32_WHITE, hover));
    return pressed;
}

bool iconButton(const char* icon, const char* tooltip, float size)
{
    if (size <= 0.0f)
        size = ImGui::GetFrameHeight();
    ImGui::PushID(icon);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + size, p0.y + size);
    const bool pressed = ImGui::InvisibleButton("##icon", ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();
    const float hover = animate(ImGui::GetID("hover"), hovered ? 1.0f : 0.0f);
    ImGui::PopID();
    if (hovered && tooltip)
        ImGui::SetTooltip("%s", tooltip);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, p1, IM_COL32(255, 255, 255, static_cast<int>(10 + 22 * hover)), ImGui::GetStyle().FrameRounding);
    centeredLabel(dl, p0, p1, icon, mix(palette::muted, palette::text, hover));
    return pressed;
}

float chip(ImDrawList* dl, const Fonts& fonts, ImVec2 pos, const char* text, ImU32 bg, ImU32 fg)
{
    const float size = fontPx(fonts.small);
    const ImVec2 ts = fonts.small->CalcTextSizeA(size, FLT_MAX, 0, text);
    const ImVec2 pad(size * 0.62f, size * 0.28f);
    dl->AddRectFilled(pos, ImVec2(pos.x + ts.x + 2 * pad.x, pos.y + ts.y + 2 * pad.y), bg, size);
    dl->AddText(fonts.small, size, ImVec2(pos.x + pad.x, pos.y + pad.y), fg, text);
    return ts.x + 2 * pad.x;
}

void tag(const Fonts& fonts, const char* text, ImU32 color)
{
    const float size = fontPx(fonts.small);
    const ImVec2 ts = fonts.small->CalcTextSizeA(size, FLT_MAX, 0, text);
    const ImVec2 box(ts.x + size * 1.24f, ts.y + size * 0.56f);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    chip(ImGui::GetWindowDrawList(), fonts, p, text, withAlpha(color, 0.16f), mix(color, IM_COL32_WHITE, 0.35f));
    ImGui::Dummy(box);
}

void keys(const Fonts& fonts, const std::string& text)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float size = fontPx(fonts.small);
    const float height = size * 1.75f;
    std::istringstream words(text);
    std::string word;
    bool first = true;
    while (words >> word)
    {
        if (!first)
            ImGui::SameLine(0, size * 0.35f);
        first = false;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImVec2 ts = fonts.small->CalcTextSizeA(size, FLT_MAX, 0, word.c_str());
        // Lower-case words ("arrows", "or") and slashes are words, the rest are keys.
        const bool is_key = word != "/" && !std::islower(static_cast<unsigned char>(word[0]));
        if (is_key)
        {
            const float w = std::max(height, ts.x + size * 1.1f);
            const ImVec2 p1(p.x + w, p.y + height);
            dl->AddRectFilled(ImVec2(p.x, p.y + 2), ImVec2(p1.x, p1.y + 2), IM_COL32(0, 0, 0, 90), size * 0.4f);
            dl->AddRectFilled(p, p1, IM_COL32(255, 255, 255, 22), size * 0.4f);
            dl->AddRect(p, p1, IM_COL32(255, 255, 255, 40), size * 0.4f);
            dl->AddText(fonts.small, size, ImVec2(p.x + (w - ts.x) * 0.5f, p.y + (height - ts.y) * 0.5f), palette::text,
                        word.c_str());
            ImGui::Dummy(ImVec2(w, height + 2));
        }
        else
        {
            dl->AddText(fonts.small, size, ImVec2(p.x, p.y + (height - ts.y) * 0.5f), palette::muted, word.c_str());
            ImGui::Dummy(ImVec2(ts.x, height + 2));
        }
    }
}

void beginCard(const char* id, float width, float padding)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, vec(palette::surface));
    ImGui::PushStyleColor(ImGuiCol_Border, vec(palette::border));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::BeginChild(id, ImVec2(width, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar);
}

void endCard()
{
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void sectionTitle(const Fonts& fonts, const char* icon, const char* text)
{
    ImGui::PushFont(fonts.heading);
    if (icon)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, vec(palette::accent));
        ImGui::TextUnformatted(icon);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, ImGui::GetFontSize() * 0.45f);
    }
    ImGui::TextUnformatted(text);
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 2));
}

bool toggle(const char* id, bool* value)
{
    const float h = ImGui::GetFrameHeight() * 0.78f;
    const float w = h * 1.8f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(w, h));
    if (pressed)
        *value = !*value;
    const float on = animate(ImGui::GetID(id), *value ? 1.0f : 0.0f);
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), mix(IM_COL32(255, 255, 255, hovered ? 40 : 28), palette::accent, on), h * 0.5f);
    const float r = h * 0.5f - 3.0f;
    dl->AddCircleFilled(ImVec2(p.x + h * 0.5f + (w - h) * on, p.y + h * 0.5f), r, IM_COL32_WHITE, 24);
    return pressed;
}

void statusRow(const Fonts& fonts, ImU32 color, const char* title, const std::string& detail)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float em = ImGui::GetFontSize();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddCircleFilled(ImVec2(p.x + em * 0.35f, p.y + em * 0.62f), em * 0.36f, withAlpha(color, 0.25f), 20);
    dl->AddCircleFilled(ImVec2(p.x + em * 0.35f, p.y + em * 0.62f), em * 0.2f, color, 20);
    ImGui::Indent(em * 1.15f);
    ImGui::PushFont(fonts.medium);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    if (!detail.empty())
        paragraph(detail);
    ImGui::Unindent(em * 1.15f);
}

void imageCover(ImDrawList* dl, ImTextureID texture, int tex_w, int tex_h, ImVec2 p0, ImVec2 p1, float rounding,
                ImDrawFlags corners, float zoom, ImU32 tint)
{
    const float box = (p1.x - p0.x) / std::max(1.0f, p1.y - p0.y);
    const float img = static_cast<float>(tex_w) / std::max(1, tex_h);
    float u = 1.0f, v = 1.0f; // visible fraction of the image
    if (img > box)
        u = box / img;
    else
        v = img / box;
    u /= zoom, v /= zoom;
    dl->AddImageRounded(texture, p0, p1, ImVec2(0.5f - u * 0.5f, 0.5f - v * 0.5f), ImVec2(0.5f + u * 0.5f, 0.5f + v * 0.5f),
                        tint, rounding, corners);
}

void shadow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding, float strength)
{
    for (int i = 1; i <= 4; ++i)
    {
        const float s = 3.0f * i;
        dl->AddRectFilled(ImVec2(p0.x - s * 0.5f, p0.y + s * 0.3f), ImVec2(p1.x + s * 0.5f, p1.y + s),
                          IM_COL32(0, 0, 0, static_cast<int>(26 * strength)), rounding + s);
    }
}

void paragraph(const std::string& text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, vec(palette::muted));
    ImGui::TextWrapped("%s", text.c_str());
    ImGui::PopStyleColor();
}
} // namespace ui
