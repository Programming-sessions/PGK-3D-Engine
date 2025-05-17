#version 330 core
layout (location = 0) in vec3 aPos; // Pozycja wierzchołka w przestrzeni modelu

uniform mat4 lightSpaceMatrix; // Macierz transformująca z przestrzeni modelu do przestrzeni klipu światła
uniform mat4 model;            // Macierz modelu obiektu

void main() {
    // Transformuj pozycję wierzchołka do przestrzeni klipu światła
    // lightSpaceMatrix to zazwyczaj lightProjection * lightView
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
