#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

// Użyjemy projekcji ortogonalnej bezpośrednio w shaderze lub przekażemy ją
// uniform mat4 projection; // Można dodać, jeśli chcesz większą kontrolę

void main() {
    // gl_Position = projection * vec4(aPos.x, aPos.y, 0.0, 1.0); // Jeśli używasz macierzy projekcji
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); // Bezpośrednie współrzędne znormalizowane
    TexCoords = aTexCoords;
}