#pragma once

#include <glad/gl.h>
#include <filesystem>

GLuint loadTexture(const std::filesystem::path& filename);
