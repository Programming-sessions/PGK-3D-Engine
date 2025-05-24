#version 330 core

// --- Wyjście shadera ---
out vec4 FragColor; // Końcowy kolor fragmentu

// --- Wejście z Vertex Shadera (interpolowane) ---
in vec2 TexCoords; // Współrzędne tekstury

// --- Uniformy ---
uniform sampler2D splashTextureSampler; // Próbnik tekstury dla obrazu splash screen
uniform float alphaValue;               // Wartość alfa dla efektu przenikania (0.0 - przezroczysty, 1.0 - nieprzezroczysty)

void main() {
    // Pobranie koloru z tekstury na podstawie interpolowanych współrzędnych
    vec4 texColor = texture(splashTextureSampler, TexCoords);

    // Opcjonalnie: Odrzucenie fragmentów, jeśli alfa tekstury jest bardzo niska
    // (przydatne, jeśli sama tekstura ma przezroczystość, np. logo z przezroczystym tłem)
    // if(texColor.a < 0.1) discard;

    // Ustawienie finalnego koloru fragmentu.
    // Używamy kolorów RGB z tekstury i mnożymy jej kanał alfa przez globalną wartość alphaValue,
    // aby uzyskać efekt płynnego pojawiania się / zanikania całego obrazu.
    FragColor = vec4(texColor.rgb, texColor.a * alphaValue);
}
