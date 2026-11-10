#pragma once

#include <vector>
#include <span>

#include "vertex.hpp"


class Mesh
{
public:

    Mesh() = default;

    static Mesh sphere(int stacks, int slices);

    std::span<const Vertex> vertices() const noexcept;

    std::span<const unsigned int> indices() const noexcept;

    bool isEmpty() const noexcept;

private:

    std::vector<Vertex> vertices_;

    std::vector<unsigned int> indices_;
};