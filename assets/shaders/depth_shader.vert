#version 330 core

// --- Atrybuty wejściowe wierzchołka ---
layout (location = 0) in vec3 aPos; // Pozycja wierzchołka w przestrzeni lokalnej modelu

// --- Uniformy ---
uniform mat4 model;            // Macierz modelu (transformacja z przestrzeni lokalnej do przestrzeni świata)
uniform mat4 lightSpaceMatrix; // Macierz transformująca do przestrzeni światła (zazwyczaj lightView * lightProjection)

// --- Wyjście do Fragment Shadera ---
// Pozycja fragmentu w przestrzeni świata.
// Potrzebna w FS do obliczenia liniowej głębokości dla map sześciennych (PointLight).
out vec3 FragPos_World_DS;

void main()
{
    // Krok 1: Transformacja pozycji wierzchołka do przestrzeni świata.
    // Ta wartość jest interpolowana i przekazywana do fragment shadera.
    FragPos_World_DS = vec3(model * vec4(aPos, 1.0));

    // Krok 2: Transformacja pozycji wierzchołka do przestrzeni przycinania (clip space) z perspektywy światła.
    // Wynikowa pozycja (gl_Position) jest używana przez OpenGL do testu głębokości
    // i rasteryzacji, a jej wartość .z (po transformacji perspektywicznej i normalizacji)
    // jest zapisywana do bufora głębi (depth map).
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}

