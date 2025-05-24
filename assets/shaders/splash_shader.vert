#version 330 core

// --- Atrybuty wejściowe wierzchołka ---
// Dla prostego czworokąta 2D pokrywającego cały ekran,
// pozycje mogą być bezpośrednio w Znormalizowanych Współrzędnych Urządzenia (NDC).
layout (location = 0) in vec2 aPos;        // Pozycja wierzchołka (zakładamy, że już w NDC, np. od -1 do 1)
layout (location = 1) in vec2 aTexCoords;  // Współrzędne tekstury dla tego wierzchołka

// --- Wyjście do Fragment Shadera (interpolowane) ---
out vec2 TexCoords; // Przekazanie współrzędnych tekstury

void main() {
    // Ustawienie pozycji wierzchołka w przestrzeni przycinania (clip space).
    // Dla czworokąta pełnoekranowego, aPos może być bezpośrednio współrzędnymi NDC.
    // Ustawiamy z = 0.0 (lub -1.0 w zależności od konwencji) dla rysowania na płaszczyźnie bliskiej,
    // oraz w = 1.0 dla poprawnej transformacji perspektywicznej (choć tutaj nie ma perspektywy).
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);

    // Przekazanie współrzędnych tekstury do fragment shadera.
    TexCoords = aTexCoords;
}
