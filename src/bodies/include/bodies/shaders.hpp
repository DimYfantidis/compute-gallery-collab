#pragma once

#include <filesystem>
#include <glad/gl.h>
#include <string>

#include <common/status.hpp>

namespace bodies
{

// Compile a shader and check for errors
Status<GLuint> compileShaderFromSource(GLenum type, const std::string& source);

// Compile a shader and check for errors
Status<GLuint> compileShaderFromFile(GLenum type, const std::filesystem::path& filepath);

} // namespace bodies