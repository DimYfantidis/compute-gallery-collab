#pragma once

#include <filesystem>
#include <string_view>

#include <imgui.h>

// Fonts by role. Icons (icons.hpp) are merged into every font except mono and display.
struct Fonts
{
    ImFont* body = nullptr;     // Inter Regular 15
    ImFont* medium = nullptr;   // Inter Medium 15: labels, buttons
    ImFont* small = nullptr;    // Inter Medium 12.5: chips, captions
    ImFont* heading = nullptr;  // Inter SemiBold 18: section titles
    ImFont* title = nullptr;    // Inter Display Bold 30: page titles
    ImFont* display = nullptr;  // Inter Display Bold 46: hero titles
    ImFont* mono = nullptr;     // Cousine 13.5: console
};

namespace palette
{
constexpr ImU32 background = IM_COL32(8, 10, 15, 255);
constexpr ImU32 sidebar = IM_COL32(12, 15, 22, 255);
constexpr ImU32 surface = IM_COL32(17, 21, 30, 222);         // panels: dark glass over the backdrop
constexpr ImU32 surface_hover = IM_COL32(24, 29, 40, 230);
constexpr ImU32 surface_solid = IM_COL32(19, 23, 32, 255);   // popups, cards over images
constexpr ImU32 border = IM_COL32(255, 255, 255, 20);
constexpr ImU32 border_strong = IM_COL32(255, 255, 255, 38);
constexpr ImU32 text = IM_COL32(234, 238, 245, 255);
constexpr ImU32 muted = IM_COL32(141, 151, 168, 255);
constexpr ImU32 faint = IM_COL32(94, 104, 122, 255);
constexpr ImU32 accent = IM_COL32(54, 194, 224, 255);       // cyan
constexpr ImU32 accent_2 = IM_COL32(139, 124, 246, 255);    // violet, end of the accent gradient
constexpr ImU32 accent_hover = IM_COL32(110, 214, 238, 255);
constexpr ImU32 success = IM_COL32(63, 185, 80, 255);
constexpr ImU32 danger = IM_COL32(248, 81, 73, 255);
constexpr ImU32 warning = IM_COL32(227, 170, 52, 255);
constexpr ImU32 on_accent = IM_COL32(6, 12, 18, 255);       // text on accent buttons
} // namespace palette

// Loads the fonts from `font_dir` (falls back to ImGui's built-in font for any missing file),
// rasterised at `raster_scale` times their nominal size for high-DPI screens.
Fonts loadFonts(const std::filesystem::path& font_dir, float raster_scale);

void applyTheme(float ui_scale);

// Stable colour per category so a category always looks the same.
ImU32 categoryColor(std::string_view category);

ImU32 withAlpha(ImU32 color, float alpha);
ImU32 mix(ImU32 a, ImU32 b, float t);

// Pixel size of `font` as drawn, for ImDrawList::AddText.
float fontPx(const ImFont* font);
