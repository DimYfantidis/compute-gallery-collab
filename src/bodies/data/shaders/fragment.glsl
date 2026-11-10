#version 330 core

// Inputs from vertex shader
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

// Output
out vec4 FragColor;

// Texture
uniform sampler2D diffuseTexture;

// Lighting
uniform vec3 lightPos;
uniform vec3 lightColor;

void main()
{
    // Sample texture
    vec4 texColor = texture(diffuseTexture, TexCoord);

    // Normalize interpolated normal
    vec3 norm = normalize(Normal);

    // Direction towards light
    vec3 lightDir = normalize(lightPos - FragPos);

    // Ambient lighting
    float ambientStrength = 0.15;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse lighting
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Final color
    vec3 result = (ambient + diffuse) * texColor.rgb;

    FragColor = vec4(result, texColor.a);
}