#version 330 core

// Ten fragment shader jest celowo pusty.
// Dla przebiegu generowania mapy głębi (depth pass), interesuje nas tylko
// zapis wartości głębokości do bufora głębokości (depth buffer).
// OpenGL robi to automatycznie, gdy nie ma jawnego zapisu do bufora koloru
// (co jest skonfigurowane w ShadowMapper przez glDrawBuffer(GL_NONE) i glReadBuffer(GL_NONE)).
// Wartości głębokości są pobierane z gl_FragCoord.z (który jest wynikiem
// transformacji w vertex shaderze i rasteryzacji).

void main()
{
    // Nie ma potrzeby niczego tutaj robić.
    // Bufor głębi zostanie wypełniony automatycznie.
}
