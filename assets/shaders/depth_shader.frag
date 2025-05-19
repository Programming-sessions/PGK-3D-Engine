#version 330 core

// Wejście z vertex shadera
in vec3 FragPos_World_DS; // Pozycja fragmentu w przestrzeni świata

// Uniformy potrzebne do obliczenia głębokości liniowej dla map sześciennych
// Te uniformy muszą być ustawione z C++ TYLKO gdy renderujemy do mapy sześciennej.
// Dla map 2D (kierunkowe/spot), możemy ich nie używać i polegać na gl_FragCoord.z.
uniform bool u_isCubeMapPass;   // Flaga wskazująca, czy to przebieg dla mapy sześciennej
uniform vec3 u_lightPos_World;  // Pozycja światła punktowego w przestrzeni świata
uniform float u_lightFarPlane;   // Płaszczyzna daleka światła punktowego

void main()
{
    if (u_isCubeMapPass) {
        // Obliczanie głębokości liniowej dla mapy sześciennej
        // Odległość od fragmentu do pozycji światła
        float lightDistance = length(FragPos_World_DS - u_lightPos_World);

        // Normalizuj odległość do zakresu [0,1] używając farPlane światła
        // Ta wartość zostanie zapisana do tekstury głębi (mapy sześciennej)
        gl_FragDepth = lightDistance / u_lightFarPlane;
    } else {
        // Dla standardowych map cieni 2D (światła kierunkowe, reflektory),
        // domyślne zachowanie OpenGL (zapis gl_FragCoord.z) jest zazwyczaj wystarczające.
        // Jeśli FBO jest skonfigurowany tylko z buforem głębi, OpenGL automatycznie
        // zapisze wartość głębokości z gl_FragCoord.z.
        // Nie ma potrzeby jawnego zapisu do gl_FragDepth, chyba że chcemy zmodyfikować
        // standardową wartość głębokości.
        // Pusty main() lub jawny zapis gl_FragCoord.z da ten sam efekt.
        // gl_FragDepth = gl_FragCoord.z; // To jest domyślne zachowanie
    }
}
