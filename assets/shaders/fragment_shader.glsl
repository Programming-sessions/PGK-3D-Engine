#version 330 core

// Zmienna wyjściowa shadera fragmentów - ostateczny kolor piksela
out vec4 FragColor;

// Wejścia z vertex shadera (interpolowane dla każdego fragmentu)
in vec3 FragPos_World;   // Pozycja fragmentu w przestrzeni świata
in vec3 Normal_World;    // Wektor normalny fragmentu w przestrzeni świata
in vec2 TexCoords;       // Współrzędne tekstury
in vec4 VertexColor;     // Kolor wierzchołka (może być użyty jako bazowy kolor obiektu)

// Struktura materiału (odpowiada tej zdefiniowanej w C++ w Lighting.h)
struct Material {
    vec3 ambient;   // Współczynnik odbicia światła otoczenia (np. kolor materiału w cieniu)
    vec3 diffuse;   // Współczynnik rozproszenia światła (główny kolor materiału)
    vec3 specular;  // Współczynnik odbicia zwierciadlanego (kolor blasku)
    float shininess; // Współczynnik połyskliwości (jak bardzo skoncentrowany jest blask)
};

// Struktura światła kierunkowego (odpowiada tej zdefiniowanej w C++ w Lighting.h)
struct DirectionalLight {
    vec3 direction; // Kierunek, Z KTÓREGO świeci światło (znormalizowany)

    vec3 ambient;  // Intensywność i kolor światła otoczenia
    vec3 diffuse;  // Intensywność i kolor światła rozproszonego
    vec3 specular; // Intensywność i kolor światła zwierciadlanego
};

// Uniformy (zmienne globalne ustawiane z C++)
uniform Material material;          // Właściwości materiału aktualnie rysowanego obiektu
uniform DirectionalLight dirLight;  // Właściwości globalnego światła kierunkowego
uniform vec3 viewPos_World;       // Pozycja kamery/obserwatora w przestrzeni świata

// Opcjonalne tekstury (jeśli chcesz używać tekstur zamiast jednolitych kolorów materiału)
// uniform sampler2D texture_diffuse1;  // Tekstura dla koloru rozproszonego (albedo)
// uniform sampler2D texture_specular1; // Tekstura dla mapy zwierciadlanej

void main() {
    // --- 1. Komponent światła otoczenia (Ambient Lighting) ---
    // Najprostszy rodzaj oświetlenia, daje obiektom pewien bazowy kolor, nawet jeśli nie są bezpośrednio oświetlone.
    vec3 ambientColor = dirLight.ambient * material.ambient;
    // Przykład z teksturą:
    // vec3 ambientColor = dirLight.ambient * texture(texture_diffuse1, TexCoords).rgb;


    // --- 2. Komponent światła rozproszonego (Diffuse Lighting) ---
    // Symuluje, jak światło odbija się od matowych powierzchni.
    // Intensywność zależy od kąta między normalną powierzchni a kierunkiem światła.
    vec3 norm = normalize(Normal_World); // Upewnij się, że normalna jest znormalizowana
    vec3 lightDir = normalize(-dirLight.direction); // Kierunek WEKTORA DO źródła światła (odwracamy kierunek światła)
    
    // Oblicz współczynnik rozproszenia (kąt między normalną a kierunkiem światła)
    // max(dot(norm, lightDir), 0.0) zapewnia, że współczynnik nie jest ujemny (gdy światło jest "za" powierzchnią)
    float diffFactor = max(dot(norm, lightDir), 0.0);
    vec3 diffuseColor = dirLight.diffuse * diffFactor * material.diffuse;
    // Przykład z teksturą:
    // vec3 diffuseColor = dirLight.diffuse * diffFactor * texture(texture_diffuse1, TexCoords).rgb;


    // --- 3. Komponent światła zwierciadlanego (Specular Lighting) ---
    // Symuluje blask/połysk na gładkich powierzchniach.
    // Intensywność zależy od kąta między kierunkiem patrzenia a kierunkiem odbicia światła.
    vec3 viewDir = normalize(viewPos_World - FragPos_World); // Kierunek od fragmentu DO kamery
    vec3 reflectDir = reflect(-lightDir, norm); // Wektor odbicia światła od powierzchni
                                                // reflect oczekuje wektora OD źródła światła, stąd -lightDir

    // Oblicz współczynnik zwierciadlany
    // pow(..., material.shininess) kontroluje rozmiar i intensywność blasku
    float specFactor = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specularColor = dirLight.specular * specFactor * material.specular;
    // Przykład z teksturą specular map:
    // vec3 specularMapColor = texture(texture_specular1, TexCoords).rgb; // Pobierz intensywność specular z mapy
    // vec3 specularColor = dirLight.specular * specFactor * specularMapColor;


    // --- Połączenie wyników ---
    // Sumujemy wszystkie komponenty oświetlenia.
    // VertexColor może być użyty jako bazowy kolor obiektu, który jest modulowany przez oświetlenie,
    // lub material.diffuse może pełnić tę rolę.
    // W tym przykładzie zakładamy, że material.diffuse (lub tekstura diffuse) to główny kolor obiektu.
    // Jeśli chcesz użyć VertexColor jako głównego koloru, zmodyfikuj obliczenia ambient i diffuse.
    
    vec3 finalColor = ambientColor + diffuseColor + specularColor;

    // Można pomnożyć przez VertexColor, jeśli VertexColor ma modyfikować ostateczny wynik
    // np. finalColor *= VertexColor.rgb;

    FragColor = vec4(finalColor, 1.0); // Ustawiamy alpha na 1.0 (nieprzezroczysty)
                                       // Można użyć VertexColor.a, jeśli ma znaczenie.
}
