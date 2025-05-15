#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 aColor;

out vec3 FragPos_World;
out vec3 Normal_World;    // Nadal przekazujemy interpolowaną normalną dla trybu smooth
out vec2 TexCoords;
out vec4 VertexColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
// uniform bool u_UseFlatShading; // Nie jest potrzebny w vertex shaderze przy metodzie dFdx/dFdy

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    FragPos_World = vec3(model * vec4(aPos, 1.0));
    Normal_World = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;

    if (length(aColor) > 0.001) {
         VertexColor = aColor;
    } else {
         VertexColor = vec4(1.0, 1.0, 1.0, 1.0);
    }
}
