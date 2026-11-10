#pragma once

#include <imgui.h>

struct Fonts;

// The Compute Gallery mark: a glowing core with two bodies on a tilted orbit, drawn with
// vector shapes so it is sharp at any size. `t` (seconds) moves the bodies; pass a constant
// for a still mark. `size` is the width of the square it fits in.
void drawLogoMark(ImDrawList* dl, ImVec2 center, float size, float t);

// Mark followed by the "Compute Gallery" wordmark, top-left at `pos`; returns its size.
ImVec2 drawLogo(ImDrawList* dl, ImVec2 pos, float mark_size, const Fonts& fonts, float t);
