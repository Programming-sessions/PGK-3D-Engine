/**
* @file TextRenderer.h
* @brief Definicja klasy TextRenderer.
*
* Plik ten zawiera deklarację klasy TextRenderer, która jest
* odpowiedzialna za renderowanie tekstu na ekranie.
*/
#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include <glad/glad.h>   // For GLint, GLuint
#include <ft2build.h>
#include FT_FREETYPE_H
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <GLFW/glfw3.h> // For GLFWwindow


/**
 * @file TextRenderer.h
 * @brief Definicja klasy TextRenderer oraz struktury Character.
 */

 /**
  * @struct Character
  * @brief Przechowuje informacje o pojedynczym znaku czcionki.
  *
  * Struktura zawiera identyfikator tekstury OpenGL dla glifu, jego rozmiar,
  * pozycje wzgledem linii bazowej (bearing) oraz przesuniecie do nastepnego znaku (advance).
  */
struct Character {
    unsigned int TextureID; ///< Identyfikator tekstury OpenGL dla glifu znaku.
    glm::ivec2   Size;      ///< Rozmiar glifu (szerokosc, wysokosc).
    glm::ivec2   Bearing;   ///< Przesuniecie glifu od linii bazowej (lewo, gora).
    unsigned int Advance;   ///< Przesuniecie kursora do nastepnego znaku (wartosc w 1/64 piksela).
};

/**
 * @class TextRenderer
 * @brief Klasa singleton odpowiedzialna za renderowanie tekstu przy uzyciu biblioteki FreeType i OpenGL.
 *
 * Zarzadza ladowaniem czcionek, generowaniem glifow jako tekstur oraz rysowaniem tekstu na ekranie.
 * Wykorzystuje ResourceManager do zarzadzania zasobami czcionek.
 */
class TextRenderer {
private:
    GLFWwindow* window;                     ///< Wskaznik do okna GLFW, w ktorym tekst bedzie renderowany.
    std::map<char, Character> Characters;   ///< Mapa przechowujaca zaladowane znaki (glify).
    unsigned int VAO, VBO;                  ///< Vertex Array Object i Vertex Buffer Object dla renderowania quadow znakow.
    unsigned int shaderProgram;             ///< Program shaderow uzywany do renderowania tekstu.

    FT_Face face;       ///< Obiekt FreeType reprezentujacy zaladowana czcionke. Pobierany z ResourceManager.

    int windowWidth;                ///< Aktualna szerokosc okna (w pikselach), uzywana do macierzy projekcji.
    int windowHeight;               ///< Aktualna wysokosc okna (w pikselach), uzywana do macierzy projekcji.
    bool initialized = false;       ///< Flaga wskazujaca, czy TextRenderer zostal pomyslnie zainicjalizowany.
    std::string currentFontName;    ///< Nazwa aktualnie zaladowanej i uzywanej czcionki (klucz w ResourceManager).

    // Cached uniform locations
    GLint locProjection;    ///< Zbuforowana lokalizacja uniformu macierzy projekcji.
    GLint locTextColor;     ///< Zbuforowana lokalizacja uniformu koloru tekstu.
    GLint locTextSampler;   ///< Zbuforowana lokalizacja uniformu samplera tekstury.

    // Singleton
    static TextRenderer* instance; ///< Statyczna instancja singletona TextRenderer.

    /**
     * @brief Prywatny konstruktor singletona.
     *
     * Inicjalizuje pola domyslnymi wartosciami, w tym zbuforowane lokalizacje uniformow.
     */
    TextRenderer() : window(nullptr), VAO(0), VBO(0), shaderProgram(0), face(nullptr),
        windowWidth(0), windowHeight(0), initialized(false), currentFontName(""),
        locProjection(-1), locTextColor(-1), locTextSampler(-1) {
    }


public:
    /**
     * @brief Zwraca instancje singletona TextRenderer.
     *
     * Jesli instancja nie istnieje, tworzy ja.
     * @return Wskaznik do instancji TextRenderer.
     */
    static TextRenderer* getInstance();

    /**
     * @brief Destruktor.
     *
     * Wywoluje metode cleanup() w celu zwolnienia zasobow.
     */
    ~TextRenderer();

    // Usuwamy domyslne konstruktory kopiujace i operatory przypisania, aby zapobiec kopiowaniu singletona.
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
    TextRenderer(TextRenderer&&) = delete;
    TextRenderer& operator=(TextRenderer&&) = delete;


    /**
     * @brief Inicjalizuje TextRenderer.
     *
     * Laduje czcionke, konfiguruje shadery, VAO/VBO oraz generuje glify.
     * @param windowPtr Wskaznik do okna GLFW.
     * @param fontPath Sciezka do pliku czcionki (.ttf).
     * @param fontNameInManager Nazwa, pod jaka czcionka bedzie przechowywana w ResourceManager.
     * @param fontSize Rozmiar czcionki w pikselach.
     * @param winWidth Szerokosc okna renderowania.
     * @param winHeight Wysokosc okna renderowania.
     * @return True, jesli inicjalizacja przebiegla pomyslnie, false w przeciwnym razie.
     */
    bool initialize(GLFWwindow* windowPtr, const std::string& fontPath, const std::string& fontNameInManager, unsigned int fontSize, int winWidth, int winHeight);

    /**
     * @brief Renderuje tekst na ekranie.
     *
     * @param text Tekst do wyrenderowania.
     * @param x Pozycja X (od lewej) poczatku tekstu.
     * @param y Pozycja Y (od dolu) poczatku tekstu.
     * @param scale Skala renderowanego tekstu.
     * @param color Kolor tekstu (RGB).
     */
    void renderText(const std::string& text, float x, float y, float scale, glm::vec3 color);

    /**
     * @brief Zwalnia wszystkie zasoby uzywane przez TextRenderer.
     *
     * Usuwa tekstury glifow, VAO/VBO, program shaderow.
     */
    void cleanup();

    /**
     * @brief Sprawdza, czy TextRenderer zostal zainicjalizowany.
     * @return True, jesli zainicjalizowany, false w przeciwnym razie.
     */
    bool isInitialized() const { return initialized; }

    /**
     * @brief Aktualizuje wymiary okna uzywane do macierzy projekcji.
     *
     * Powinno byc wywolywane przy zmianie rozmiaru okna.
     * @param newWindowWidth Nowa szerokosc okna.
     * @param newWindowHeight Nowa wysokosc okna.
     */
    void updateProjectionMatrix(int newWindowWidth, int newWindowHeight);

private:
    /**
     * @brief Inicjalizuje shadery uzywane do renderowania tekstu.
     *
     * Kompiluje i linkuje vertex oraz fragment shader. Pobiera takze lokalizacje uniformow.
     * @return True, jesli inicjalizacja shaderow przebiegla pomyslnie, false w przeciwnym razie.
     */
    bool initializeShaders();

    /**
     * @brief Laduje glify dla aktualnie ustawionej czcionki.
     *
     * Generuje tekstury dla pierwszych 128 znakow ASCII i zapisuje je w mapie `Characters`.
     * @return True, jesli ladowanie glifow przebieglo pomyslnie, false w przeciwnym razie.
     */
    bool loadGlyphs();
};

#endif // TEXT_RENDERER_H
