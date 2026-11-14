#include <bodies/textures.hpp>
#include <iostream>
#include <stb/stb_image.h>

GLuint loadTexture(const std::filesystem::path& filepath)
{
    int width, height, channels;

    unsigned char* data = stbi_load(filepath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!data)
    {
        std::cerr << "Failed to load texture: '" << filepath.string() << "'\n";
        return 0;
    }
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Repeat horizontally (around the sphere).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);

    // Clamp vertically (at the poles).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    glBindTexture(GL_TEXTURE_2D, 0);

    return texture;
}