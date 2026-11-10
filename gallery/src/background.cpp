#include "background.hpp"

#include <cstdio>

namespace
{
const char* VERTEX = R"(#version 330 core
// One triangle that covers the screen; no vertex buffer needed.
void main()
{
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

const char* FRAGMENT = R"(#version 330 core
out vec4 frag;
uniform vec2 resolution;
uniform float time;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

float noise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

float fbm(vec2 p)
{
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; ++i) { v += a * noise(p); p *= 2.03; a *= 0.5; }
    return v;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / resolution;
    vec2 p = (gl_FragCoord.xy - 0.5 * resolution) / resolution.y;

    vec3 color = mix(vec3(0.027, 0.035, 0.055), vec3(0.040, 0.050, 0.078), uv.y);

    // Two layers of nebula, cyan and violet, drifting very slowly.
    float t = time * 0.015;
    float a = fbm(p * 1.5 + vec2(t, -0.6 * t));
    float b = fbm(p * 2.2 - vec2(0.7 * t, 0.3 * t) + 4.0);
    vec3 nebula = vec3(0.21, 0.76, 0.88) * pow(a, 3.0) * 0.30 + vec3(0.55, 0.49, 0.96) * pow(b, 3.0) * 0.24;
    nebula *= smoothstep(1.4, 0.1, length(p - vec2(0.55, 0.30))); // brightest towards the top right
    color += nebula;

    // Sparse stars on a 3 px grid, each twinkling at its own rate.
    vec2 cell = floor(gl_FragCoord.xy / 3.0);
    float h = hash(cell);
    float star = step(0.9991, h) * (0.6 + 0.4 * sin(time * (0.6 + 2.0 * fract(h * 91.0)) + h * 50.0));
    color += vec3(star) * 0.38;

    frag = vec4(color, 1.0);
}
)";

GLuint compile(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "background shader: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
} // namespace

Background::Background()
{
    const GLuint vs = compile(GL_VERTEX_SHADER, VERTEX);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, FRAGMENT);
    if (vs && fs)
    {
        program_ = glCreateProgram();
        glAttachShader(program_, vs);
        glAttachShader(program_, fs);
        glLinkProgram(program_);
        GLint ok = GL_FALSE;
        glGetProgramiv(program_, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            glDeleteProgram(program_);
            program_ = 0;
        }
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (program_)
    {
        resolution_loc_ = glGetUniformLocation(program_, "resolution");
        time_loc_ = glGetUniformLocation(program_, "time");
    }
    glGenVertexArrays(1, &vao_);
}

Background::~Background()
{
    if (cache_fbo_)
        glDeleteFramebuffers(1, &cache_fbo_);
    if (cache_tex_)
        glDeleteTextures(1, &cache_tex_);
    glDeleteVertexArrays(1, &vao_);
    if (program_)
        glDeleteProgram(program_);
}

void Background::render(int width, int height, float time)
{
    glUseProgram(program_);
    glUniform2f(resolution_loc_, static_cast<float>(width), static_cast<float>(height));
    glUniform1f(time_loc_, time);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glUseProgram(0);
}

void Background::draw(int width, int height, float time, bool animated)
{
    glViewport(0, 0, width, height);
    if (!program_)
    {
        glClearColor(0.031f, 0.039f, 0.059f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    if (animated)
    {
        render(width, height, time);
        return;
    }

    // Still: render once per size into a texture, then just copy it.
    if (!cache_fbo_ || width != cache_w_ || height != cache_h_)
    {
        if (!cache_fbo_)
        {
            glGenFramebuffers(1, &cache_fbo_);
            glGenTextures(1, &cache_tex_);
        }
        glBindTexture(GL_TEXTURE_2D, cache_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, cache_fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cache_tex_, 0);
        render(width, height, time);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        cache_w_ = width;
        cache_h_ = height;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, cache_fbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
