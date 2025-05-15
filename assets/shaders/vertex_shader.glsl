#version 330 core

// Atrybuty wejściowe wierzchołka
layout (location = 0) in vec3 aPos;        // Pozycja wierzchołka (przestrzeń lokalna)
layout (location = 1) in vec3 aNormal;     // Wektor normalny wierzchołka (przestrzeń lokalna)
layout (location = 2) in vec2 aTexCoords;  // Współrzędne tekstury
layout (location = 3) in vec4 aColor;      // Kolor wierzchołka (opcjonalny)

// Wyjścia do shadera fragmentów (interpolowane)
out vec3 FragPos_World;   // Pozycja fragmentu w przestrzeni świata
out vec3 Normal_World;    // Wektor normalny fragmentu w przestrzeni świata
out vec2 TexCoords;       // Przekazane współrzędne tekstury
out vec4 VertexColor;     // Przekazany kolor wierzchołka

// Uniformy (zmienne globalne dla shadera, ustawiane z C++)
uniform mat4 model;       // Macierz modelu (transformacja z przestrzeni lokalnej do światowej)
uniform mat4 view;        // Macierz widoku (transformacja z przestrzeni światowej do przestrzeni widoku/kamery)
uniform mat4 projection;  // Macierz projekcji (transformacja z przestrzeni widoku do przestrzeni przycinania)

void main() {
    // Transformacja pozycji wierzchołka do przestrzeni przycinania (Clip Space)
    // gl_Position to specjalna zmienna wyjściowa vertex shadera, która określa ostateczną pozycję.
    gl_Position = projection * view * model * vec4(aPos, 1.0);

    // Transformacja pozycji wierzchołka do przestrzeni świata i przekazanie do fragment shadera.
    // Używane do obliczeń oświetlenia w przestrzeni świata.
    FragPos_World = vec3(model * vec4(aPos, 1.0));

    // Transformacja wektora normalnego do przestrzeni świata.
    // Ważne: Dla normalnych używamy macierzy normalnych (transponowana odwrotność części 3x3 macierzy modelu),
    // aby poprawnie obsłużyć niesymetryczne skalowanie modelu.
    // Jeśli model nie jest skalowany niesymetrycznie, można by użyć mat3(model).
    Normal_World = mat3(transpose(inverse(model))) * aNormal;
    // Normal_World = normalize(Normal_World); // Normalizacja może być zrobiona tutaj lub w frag shaderze

    // Przekazanie współrzędnych tekstury bez zmian.
    TexCoords = aTexCoords;

    // Przekazanie koloru wierzchołka.
    // Jeśli kolor nie jest używany (np. wszystkie wartości są 0), można ustawić domyślny.
    if (length(aColor) > 0.001) { // Prosty warunek, czy kolor jest znaczący
         VertexColor = aColor;
    } else {
         VertexColor = vec4(1.0, 1.0, 1.0, 1.0); // Domyślny biały, jeśli nie ma koloru wierzchołka
    }
}
