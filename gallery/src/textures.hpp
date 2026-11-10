#pragma once

#include <filesystem>
#include <vector>

#include <glad/gl.h>

struct Texture
{
    GLuint id = 0;
    int width = 0;
    int height = 0;
    explicit operator bool() const noexcept { return id != 0; }
};

struct Image
{
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels; // RGBA, empty on failure
};

// Loads a PNG/JPEG as RGBA pixels in memory.
Image loadImage(const std::filesystem::path& path);

// Loads a PNG/JPEG into an RGBA texture; returns an empty Texture on failure.
Texture loadTexture(const std::filesystem::path& path);

void destroyTexture(Texture& texture);

// Saves the current default framebuffer (width x height pixels) as a PNG.
bool saveFramebufferPng(const std::filesystem::path& path, int width, int height);
