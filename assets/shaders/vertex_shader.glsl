#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 aColor;

// Wyjścia do fragment shadera
out vec3 FragPos_World;
out vec3 Normal_World;
out vec2 TexCoords;
out vec4 VertexColor_FS;
out vec4 FragPosDirLightSpace; // Dla światła kierunkowego

// NOWE: Tablica pozycji fragmentu w przestrzeniach świateł reflektorów
// Rozmiar tablicy musi odpowiadać MAX_SHADOW_CASTING_SPOT_LIGHTS w C++ i FS
// Jeśli MAX_SHADOW_CASTING_SPOT_LIGHTS = 2
out vec4 FragPosSpotLightSpace[2];


uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform mat4 dirLightSpaceMatrix; // Dla światła kierunkowego

// NOWE: Tablica macierzy przestrzeni światła dla reflektorów rzucających cień
// Rozmiar musi odpowiadać MAX_SHADOW_CASTING_SPOT_LIGHTS
uniform mat4 spotLightSpaceMatricesVS[2]; // Nazwa z VS, aby uniknąć konfliktu, jeśli też w FS
uniform int numActiveSpotShadowCastersVS; // Ile z tych macierzy jest faktycznie używanych

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    FragPos_World = vec3(model * vec4(aPos, 1.0));
    Normal_World = normalize(mat3(transpose(inverse(model))) * aNormal);
    TexCoords = aTexCoords;
    VertexColor_FS = aColor;

    FragPosDirLightSpace = dirLightSpaceMatrix * model * vec4(aPos, 1.0);

    // Oblicz pozycje w przestrzeniach świateł dla aktywnych reflektorów rzucających cień
    for (int i = 0; i < numActiveSpotShadowCastersVS; ++i) {
        if (i < 2) { // Upewnij się, że nie wychodzimy poza tablicę FragPosSpotLightSpace
            FragPosSpotLightSpace[i] = spotLightSpaceMatricesVS[i] * model * vec4(aPos, 1.0);
        }
    }
    // Dla nieużywanych slotów można by ustawić FragPosSpotLightSpace[i] = vec4(0.0);
    // ale shader fragmentów i tak powinien iterować tylko do numActiveSpotShadowCastersFS
}
