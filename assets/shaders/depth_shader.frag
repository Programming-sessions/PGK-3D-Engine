#version 330 core

// Wejście z Vertex Shadera
in vec3 FragPos_World_DS; // Pozycja fragmentu w przestrzeni świata (przekazana z VS)

// --- Uniformy ---
// Te uniformy są potrzebne TYLKO do obliczenia głębokości liniowej dla map sześciennych (PointLight shadows).
// Dla map 2D (światła kierunkowe/reflektory), możemy ich nie używać i polegać na gl_FragCoord.z.
uniform bool u_isCubeMapPass;   // Flaga: true, jeśli renderujemy do sześciennej mapy cieni (PointLight)
uniform vec3 u_lightPos_World;  // Pozycja światła punktowego w przestrzeni świata (tylko dla CubeMapPass)
uniform float u_lightFarPlane;   // Daleka płaszczyzna przycinania światła punktowego (tylko dla CubeMapPass)

// GLSL nie wymaga jawnego wyjścia dla głębokości, jeśli używamy standardowego bufora głębi.
// Zapis do `gl_FragDepth` jest potrzebny tylko, gdy chcemy nadpisać standardową wartość głębokości.

void main()
{
    if (u_isCubeMapPass) {
        // --- Obliczanie głębokości liniowej dla sześciennej mapy cieni (PointLight) ---
        // Obliczamy odległość fragmentu od pozycji światła punktowego.
        float lightDistance = length(FragPos_World_DS - u_lightPos_World);
        // Normalizujemy tę odległość do zakresu [0,1] używając dalekiej płaszczyzny przycinania światła.
        // Ta znormalizowana wartość (liniowa głębokość) zostanie zapisana do tekstury głębi (CubeMap).
        gl_FragDepth = lightDistance / u_lightFarPlane;
    } else {
        // --- Standardowa mapa cieni 2D (DirectionalLight, SpotLight) ---
        // Dla standardowych map cieni 2D, domyślne zachowanie OpenGL jest zazwyczaj wystarczające.
        // Jeśli Framebuffer Object (FBO) jest skonfigurowany tylko z buforem głębi (bez bufora koloru),
        // OpenGL automatycznie zapisze wartość głębokości z `gl_FragCoord.z` (po transformacji perspektywicznej).
        // Nie ma potrzeby jawnego zapisu do `gl_FragDepth`, chyba że chcemy zmodyfikować
        // standardową wartość głębokości (np. wprowadzić własne nieliniowe mapowanie).
        // Pusty `main()` lub jawny zapis `gl_FragDepth = gl_FragCoord.z;` dałby ten sam efekt
        // co pozwolenie OpenGL na domyślne działanie. 
        // Zostawiamy pusty blok `else`, aby polegać na domyślnym zapisie głębokości przez OpenGL.
    }
}
