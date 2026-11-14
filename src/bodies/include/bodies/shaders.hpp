#pragma once

#include <filesystem>
#include <format>
#include <fstream>
#include <glad/gl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// Compile a shader and check for errors
inline GLuint compileShaderFromSource(GLenum type, const std::string& source)
{
    const char* src_ptr = source.c_str();
    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &src_ptr, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compilation failed:\n" << log << '\n';
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Compile a shader and check for errors
inline GLuint compileShaderFromFile(GLenum type, const std::filesystem::path& filepath)
{
    std::ifstream ifs(filepath, std::ios::in);

    if (!ifs.is_open())
        throw std::runtime_error(std::format("File '{}' not found! {}: {}", filepath.string(), __FILE__, __LINE__));

    std::stringstream text_buffer;
    text_buffer << ifs.rdbuf();
    return compileShaderFromSource(type, text_buffer.str());
}