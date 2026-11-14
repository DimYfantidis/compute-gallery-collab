#include <bodies/body_processing.hpp>
#include <memory_resource>

#include <format>
#include <iostream>

static std::mt19937& getGenerator()
{
    static thread_local std::mt19937 generator(std::random_device{}());
    return generator;
}

static std::vector<std::byte>& getMemoryBuffer()
{
    static std::vector<std::byte> mem_buffer;
    return mem_buffer;
}

static bool detectSphereCollision(const glm::vec3& p1, float r1, const glm::vec3& p2, float r2)
{
    const glm::vec3 diff = p1 - p2;
    const float radiusSum = r1 + r2;
    return glm::dot(diff, diff) <= radiusSum * radiusSum;
}

std::vector<SphereData> initBodies(const InitBodiesServiceRequest& request)
{
    const size_t num_bodies = request.num_bodies;
    const auto& [min_radius, max_radius] = request.radius_range;
    const auto& [min_distance, max_distance] = request.distance_range;
    const auto& [min_mass, max_mass] = request.mass_range;

    auto& generator = getGenerator();
    // Random non-uniform distance in [d_min, d_max].
    std::piecewise_linear_distribution<float> distanceDist({min_distance, max_distance},
                                                           [min_distance](float x) { return x - min_distance; });
    // Random rotation angles between -PI and PI.
    std::uniform_real_distribution<float> rotDist(-glm::pi<float>(), glm::pi<float>());
    // Random body radius in [r_min, r_max].
    std::uniform_real_distribution<float> radiusDist(min_radius, max_radius);
    // Random body mass in [m_min, m_max].
    std::uniform_real_distribution<float> massDist(min_mass, max_mass);

    std::vector<SphereData> bodies;
    bodies.reserve(num_bodies);
    std::multimap<float, size_t> bodiesByDistance;

    while (bodies.size() < num_bodies)
    {
        // Generate random distance from the center of the galaxy.
        float distance = distanceDist(generator);
        // Generate random Euler angles (radians)
        glm::vec3 rotation(rotDist(generator), rotDist(generator), rotDist(generator));
        // Rotation matrix
        glm::mat4 R(1.0f);
        R = glm::rotate(R, rotation.x, glm::vec3(1, 0, 0));
        R = glm::rotate(R, rotation.y, glm::vec3(0, 1, 0));
        R = glm::rotate(R, rotation.z, glm::vec3(0, 0, 1));
        // Translation vector
        glm::vec3 pos = glm::vec3(R * glm::vec4(distance, 0.0f, 0.0f, 1.0f));
        // Body radius
        float radius = radiusDist(generator);
        // Body mass
        float mass = massDist(generator);

        /*
         *  For a new sphere at distance d with radius r, you only need to examine existing spheres
         *  whose distances from the origin are within [d-(r+r_max), d+(r+r_max)]. Everything outside
         *  this interval is guaranteed not to collide.
         */

        // Determine the search interval.
        float d = glm::length(pos);
        float range = radius + max_radius;
        bool collides = false;
        // Find the first potentially colliding sphere.
        auto it = bodiesByDistance.lower_bound(d - range);
        // Examine only spheres within the radial interval.
        while (it != bodiesByDistance.end() && it->first <= d + range)
        {
            size_t j = it->second;
            if (detectSphereCollision(pos, radius, bodies[j].position, bodies[j].radius))
            {
                collides = true;
                break;
            }
            ++it;
        }
        if (collides)
            continue;
        // Accept the new sphere.
        size_t index = bodies.size();
        bodies.push_back({
            .position = pos,
            .velocity = glm::vec3(0.0f, 0.0f, 0.0f),
            .radius = radius,
            .mass = mass,
        });
        bodiesByDistance.emplace(d, index);
    }
    return bodies;
}

void updateBodies(std::span<SphereData> bodies, double dt_sec, double sim_speed_factor)
{
    constexpr float G = 6.67430e-11f;

    const size_t num_bodies = bodies.size();
    std::vector<std::byte>& buffer = getMemoryBuffer();
    if (buffer.size() != num_bodies)
    {
        buffer.resize(num_bodies);
    }
    std::pmr::monotonic_buffer_resource pool{buffer.data(), buffer.size(), std::pmr::get_default_resource()};
    std::pmr::vector<glm::vec3> accelerations{num_bodies, glm::vec3(0.0f), &pool};
    for (size_t i = 0; i < num_bodies; ++i)
    {
        const glm::vec3& qi = bodies[i].position;
        for (size_t j = 0; j < num_bodies; ++j)
        {
            if (i == j)
                continue;
            const float mj = bodies[j].mass;
            const glm::vec3& qj = bodies[j].position;
            const glm::vec3 diff = qj - qi;
            const float dot = glm::dot(diff, diff);
            if (std::abs(dot) > .0001f)
                accelerations[i] += (G * mj * diff) / (dot * std::sqrt(dot));
        }
    }
    const float dt = static_cast<float>(sim_speed_factor * dt_sec);

    for (size_t i = 0; i < num_bodies; ++i)
    {
        bodies[i].velocity += accelerations[i] * dt;
        bodies[i].position += bodies[i].velocity * dt;
    }
}