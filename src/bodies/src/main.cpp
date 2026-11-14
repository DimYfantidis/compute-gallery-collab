#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <bodies/body_processing.hpp>
#include <bodies/mesh.hpp>
#include <bodies/shaders.hpp>
#include <bodies/textures.hpp>
#include <bodies/vertex.hpp>

int main()
{
    // Resource directory.
    const std::filesystem::path DATA_DIR = DATA_DIRECTORY_PATH;

    // There must be no padding when transferring data to the GPU.
    static_assert(std::is_standard_layout_v<Vertex>);

    // Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return EXIT_FAILURE;
    }

    // Request OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(1920, 1080, "OpenGL Sphere", nullptr, nullptr);

    if (!window)
    {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);

    // Load OpenGL functions using GLAD 2
    if (!gladLoadGL(glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // Enable vertical synchronization
    glfwSwapInterval(1);

    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n';

    // -------------------------------------------------------
    // Create sphere mesh
    // -------------------------------------------------------

    const Mesh sphere = Mesh::sphere(32, 64);

    const auto vertices = sphere.vertices();
    const auto indices = sphere.indices();

    std::cout << "Vertices: " << vertices.size() << '\n';
    std::cout << "Indices: " << indices.size() << '\n';
    std::cout << "Triangles: " << indices.size() / 3 << '\n';

    // -------------------------------------------------------
    // Compile shaders
    // -------------------------------------------------------

    GLuint vertexShader = compileShaderFromFile(GL_VERTEX_SHADER, DATA_DIR / "shaders" / "sphere_vertex.glsl");

    GLuint fragmentShader = compileShaderFromFile(GL_FRAGMENT_SHADER, DATA_DIR / "shaders" / "fragment.glsl");

    if (!vertexShader || !fragmentShader)
    {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // -------------------------------------------------------
    // Create shader program
    // -------------------------------------------------------

    GLuint shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint linked = GL_FALSE;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linked);

    // Individual shaders are no longer needed
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!linked)
    {
        char log[1024];

        glGetProgramInfoLog(shaderProgram, sizeof(log), nullptr, log);

        std::cerr << "Shader linking failed:\n" << log << '\n';

        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // -------------------------------------------------------
    // Create VAO, VBO and EBO
    // -------------------------------------------------------

    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // Bind VAO first
    glBindVertexArray(VAO);

    // -------------------------------------------------------
    // Upload vertex data to VBO
    // -------------------------------------------------------

    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size_bytes()), vertices.data(), GL_STATIC_DRAW);

    // -------------------------------------------------------
    // Upload index data to EBO
    // -------------------------------------------------------

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size_bytes()), indices.data(),
                 GL_STATIC_DRAW);

    // -------------------------------------------------------
    // Configure vertex attributes
    // -------------------------------------------------------

    // Position: layout(location = 0) in vec3
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, position)));

    glEnableVertexAttribArray(0);

    // Normal: layout(location = 1) in vec3
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, normal)));

    glEnableVertexAttribArray(1);

    // Texture coordinates: layout(location = 2) in vec2
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, texCoord)));

    glEnableVertexAttribArray(2);

    // Unbind VAO
    glBindVertexArray(0);

    // Unbind VBO (optional)
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // IMPORTANT:
    // Do not unbind GL_ELEMENT_ARRAY_BUFFER while VAO is bound.
    // The VAO remembers its associated EBO.

    // -------------------------------------------------------
    // Load the Earth's texture
    // -------------------------------------------------------

    GLuint ceresTexture = loadTexture(DATA_DIR / "textures" / "2k_ceres_fictional.jpg");

    constexpr size_t NUM_BODIES = 1'000;
    std::vector<SphereData> bodyData = initBodies({.num_bodies = NUM_BODIES,
                                                   .radius_range = {.min = 0.4f, .max = 2.0f},
                                                   .distance_range = {.min = 20.0f, .max = 400.0f},
                                                   .mass_range = {.min = 480.0f, .max = 500.0f}});

    // Camera parameters
    double u = glm::pi<double>() / 4;
    double v = glm::pi<double>() / 3;
    double r = 5.0;

    // Camera matrix
    glm::mat4 view;

    // GPU draw buffer data.
    constexpr size_t NUM_OBJECTS_PER_DRAW = 500;

    // Get uniform locations
    const GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    const GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    const std::vector<GLint> positionLoc = std::invoke([shaderProgram]() {
        std::vector<GLint> result(NUM_OBJECTS_PER_DRAW);
        for (size_t i = 0; i < NUM_OBJECTS_PER_DRAW; ++i)
        {
            std::string name = std::format("positions[{}]", i);
            result[i] = glGetUniformLocation(shaderProgram, name.c_str());
        }
        return result;
    });
    const std::vector<GLint> radiusLoc = std::invoke([shaderProgram]() {
        std::vector<GLint> result(NUM_OBJECTS_PER_DRAW);
        for (size_t i = 0; i < NUM_OBJECTS_PER_DRAW; ++i)
        {
            std::string name = std::format("radii[{}]", i);
            result[i] = glGetUniformLocation(shaderProgram, name.c_str());
        }
        return result;
    });

    // -------------------------------------------------------
    // Main rendering loop
    // -------------------------------------------------------

    const GLsizei indexCount = static_cast<GLsizei>(indices.size());

    std::chrono::steady_clock::time_point ts_current = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point ts_previous = std::chrono::steady_clock::now();
    double sim_speed_factor = 1000.0;

    while (!glfwWindowShouldClose(window))
    {
        ts_current = std::chrono::steady_clock::now();
        double dt_sec =
            (double)std::chrono::duration_cast<std::chrono::nanoseconds>(ts_current - ts_previous).count() * 1e-9;
        ts_previous = ts_current;

        // Update viewport (projection matrix)
        int width = 0;
        int height = 0;

        glfwGetFramebufferSize(window, &width, &height);

        if (width == 0 || height == 0)
        {
            glfwPollEvents();
            continue;
        }

        glViewport(0, 0, width, height);

        // Update projection matrix
        const float aspect = static_cast<float>(width) / static_cast<float>(height);

        const glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 2000.0f);

        // Update Camera (view matrix)
        constexpr double SPEED_RADIANS_PER_SEC = glm::pi<double>();
        constexpr double SPEED_DISTANCE_PER_SEC = 80.0;

        // Close on Escape
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            u += SPEED_RADIANS_PER_SEC * dt_sec;
            u = (u > 2 * glm::pi<double>() ? u - 2 * glm::pi<double>() : u);
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            u -= SPEED_RADIANS_PER_SEC * dt_sec;
            u = (u < 0 ? u + 2 * glm::pi<double>() : u);
        }
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            v -= SPEED_RADIANS_PER_SEC * dt_sec;
            v = (v < 0.01 * glm::pi<double>() ? 0.01 : v);
        }
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            v += SPEED_RADIANS_PER_SEC * dt_sec;
            v = (v > 0.99 * glm::pi<double>() ? 0.99 * glm::pi<double>() : v);
        }
        if (glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
        {
            r -= SPEED_DISTANCE_PER_SEC * dt_sec;
            r = (r < 1.0 ? 1.0 : r);
        }
        if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
        {
            r += SPEED_DISTANCE_PER_SEC * dt_sec;
        }
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
        {
            sim_speed_factor *= 1.02;
            std::cout << "Simulation speed: x" << sim_speed_factor << '\n';
        }
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
        {
            sim_speed_factor /= 1.02;
            sim_speed_factor = (sim_speed_factor < 1000.0 ? 1000.0 : sim_speed_factor);
            std::cout << "Simulation speed: x" << sim_speed_factor << '\n';
        }

        {
            auto cam_x = (float)(r * std::sin(v) * std::cos(u));
            auto cam_y = (float)(r * std::cos(v));
            auto cam_z = (float)(r * std::sin(v) * std::sin(u));
            view =
                glm::lookAt(glm::vec3(cam_x, cam_y, cam_z), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        }
        updateBodies(bodyData, dt_sec, sim_speed_factor);

        // Clear color and depth buffers
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Activate shader program
        glUseProgram(shaderProgram);

        // Texture unit 0
        glUniform1i(glGetUniformLocation(shaderProgram, "diffuseTexture"), 0);
        // Lighting
        glUniform3f(glGetUniformLocation(shaderProgram, "lightPos"), 0.0f, 0.0f, 0.0f);
        glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), 1.0f, 1.0f, 1.0f);

        // Upload view matrix
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // Upload projection matrix
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ceresTexture);

        // Bind sphere VAO
        glBindVertexArray(VAO);

        constexpr size_t NUM_LEFTOVER = NUM_BODIES % NUM_OBJECTS_PER_DRAW;
        constexpr size_t NUM_GPU_CALLS = NUM_BODIES / NUM_OBJECTS_PER_DRAW + (NUM_LEFTOVER == 0 ? 0 : 1);

        // Draw all spheres
        for (size_t start = 0; start < NUM_BODIES; start += NUM_OBJECTS_PER_DRAW)
        {
            const size_t batch_size = std::min(NUM_OBJECTS_PER_DRAW, NUM_BODIES - start);
            for (size_t j = 0; j < batch_size; ++j)
            {
                const size_t index = start + j;
                const glm::vec3& position = bodyData[index].position;
                const float radius = bodyData[index].radius;
                glUniform3fv(positionLoc[j], 1, glm::value_ptr(position));
                glUniform1fv(radiusLoc[j], 1, &radius);
            }
            glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr,
                                    static_cast<GLsizei>(batch_size));
        }
        // Present frame
        glfwSwapBuffers(window);
        // Handle input and window events
        glfwPollEvents();
    }

    // -------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    glDeleteTextures(1, &ceresTexture);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}