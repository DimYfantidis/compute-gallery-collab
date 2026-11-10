#include "textures.hpp"

#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

Image loadImage(const std::filesystem::path& path)
{
    Image img;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.string().c_str(), &img.width, &img.height, &channels, STBI_rgb_alpha);
    if (!pixels)
        return {};
    img.pixels.assign(pixels, pixels + static_cast<size_t>(img.width) * img.height * 4);
    stbi_image_free(pixels);
    return img;
}

Texture loadTexture(const std::filesystem::path& path)
{
    Texture tex;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.string().c_str(), &tex.width, &tex.height, &channels, STBI_rgb_alpha);
    if (!pixels)
        return {};

    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);
    return tex;
}

void destroyTexture(Texture& texture)
{
    if (texture.id)
        glDeleteTextures(1, &texture.id);
    texture = {};
}

bool saveFramebufferPng(const std::filesystem::path& path, int width, int height)
{
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    stbi_flip_vertically_on_write(1);
    return stbi_write_png(path.string().c_str(), width, height, 4, pixels.data(), width * 4) != 0;
}
