/**
* @file IRenderable.h
* @brief Definicja interfejsu IRenderable.
*
* Plik ten zawiera deklarację interfejsu IRenderable, który musi być
* implementowany przez wszystkie obiekty, które mogą być renderowane na ekranie.
*/
#ifndef IRENDERABLE_H
#define IRENDERABLE_H

#include <glad/glad.h> // Podstawowe funkcje OpenGL, moga byc potrzebne w implementacjach
#include <glm/glm.hpp> // Dla typow macierzy glm::mat4

// Deklaracja wyprzedzajaca, aby uniknac pelnego #include "Shader.h" w interfejsie.
// Wystarczy wskaznik lub referencja do typu Shader.
class Shader;

/**
 * @interface IRenderable
 * @brief Interfejs dla wszystkich obiektow, ktore moga byc renderowane w scenie.
 *
 * Definiuje kontrakt dla obiektow renderowalnych, okreslajac metody,
 * ktore musza zostac zaimplementowane, aby obiekt mogl byc narysowany
 * oraz aby mogl potencjalnie rzucac cienie.
 */
class IRenderable {
public:
    /**
     * @brief Wirtualny destruktor.
     * Niezbedny dla poprawnego zarzadzania pamiecia przy polimorfizmie,
     * gdy obiekty sa usuwane poprzez wskaznik do klasy bazowej (interfejsu).
     */
    virtual ~IRenderable() = default;

    /**
     * @brief Metoda renderujaca obiekt w glownym przebiegu renderowania.
     * Implementacja tej metody powinna zawierac logike rysowania obiektu,
     * wlaczajac w to ustawienie odpowiedniego shadera, powiazanie VAO,
     * ustawienie macierzy modelu oraz wywolanie komendy rysujacej.
     * @param viewMatrix Macierz widoku kamery.
     * @param projectionMatrix Macierz projekcji kamery.
     */
    virtual void render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) = 0;

    /**
     * @brief Metoda renderujaca obiekt wylacznie na potrzeby generowania mapy glebokosci (np. dla cieni).
     * Zazwyczaj uzywa uproszczonego shadera (depth shader), ktory zapisuje tylko informacje o glebokosci.
     * @param depthShader Wskaznik do shadera uzywanego do zapisu glebokosci.
     */
    virtual void renderForDepthPass(Shader* depthShader) = 0;

    /**
     * @brief Sprawdza, czy obiekt aktualnie rzuca cienie.
     * @return True jesli obiekt rzuca cienie, false w przeciwnym wypadku.
     */
    virtual bool castsShadow() const = 0;

    /**
     * @brief Ustawia, czy obiekt ma rzucac cienie.
     * @param castsShadow Wartosc logiczna okreslajaca, czy obiekt bedzie rzucal cienie.
     */
    virtual void setCastsShadow(bool castsShadow) = 0;

    // Opcjonalne metody, ktore mozna dodac w przyszlosci, jesli beda potrzebne:
    // /**
    //  * @brief Sprawdza, czy obiekt jest aktualnie widoczny.
    //  * @return True jesli obiekt jest widoczny, false w przeciwnym wypadku.
    //  */
    // virtual bool isVisible() const { return true; }

    // /**
    //  * @brief Pobiera kolejnosc renderowania obiektu.
    //  * Moze byc uzywane do sortowania obiektow (np. przezroczystych).
    //  * @return Wartosc calkowita okreslajaca kolejnosc renderowania.
    //  */
    // virtual int getRenderOrder() const { return 0; }
};

#endif // IRENDERABLE_H
