#version 330 core

// Stałe (powinny być zsynchronizowane z C++ oraz Fragment Shaderem)
// Definiujemy tutaj, aby bylo spójne z FS i komentarzami.
// W praktyce, jesli te stale sa takie same dla VS i FS, mozna rozwazyc przekazanie
// ich przez system budowania shaderow lub uzycie wspolnego pliku include (jesli system to wspiera).
const int MAX_SHADOW_CASTING_SPOT_LIGHTS_VS = 2; // Rozmiar tablic dla cieni reflektorów w VS

// --- Atrybuty wejściowe wierzchołka ---
layout (location = 0) in vec3 aPos;        // Pozycja wierzchołka (przestrzeń lokalna modelu)
layout (location = 1) in vec3 aNormal;     // Wektor normalny wierzchołka (przestrzeń lokalna modelu)
layout (location = 2) in vec2 aTexCoords;  // Współrzędne tekstury
layout (location = 3) in vec4 aColor;      // Kolor wierzchołka (opcjonalny)

// --- Wyjścia do Fragment Shadera (interpolowane) ---
out vec3 FragPos_World;            // Pozycja fragmentu w przestrzeni świata
out vec3 Normal_World;             // Wektor normalny fragmentu w przestrzeni świata
out vec2 TexCoords;                // Współrzędne tekstury
out vec4 VertexColor_FS;           // Kolor wierzchołka przekazany do FS
out vec4 FragPosDirLightSpace;     // Pozycja fragmentu w przestrzeni światła kierunkowego (dla cieni)

// Pozycja fragmentu w przestrzeniach świateł reflektorów rzucających cień
out vec4 FragPosSpotLightSpace[MAX_SHADOW_CASTING_SPOT_LIGHTS_VS];

// --- Uniformy (zmienne globalne ustawiane z CPU) ---
uniform mat4 model;                // Macierz modelu (transformacja lokalna -> świat)
uniform mat4 view;                 // Macierz widoku (transformacja świat -> widok/kamera)
uniform mat4 projection;           // Macierz projekcji (transformacja widok -> przestrzeń przycinania)

// Uniformy dla cieni światła kierunkowego
uniform mat4 dirLightSpaceMatrix;  // Macierz transformująca do przestrzeni światła kierunkowego

// Uniformy dla cieni reflektorów
// Tablica macierzy transformujących do przestrzeni świateł reflektorów rzucających cień
uniform mat4 spotLightSpaceMatricesVS[MAX_SHADOW_CASTING_SPOT_LIGHTS_VS];
uniform int numActiveSpotShadowCastersVS; // Liczba aktywnych reflektorów rzucających cień (ile macierzy użyć)

void main() {
    // Transformacja pozycji wierzchołka do przestrzeni przycinania (clip space)
    gl_Position = projection * view * model * vec4(aPos, 1.0);

    // Transformacja pozycji wierzchołka do przestrzeni świata (dla obliczeń oświetlenia w FS)
    FragPos_World = vec3(model * vec4(aPos, 1.0));

    // Transformacja wektora normalnego do przestrzeni świata
    // Używamy macierzy normalnych (transponowana odwrotność części 3x3 macierzy modelu)
    // aby poprawnie obsłużyć niejednorodne skalowanie modelu.
    Normal_World = normalize(mat3(transpose(inverse(model))) * aNormal);

    // Przekazanie współrzędnych tekstury i koloru wierzchołka bez zmian
    TexCoords = aTexCoords;
    VertexColor_FS = aColor;

    // Transformacja pozycji fragmentu do przestrzeni światła kierunkowego
    FragPosDirLightSpace = dirLightSpaceMatrix * model * vec4(aPos, 1.0);

    // Oblicz pozycje w przestrzeniach świateł dla aktywnych reflektorów rzucających cień
    // Iterujemy tylko przez aktywne reflektory rzucajace cien, dla ktorych mamy macierze.
    for (int i = 0; i < numActiveSpotShadowCastersVS; ++i) {
        // Upewniamy się, że nie wychodzimy poza zadeklarowany rozmiar tablicy
        // (chociaż numActiveSpotShadowCastersVS powinno być <= MAX_SHADOW_CASTING_SPOT_LIGHTS_VS)
        if (i < MAX_SHADOW_CASTING_SPOT_LIGHTS_VS) {
            FragPosSpotLightSpace[i] = spotLightSpaceMatricesVS[i] * model * vec4(aPos, 1.0);
        }
    }
    // Dla nieużywanych slotów FragPosSpotLightSpace nie musimy nic robić,
    // ponieważ Fragment Shader powinien iterować tylko do `numActiveSpotShadowCastersFS` (lub odpowiednika).
}
