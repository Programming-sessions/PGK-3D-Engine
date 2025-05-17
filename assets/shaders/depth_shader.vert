#version 330 core
layout (location = 0) in vec3 aPos; // Pozycja wierzchołka w przestrzeni modelu

// Macierze transformacji
uniform mat4 model;             // Macierz modelu dla obiektu
uniform mat4 lightSpaceMatrix;  // Macierz transformująca do przestrzeni światła (lightView * lightProjection)

void main()
{
    // Transformacja pozycji wierzchołka do przestrzeni klipu z perspektywy światła
    // Wynikowa pozycja jest używana do testu głębokości i zapisu do mapy głębi.
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
