#include "logo.hpp"

#include <algorithm>
#include <cmath>

#include "theme.hpp"

namespace
{
constexpr float PI = 3.14159265f;
constexpr float TILT = -28.0f * PI / 180.0f; // orbit tilt, as in the icon

ImVec2 orbitPoint(ImVec2 c, float rx, float ry, float theta)
{
    const float x = rx * std::cos(theta), y = ry * std::sin(theta);
    return ImVec2(c.x + x * std::cos(TILT) - y * std::sin(TILT), c.y + x * std::sin(TILT) + y * std::cos(TILT));
}

// Half of the orbit: the far side (behind the core) or the near side.
void drawOrbitHalf(ImDrawList* dl, ImVec2 c, float rx, float ry, float thickness, bool far_side)
{
    constexpr int SEGMENTS = 40;
    const float start = far_side ? PI : 0.0f;
    ImVec2 prev = orbitPoint(c, rx, ry, start);
    for (int i = 1; i <= SEGMENTS; ++i)
    {
        const float theta = start + PI * i / SEGMENTS;
        const ImVec2 p = orbitPoint(c, rx, ry, theta);
        // Cyan on the left fading to violet on the right; the far side is dimmer.
        const float along = 0.5f + 0.5f * std::cos(theta);
        const ImU32 col = withAlpha(mix(palette::accent_2, palette::accent, along), far_side ? 0.45f : 0.9f);
        dl->AddLine(prev, p, col, thickness);
        prev = p;
    }
}

// A shaded sphere: layered discs that brighten towards the light (top-left).
void drawSphere(ImDrawList* dl, ImVec2 c, float r, ImU32 dark, ImU32 light)
{
    constexpr int LAYERS = 12;
    for (int k = 0; k < LAYERS; ++k)
    {
        const float t = static_cast<float>(k) / (LAYERS - 1);
        const float radius = r * (1.0f - 0.82f * t);
        const ImVec2 offset(-r * 0.32f * t, -r * 0.36f * t);
        dl->AddCircleFilled(ImVec2(c.x + offset.x, c.y + offset.y), radius, mix(dark, light, t * t), 32);
    }
}

struct Body
{
    float theta;
    float radius;
    ImU32 dark, light;
};
} // namespace

void drawLogoMark(ImDrawList* dl, ImVec2 c, float size, float t)
{
    const float half = size * 0.5f;
    const float core = half * 0.36f;
    const float rx = half * 0.86f, ry = half * 0.32f;
    const float thickness = std::max(1.5f, size * 0.035f);

    // Soft glow behind the core.
    for (int i = 6; i >= 1; --i)
        dl->AddCircleFilled(c, core * (1.0f + 0.32f * i), withAlpha(palette::accent, 0.035f), 40);

    const float phase = t * 0.55f;
    const Body bodies[] = {
        {2.75f + phase, half * 0.17f, IM_COL32(138, 62, 16, 255), IM_COL32(255, 210, 174, 255)},
        {2.75f + phase + PI + 0.85f, half * 0.09f, IM_COL32(70, 160, 190, 255), IM_COL32(225, 250, 255, 255)},
    };
    auto drawBodies = [&](bool far_side) {
        for (const Body& b : bodies)
            if ((std::sin(b.theta) < 0.0f) == far_side)
                drawSphere(dl, orbitPoint(c, rx, ry, b.theta), b.radius, b.dark, b.light);
    };

    drawOrbitHalf(dl, c, rx, ry, thickness, true);
    drawBodies(true);
    drawSphere(dl, c, core, IM_COL32(15, 90, 115, 255), IM_COL32(216, 247, 255, 255));
    drawOrbitHalf(dl, c, rx, ry, thickness, false);
    drawBodies(false);
}

ImVec2 drawLogo(ImDrawList* dl, ImVec2 pos, float mark_size, const Fonts& fonts, float t)
{
    drawLogoMark(dl, ImVec2(pos.x + mark_size * 0.5f, pos.y + mark_size * 0.5f), mark_size, t);

    const float name_px = fontPx(fonts.heading);
    const float tag_px = fontPx(fonts.small);
    const float text_h = name_px + tag_px * 1.15f;
    const ImVec2 text_pos(pos.x + mark_size * 1.12f, pos.y + (mark_size - text_h) * 0.5f);
    dl->AddText(fonts.heading, name_px, text_pos, palette::text, "Compute Gallery");
    dl->AddText(fonts.small, tag_px, ImVec2(text_pos.x, text_pos.y + name_px * 1.08f), palette::muted,
                "HPC exhibition");
    const float name_w = fonts.heading->CalcTextSizeA(name_px, FLT_MAX, 0, "Compute Gallery").x;
    return ImVec2(mark_size * 1.12f + name_w, mark_size);
}
