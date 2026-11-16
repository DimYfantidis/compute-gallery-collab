#pragma once

#include <filesystem>
#include <glad/gl.h>

#include <common/status.hpp>

namespace bodies
{

Status<GLuint> loadTexture(const std::filesystem::path& filepath);

}