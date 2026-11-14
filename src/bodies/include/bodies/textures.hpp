#pragma once

#include <filesystem>
#include <glad/gl.h>

GLuint loadTexture(const std::filesystem::path& filename);
