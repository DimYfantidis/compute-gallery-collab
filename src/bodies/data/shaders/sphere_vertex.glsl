#version 330 core

// Vertex attributes
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

// Transformation matrices
uniform mat4 view;
uniform mat4 projection;

// Sphere data
uniform vec3 positions[500];
uniform float radii[500];

// Outputs to fragment shader
out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main()
{
    vec3 position = positions[gl_InstanceID];
    float radius = radii[gl_InstanceID];

    // Position in world space
    vec3 worldPos = aPosition * radius + position;

    FragPos = worldPos;
    Normal = aNormal;

    // Pass texture coordinates
    TexCoord = aTexCoord;

    // Final vertex position
    gl_Position = projection * view * vec4(worldPos, 1.0);
}