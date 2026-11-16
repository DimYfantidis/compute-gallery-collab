#include <filesystem>
#include <format>
#include <fstream>
#include <glad/gl.h>
#include <sstream>
#include <string>

#include <bodies/shaders.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

static const std::shared_ptr<spdlog::logger> logger = spdlog::stdout_color_mt("bodies.shaders");

namespace bodies
{

// Compile a shader and check for errors
Status<GLuint> compileShaderFromSource(GLenum type, const std::string& source)
{
    const char* src_ptr = source.c_str();
    GLuint shader_id = glCreateShader(type);

    glShaderSource(shader_id, 1, &src_ptr, nullptr);
    glCompileShader(shader_id);

    GLint success;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char log[1024];
        glGetShaderInfoLog(shader_id, sizeof(log), nullptr, log);
        std::string error_msg = std::format("Shader compilation failed: {}", log);
        logger->warn(error_msg);
        glDeleteShader(shader_id);
        return {
            .ok = false,
            .value = {},
            .message = std::move(error_msg),
        };
    }
    return {
        .ok = true,
        .value = shader_id,
        .message = "",
    };
}

// Compile a shader and check for errors
Status<GLuint> compileShaderFromFile(GLenum type, const std::filesystem::path& filepath)
{
    std::ifstream ifs(filepath, std::ios::in);

    if (!ifs.is_open())
    {
        std::string error_msg = std::format("File '{}' not found! {}: {}", filepath.string(), __FILE__, __LINE__);
        logger->warn(error_msg);
        return {
            .ok = false,
            .value = {},
            .message = std::move(error_msg),
        };
    }
    std::stringstream text_buffer;
    text_buffer << ifs.rdbuf();
    return compileShaderFromSource(type, text_buffer.str());
}

} // namespace bodies