#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace bodies
{
    
struct SphereData
{
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f, 0.0f, 0.0f};
    float radius{0.0f};
    float mass{0.0f};
};

struct NumericRange
{
    float min;
    float max;
};

/** @brief @ref initBodies input parameters. */
struct InitBodiesServiceRequest
{
    /** @brief The size of the list. */
    size_t num_bodies;

    /** @brief The minimum and maximum possible radius a celestial body may have. */
    NumericRange radius_range;

    /** @brief The minimum and maximum possible distance a celestial body's center may
     *  have from the center of the galaxy (i.e. the origin of the models).
     */
    NumericRange distance_range;

    /** @brief The minimum and maximum possible mass a celestial body may have. */
    NumericRange mass_range;
};

/**
 *  @brief Get a list of randomly initialized celestial bodies, whose randomness is restrained within
 *  the specified bounds.
 *  @note None of the generated spheres are in collision with each other.
 *  @note All bodies start as immobile.
 *  @param[in] request See @ref InitBodiesServiceRequest
 *  @returns The list of celestial bodies that satisfy the following properties.
 */
std::vector<SphereData> initBodies(const InitBodiesServiceRequest& request);

/**
 *  @brief Updates the bodies' physical state within the main display loop.
 *  @param[in, out] bodies The collection of bodies to update.
 *  @param[in] dt_sec The time elapsed for an iteration of the display loop. Should be dynamic.
 *  @param[in] sim_speed_factor Scale factor for @ref dt_sec.
 */
void updateBodies(std::span<SphereData> bodies, double dt_sec, double sim_speed_factor);

} // namespace bodies