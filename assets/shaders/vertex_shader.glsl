#version 330 core

// Atrybuty wejściowe wierzchołka
layout (location = 0) in vec3 position;   // Pozycja wierzchołka
layout (location = 1) in vec3 normal;     // Normalny wektor
layout (location = 2) in vec2 texCoords;  // Współrzędne tekstury
layout (location = 3) in vec4 color;      // Kolor wierzchołka

// Wyjścia do shadera fragmentów
out vec4 vertexColor; // Bezpośrednie przekazanie koloru

// Uniformy (macierze transformacji)
uniform mat4 model;       // Macierz modelu
uniform mat4 view;        // Macierz widoku
uniform mat4 projection;  // Macierz projekcji

void main() {
    // Transformacja pozycji do przestrzeni przycinania
    gl_Position = projection * view * model * vec4(position, 1.0);

    // Przekazanie koloru do shadera fragmentów
    vertexColor = color;
}