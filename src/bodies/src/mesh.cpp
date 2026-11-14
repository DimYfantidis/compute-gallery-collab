#include <format>
#include <iostream>
#include <stdexcept>

#include <glm/gtc/constants.hpp>

#include <bodies/mesh.hpp>

Mesh Mesh::sphere(int stacks, int slices)
{
    Mesh mesh;

    if (stacks < 2 || slices < 3)
    {
        std::cerr << std::format("Expected stacks > 1, slices > 2: "
                                 "Received {{stacks: {}, slices, {} }} instead.",
                                 stacks, slices)
                  << '\n';
        return mesh;
    }

    mesh.vertices_.resize((stacks + 1) * (slices + 1));
    for (int i = 0; i <= stacks; ++i)
    {
        const float theta = glm::pi<float>() * static_cast<float>(i) / stacks;

        for (int j = 0; j <= slices; ++j)
        {
            const float phi = 2.0f * glm::pi<float>() * static_cast<float>(j) / slices;
            auto& vertex = mesh.vertices_[i * (slices + 1) + j];

            vertex.position.x = std::sin(theta) * std::cos(phi);
            vertex.position.y = std::cos(theta);
            vertex.position.z = std::sin(theta) * std::sin(phi);

            vertex.normal = glm::normalize(vertex.position);

            vertex.texCoord.x = static_cast<float>(j) / slices;
            vertex.texCoord.y = static_cast<float>(i) / stacks;
        }
    }

    mesh.indices_.resize(6 * stacks * slices);
    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            const int offset = 6 * (i * slices + j);

            unsigned int first = i * (slices + 1) + j;
            unsigned int second = first + slices + 1;

            mesh.indices_[offset + 0] = first;
            mesh.indices_[offset + 1] = first + 1;
            mesh.indices_[offset + 2] = second;

            mesh.indices_[offset + 3] = first + 1;
            mesh.indices_[offset + 4] = second + 1;
            mesh.indices_[offset + 5] = second;
        }
    }
    return mesh;
}

std::span<const Vertex> Mesh::vertices() const noexcept
{
    return vertices_;
}

std::span<const unsigned int> Mesh::indices() const noexcept
{
    return indices_;
}

bool Mesh::isEmpty() const noexcept
{
    return vertices_.empty();
}
