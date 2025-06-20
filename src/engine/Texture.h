/**
* @file Texture.h
* @brief Definicja klasy Texture.
*
* Plik ten zawiera deklarację klasy Texture, która zarządza
* danymi tekstur i ich powiązaniem z jednostkami teksturującymi w OpenGL.
*/
#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/glad.h>
#include <string>

/**
 * @file Texture.h
 * @brief Definicja klasy Texture przechowujacej dane tekstury.
 */

 /**
  * @brief Klasa reprezentujaca teksture 2D.
  * * Przechowuje informacje o teksturze, takie jak jej identyfikator w systemie graficznym (np. OpenGL),
  * wymiary, liczbe kanalow, typ oraz sciezke do pliku zrodlowego.
  */
class Texture {
public:
    /** @brief Identyfikator tekstury w systemie graficznym (np. OpenGL ID).
     * Wartosc 0 zazwyczaj oznacza niezaalokowana lub niepoprawna teksture.
     */
    GLuint ID;

    /** @brief Szerokosc tekstury w pikselach. */
    int width;

    /** @brief Wysokosc tekstury w pikselach. */
    int height;

    /** @brief Liczba kanalow koloru w teksturze (np. 3 dla RGB, 4 dla RGBA). */
    int nrChannels;

    /** @brief Typ tekstury okreslajacy jej zastosowanie w shaderach (np. "diffuse", "specular", "normal", "height"). */
    std::string type;

    /** @brief Sciezka do pliku zrodlowego tekstury.
     * Przydatna do celow debugowania, serializacji lub ponownego ladowania zasobow.
     */
    std::string path;

    /**
     * @brief Domyslny konstruktor.
     * * Inicjalizuje wszystkie pola numeryczne na 0, a pola tekstowe (type, path) na puste ciagi znakow.
     * Zapewnia to spojnosc obiektu zaraz po utworzeniu.
     */
    Texture() : ID(0), width(0), height(0), nrChannels(0), type(""), path("") {}
};

#endif // TEXTURE_H