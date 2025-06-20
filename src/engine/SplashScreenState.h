/**
* @file SplashScreenState.h
* @brief Definicja klasy SplashScreenState.
*
* Plik ten zawiera deklarację klasy SplashScreenState, która reprezentuje
* stan ekranu powitalnego w grze.
*/
#ifndef SPLASH_SCREEN_STATE_H
#define SPLASH_SCREEN_STATE_H

#include <string>
#include <memory> // Dla std::unique_ptr i std::shared_ptr

#include "IGameState.h" 

// Forward declarations
class Engine;
class InputManager;
class EventManager;
class Renderer;
class Texture;
class Shader;

/**
 * @file SplashScreenState.h
 * @brief Definicja klasy SplashScreenState, reprezentujacej stan ekranu powitalnego.
 */

 /**
  * @class SplashScreenState
  * @brief Stan gry wyswietlajacy obraz powitalny przez okreslony czas.
  * * Odpowiada za ladowanie, wyswietlanie (z efektem pojawiania sie i znikania)
  * oraz zwalnianie zasobow zwiazanych z ekranem powitalnym.
  * Przejscie do nastepnego stanu nastepuje po uplywie czasu lub interakcji uzytkownika.
  */
class SplashScreenState : public IGameState {
public:
    /**
     * @brief Konstruktor.
     * @param texturePath Sciezka do pliku tekstury ekranu powitalnego.
     * @param displayDuration Calkowity czas wyswietlania ekranu powitalnego w sekundach.
     */
    SplashScreenState(const std::string& texturePath, float displayDuration);

    /**
     * @brief Destruktor.
     */
    ~SplashScreenState() override;

    // Usuniete konstruktory kopiujace i operatory przypisania, aby zapobiec kopiowaniu stanu
    SplashScreenState(const SplashScreenState&) = delete;
    SplashScreenState& operator=(const SplashScreenState&) = delete;
    SplashScreenState(SplashScreenState&&) = delete;
    SplashScreenState& operator=(SplashScreenState&&) = delete;

    /**
     * @brief Inicjalizuje stan.
     * Laduje zasoby (teksture, shader) i przygotowuje stan do dzialania.
     * @param engine Wskaznik do glownego obiektu silnika.
     */
    void init() override;

    /**
     * @brief Sprzata zasoby uzywane przez stan.
     * Wywolywane przed zniszczeniem stanu.
     */
    void cleanup() override;

    /**
     * @brief Wstrzymuje stan.
     * Wywolywane, gdy stan przechodzi do tla (np. gdy inny stan staje sie aktywny).
     */
    void pause() override;

    /**
     * @brief Wznawia stan.
     * Wywolywane, gdy stan wraca z tla i staje sie ponownie aktywny.
     */
    void resume() override;

    /**
     * @brief Przetwarza zdarzenia wejsciowe.
     * @param inputManager Wskaznik do menedzera wejscia.
     * @param eventManager Wskaznik do menedzera zdarzen (jesli uzywany).
     */
    void handleEvents(InputManager* inputManager, EventManager* eventManager) override;

    /**
     * @brief Aktualizuje logike stanu.
     * @param deltaTime Czas od ostatniej klatki w sekundach.
     */
    void update(float deltaTime) override;

    /**
     * @brief Renderuje stan.
     * @param renderer Wskaznik do obiektu renderujacego.
     */
    void render(Renderer* renderer) override;

    /**
     * @brief Sprawdza, czy stan zakonczyl swoje dzialanie.
     * @return True, jesli stan powinien zostac zakonczony, false w przeciwnym razie.
     */
    bool isFinished() const { return m_isFinished; }

private:
    // Wskazniki na zasoby
    std::shared_ptr<Texture> m_splashTexturePtr; ///< Wskaznik na obiekt tekstury (zarzadzany przez ResourceManager).
    std::unique_ptr<Shader> m_splashShader;      ///< Wskaznik na obiekt shadera dla ekranu powitalnego.

    // Konfiguracja
    std::string m_texturePath;    ///< Sciezka do pliku tekstury.
    std::string m_textureKey;     ///< Klucz uzywany do identyfikacji tekstury w ResourceManager.
    float m_displayDuration;      ///< Calkowity czas wyswietlania.

    // Stan wewnetrzny
    float m_elapsedTime;          ///< Czas, ktory uplynal od rozpoczecia stanu.
    bool m_isFinished;            ///< Flaga wskazujaca, czy stan zakonczyl dzialanie.
    float m_alpha;                ///< Aktualna wartosc alfa dla efektu pojawiania/znikania (0.0 - 1.0).

    // Zasoby renderowania OpenGL
    unsigned int m_quadVAO;       ///< Vertex Array Object dla czworokata wyswietlajacego teksture.
    unsigned int m_quadVBO;       ///< Vertex Buffer Object dla czworokata.

    /**
     * @brief Inicjalizuje zasoby renderowania OpenGL (VAO, VBO, shader).
     * Ta metoda jest wywolywana po zaladowaniu tekstury, aby mozna bylo uzyc jej wymiarow.
     */
    void initRenderResources();

    /**
     * @brief Sprzata zasoby renderowania OpenGL (VAO, VBO).
     * Shader jest zwalniany automatycznie przez unique_ptr.
     */
    void cleanupRenderResources();
};

#endif // SPLASH_SCREEN_STATE_H
