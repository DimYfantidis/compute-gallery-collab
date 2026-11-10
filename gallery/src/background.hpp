#pragma once

#include <glad/gl.h>

// The gallery's backdrop: a dark gradient with slowly drifting nebula and twinkling stars,
// drawn by a fragment shader. When not animated it is rendered once per window size into a
// texture and copied, so it costs next to nothing (e.g. on software rendering).
class Background
{
public:
    Background();
    ~Background();
    Background(const Background&) = delete;
    Background& operator=(const Background&) = delete;

    // Fills the current framebuffer (width x height pixels). `time` animates when `animated`.
    void draw(int width, int height, float time, bool animated);

private:
    void render(int width, int height, float time);

    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLint resolution_loc_ = -1;
    GLint time_loc_ = -1;
    GLuint cache_fbo_ = 0;
    GLuint cache_tex_ = 0;
    int cache_w_ = 0;
    int cache_h_ = 0;
};
