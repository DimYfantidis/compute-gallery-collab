#include "theme.hpp"

#include <array>
#include <functional>

#include "icons.hpp"

namespace fs = std::filesystem;

static ImVec4 vec(ImU32 c)
{
    return ImGui::ColorConvertU32ToFloat4(c);
}

Fonts loadFonts(const fs::path& font_dir, float raster_scale)
{
    ImGuiIO& io = ImGui::GetIO();
    static const ImWchar icon_range[] = {ICON_RANGE_MIN, ICON_RANGE_MAX, 0};

    auto load = [&](const char* file, float size, bool with_icons) -> ImFont* {
        const fs::path path = font_dir / file;
        ImFontConfig cfg;
        cfg.SizePixels = size * raster_scale;
        cfg.OversampleH = 2;
        ImFont* font = fs::exists(path) ? io.Fonts->AddFontFromFileTTF(path.string().c_str(), size * raster_scale, &cfg)
                                        : io.Fonts->AddFontDefault(&cfg);
        const fs::path icons = font_dir / "lucide.ttf";
        if (font && with_icons && fs::exists(icons))
        {
            ImFontConfig merge;
            merge.MergeMode = true;
            merge.PixelSnapH = true;
            merge.GlyphOffset = ImVec2(0, size * raster_scale * 0.14f); // sit on the text baseline
            merge.GlyphMinAdvanceX = size * raster_scale;                // icons line up in lists
            io.Fonts->AddFontFromFileTTF(icons.string().c_str(), size * raster_scale, &merge, icon_range);
        }
        return font;
    };

    Fonts f;
    f.body = load("Inter-Regular.ttf", 15.0f, true); // first font added = default font
    f.medium = load("Inter-Medium.ttf", 15.0f, true);
    f.small = load("Inter-Medium.ttf", 12.5f, true);
    f.heading = load("Inter-SemiBold.ttf", 18.0f, true);
    f.title = load("InterDisplay-Bold.ttf", 30.0f, true);
    f.display = load("InterDisplay-Bold.ttf", 46.0f, false);
    f.mono = load("Cousine-Regular.ttf", 13.5f, false);
    return f;
}

void applyTheme(float ui_scale)
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = ImVec2(12, 8);
    s.ItemSpacing = ImVec2(10, 10);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.CellPadding = ImVec2(8, 7);
    s.ScrollbarSize = 10;
    s.WindowRounding = 0;
    s.ChildRounding = 14;
    s.FrameRounding = 9;
    s.PopupRounding = 12;
    s.ScrollbarRounding = 8;
    s.GrabRounding = 9;
    s.GrabMinSize = 14;
    s.TabRounding = 9;
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 0;
    s.FrameBorderSize = 0;
    s.PopupBorderSize = 1;
    s.SeparatorTextBorderSize = 1;
    s.SeparatorTextPadding = ImVec2(0, 6);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = vec(palette::text);
    c[ImGuiCol_TextDisabled] = vec(palette::muted);
    c[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0); // the background is drawn underneath
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = vec(palette::surface_solid);
    c[ImGuiCol_Border] = vec(palette::border);
    c[ImGuiCol_FrameBg] = vec(IM_COL32(255, 255, 255, 14));
    c[ImGuiCol_FrameBgHovered] = vec(IM_COL32(255, 255, 255, 22));
    c[ImGuiCol_FrameBgActive] = vec(IM_COL32(255, 255, 255, 28));
    c[ImGuiCol_Button] = vec(IM_COL32(255, 255, 255, 14));
    c[ImGuiCol_ButtonHovered] = vec(IM_COL32(255, 255, 255, 26));
    c[ImGuiCol_ButtonActive] = vec(IM_COL32(255, 255, 255, 36));
    c[ImGuiCol_Header] = vec(withAlpha(palette::accent, 0.18f));
    c[ImGuiCol_HeaderHovered] = vec(withAlpha(palette::accent, 0.26f));
    c[ImGuiCol_HeaderActive] = vec(withAlpha(palette::accent, 0.34f));
    c[ImGuiCol_CheckMark] = vec(palette::on_accent);
    c[ImGuiCol_SliderGrab] = vec(palette::accent);
    c[ImGuiCol_SliderGrabActive] = vec(palette::accent_hover);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = vec(IM_COL32(255, 255, 255, 28));
    c[ImGuiCol_ScrollbarGrabHovered] = vec(IM_COL32(255, 255, 255, 48));
    c[ImGuiCol_ScrollbarGrabActive] = vec(IM_COL32(255, 255, 255, 64));
    c[ImGuiCol_Separator] = vec(palette::border);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableBorderLight] = vec(palette::border);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.025f);
    c[ImGuiCol_TextSelectedBg] = vec(withAlpha(palette::accent, 0.35f));
    c[ImGuiCol_NavCursor] = vec(palette::accent);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.55f);

    s.ScaleAllSizes(ui_scale);
}

ImU32 categoryColor(std::string_view category)
{
    static constexpr std::array<ImU32, 6> colors = {
        IM_COL32(139, 124, 246, 255), // violet
        IM_COL32(54, 194, 224, 255),  // cyan
        IM_COL32(240, 136, 62, 255),  // orange
        IM_COL32(63, 185, 80, 255),   // green
        IM_COL32(219, 97, 162, 255),  // pink
        IM_COL32(227, 170, 52, 255),  // amber
    };
    // Known categories get fixed colours, the rest a stable hashed one.
    if (category == "Graphics") return colors[0];
    if (category == "Compute") return colors[1];
    if (category == "Imaging") return colors[2];
    if (category == "Benchmarks") return colors[3];
    return colors[std::hash<std::string_view>{}(category) % colors.size()];
}

ImU32 withAlpha(ImU32 color, float alpha)
{
    ImVec4 v = ImGui::ColorConvertU32ToFloat4(color);
    v.w *= alpha;
    return ImGui::ColorConvertFloat4ToU32(v);
}

ImU32 mix(ImU32 a, ImU32 b, float t)
{
    const ImVec4 x = ImGui::ColorConvertU32ToFloat4(a);
    const ImVec4 y = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t));
}

float fontPx(const ImFont* font)
{
    return font->FontSize * ImGui::GetIO().FontGlobalScale;
}
