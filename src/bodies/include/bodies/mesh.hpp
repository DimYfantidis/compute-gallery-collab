#pragma once

#include <span>
#include <vector>

#include "vertex.hpp"

namespace bodies
{
    
/** @class Mesh
 *  @brief Represents a set if triangular meshes, encapsulating all necessary VAO and EBO buffer elements.
 */
class Mesh
{
public:
    /** @brief Factory method for spherical meshes.
     *  @param[in] stacks Number of meridians.
     *  @param[in] slices Number of parallels.
     *  @returns A UV-generated sphere mesh.
     */
    static Mesh sphere(int stacks, int slices);

    /** @returns Read-only view of the ordered collection of vertices. */
    std::span<const Vertex> vertices() const noexcept;

    /** @returns Read-only view of the ordered collection of indices. */
    std::span<const unsigned int> indices() const noexcept;

    /** @returns True if the object has been initialized, false otherwise. */
    bool isEmpty() const noexcept;

private:
    std::vector<Vertex> vertices_;

    std::vector<unsigned int> indices_;
};

}