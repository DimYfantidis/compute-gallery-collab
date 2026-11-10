#pragma once

#include <string>

#include <imgui.h>

#include "theme.hpp"

// Custom-drawn controls that give the gallery its look; everything else is plain Dear ImGui.
namespace ui
{
// Eases towards `target`, per widget id, for hover and press animations.
float animate(ImGuiID id, float target, float speed = 14.0f);

// True while any animate() value is still moving: the main loop then keeps drawing frames.
bool animating();
void beginFrame();
// Asks for smooth frames this frame (e.g. during a page transition).
void keepAnimating();

// Rounded rectangle whose colour, alpha included, goes from c0 to c1 (left to right, or top
// to bottom when `vertical`).
void gradient(ImDrawList* dl, ImVec2 p0, ImVec2 p1, ImU32 c0, ImU32 c1, bool vertical, float rounding = 0.0f,
              ImDrawFlags corners = 0);

// Shortens `text` with "..." to fit `max_width` in the current font.
std::string ellipsize(std::string text, float max_width);

// Accent-gradient button for the main action of a page. `label` may start with an icon.
bool primaryButton(const char* label, ImVec2 size, bool enabled = true);
bool dangerButton(const char* label, ImVec2 size = ImVec2(0, 0));
// Quiet button: translucent until hovered.
bool ghostButton(const char* label, ImVec2 size = ImVec2(0, 0));
// Square icon-only button with a tooltip.
bool iconButton(const char* icon, const char* tooltip, float size = 0.0f);

// A small rounded label drawn at `pos`; returns its width.
float chip(ImDrawList* dl, const Fonts& fonts, ImVec2 pos, const char* text, ImU32 bg, ImU32 fg);
// The same as an inline item (use SameLine between them).
void tag(const Fonts& fonts, const char* text, ImU32 color);

// Keys drawn as keycaps: "W A S D / arrows" gives four keys, a slash and the word "arrows".
void keys(const Fonts& fonts, const std::string& text);

// A rounded, translucent panel around the items between begin and end.
void beginCard(const char* id, float width = 0.0f, float padding = 18.0f);
void endCard();

void sectionTitle(const Fonts& fonts, const char* icon, const char* text);

// On/off switch; returns true when toggled.
bool toggle(const char* id, bool* value);

// Coloured dot, a title and a wrapped explanation.
void statusRow(const Fonts& fonts, ImU32 color, const char* title, const std::string& detail);

// Draws `texture` cropped to fill p0..p1 (like CSS object-fit: cover), optionally zoomed.
void imageCover(ImDrawList* dl, ImTextureID texture, int tex_w, int tex_h, ImVec2 p0, ImVec2 p1, float rounding,
                ImDrawFlags corners, float zoom = 1.0f, ImU32 tint = IM_COL32_WHITE);

// Soft shadow under a rounded rectangle.
void shadow(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float rounding, float strength = 1.0f);

// Wrapped paragraph in the muted colour.
void paragraph(const std::string& text);
} // namespace ui
