#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 aColor; // Kolor wierzchołka dla flat shading

// Wyjścia do fragment shadera
out vec3 FragPos_World;    // Pozycja fragmentu w przestrzeni świata
out vec3 Normal_World;     // Normalna w przestrzeni świata (interpolowana)
out vec2 TexCoords;        // Współrzędne tekstury
out vec4 VertexColor_FS;   // Przekazanie koloru wierzchołka do FS

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    FragPos_World = vec3(model * vec4(aPos, 1.0));

    // Transformacja normalnej do przestrzeni świata
    // Używamy macierzy normalnych (transponowana odwrotność submacierzy 3x3 z macierzy modelu)
    // aby poprawnie obsłużyć niescalarne skalowanie.
    Normal_World = normalize(mat3(transpose(inverse(model))) * aNormal);
    
    TexCoords = aTexCoords;
    VertexColor_FS = aColor; // Przekaż kolor wierzchołka
}
