#version 330 core

// Fragment shader dla przebiegu generowania mapy głębi jest bardzo prosty.
// Nie potrzebuje on generować żadnego koloru, ponieważ interesuje nas tylko
// bufor głębi, który jest wypełniany automatycznie przez potok renderowania.
// Można go zostawić pustym.

void main() {
    // gl_FragDepth = gl_FragCoord.z; // To jest domyślne zachowanie, więc nie jest konieczne
}
