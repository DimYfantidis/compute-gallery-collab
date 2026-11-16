#include <bodies/textures.hpp>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <stb/stb_image.h>

static const std::shared_ptr<spdlog::logger> logger = spdlog::stdout_color_mt("bodies.texture.cpp");

namespace bodies
{

Status<GLuint> loadTexture(const std::filesystem::path& filepath)
{
    int width, height, channels;

    unsigned char* data = stbi_load(filepath.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!data)
    {
        std::string error_msg = std::format("Failed to load texture: '{}'", filepath.string());
        logger->warn(error_msg);
        return {
            .ok = false,
            .value = {},
            .message = std::move(error_msg),
        };
    }
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

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

    return {
        .ok = true,
        .value = texture_id,
        .message = "",
    };
}

} // namespace bodies