/**
* @file InputManager.h
* @brief Definicja klasy InputManager.
*
* Plik ten zawiera deklarację klasy InputManager, która jest
* odpowiedzialna za obsługę wejścia z klawiatury i myszy.
*/
#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H

#include <GLFW/glfw3.h> // Niezbedne dla typow GLFW (GLFWwindow, stale klawiszy/myszy)
#include <glm/glm.hpp>  // Niezbedne dla glm::vec2

// Usunieto niebezposrednio uzywane w tym pliku naglowkowym:
// #include <string>
// #include <vector>
// #include <map>
// Jesli sa potrzebne w implementacji (.cpp), powinny byc tam dolaczone.

#include "Event.h"        // Wymagane dla definicji zdarzen (np. KeyPressedEvent)
#include "EventManager.h" // Wymagane dla wskaznika m_eventManager i rozglaszania zdarzen

// Stale definiujace maksymalna liczbe obslugiwanych klawiszy i przyciskow myszy.
// Wartosci sa oparte na stalych GLFW.

/** @brief Maksymalna liczba obslugiwanych stanow klawiszy (na podstawie GLFW_KEY_LAST). */
const int MAX_KEYS = GLFW_KEY_LAST + 1;
/** @brief Maksymalna liczba obslugiwanych stanow przyciskow myszy (na podstawie GLFW_MOUSE_BUTTON_LAST). */
const int MAX_MOUSE_BUTTONS = GLFW_MOUSE_BUTTON_LAST + 1;

/**
 * @class InputManager
 * @brief Klasa odpowiedzialna za zarzadzanie wejsciem od uzytkownika (klawiatura, mysz).
 *
 * Przechwytuje zdarzenia z GLFW, aktualizuje wewnetrzne stany klawiszy i przyciskow myszy,
 * udostepnia metody do odpytywania o te stany (polling) oraz rozglasza zdarzenia
 * wejsciowe za pomoca EventManagera.
 */
class InputManager {
public:
    /**
     * @brief Konstruktor InputManagera.
     * @param window Wskaznik do okna GLFW, z ktorego beda przechwytywane zdarzenia.
     * @param eventManager Wskaznik do EventManagera, ktory bedzie uzywany do rozglaszania zdarzen wejsciowych.
     * @note Rejestruje niezbedne callbacki GLFW.
     */
    InputManager(GLFWwindow* window, EventManager* eventManager);

    /**
     * @brief Destruktor InputManagera.
     * @note Potencjalnie moze odrejestrowywac callbacki GLFW, chociaz GLFW robi to automatycznie przy zamykaniu okna.
     */
    ~InputManager();

    /**
     * @brief Aktualizuje wewnetrzne stany wejscia. Powinna byc wywolywana raz na klatke.
     * Glownie kopiuje obecne stany klawiszy/przyciskow do poprzednich stanow,
     * co jest uzywane przez metody pollingowe typu isKeyTyped/isKeyReleased.
     * Oblicza rowniez delte myszy dla metod pollingowych.
     */
    void update();

    // --- Metody odpytywania o stan klawiatury (Polling) ---

    /**
     * @brief Sprawdza, czy dany klawisz jest aktualnie wcisniety.
     * @param keyCode Kod klawisza (np. GLFW_KEY_W).
     * @return True jesli klawisz jest wcisniety, false w przeciwnym wypadku.
     */
    bool isKeyPressed(int keyCode) const;

    /**
     * @brief Sprawdza, czy dany klawisz zostal wcisniety w tej klatce.
     * (tj. byl zwolniony w poprzedniej klatce i jest wcisniety w obecnej).
     * @param keyCode Kod klawisza.
     * @return True jesli klawisz zostal wlasnie wcisniety, false w przeciwnym wypadku.
     */
    bool isKeyTyped(int keyCode) const;

    /**
     * @brief Sprawdza, czy dany klawisz zostal zwolniony w tej klatce.
     * (tj. byl wcisniety w poprzedniej klatce i jest zwolniony w obecnej).
     * @param keyCode Kod klawisza.
     * @return True jesli klawisz zostal wlasnie zwolniony, false w przeciwnym wypadku.
     */
    bool isKeyReleased(int keyCode) const;

    // --- Metody odpytywania o stan myszy (Polling) ---

    /**
     * @brief Sprawdza, czy dany przycisk myszy jest aktualnie wcisniety.
     * @param buttonCode Kod przycisku myszy (np. GLFW_MOUSE_BUTTON_LEFT).
     * @return True jesli przycisk jest wcisniety, false w przeciwnym wypadku.
     */
    bool isMouseButtonPressed(int buttonCode) const;

    /**
     * @brief Sprawdza, czy dany przycisk myszy zostal klikniety w tej klatce.
     * (tj. byl zwolniony w poprzedniej klatce i jest wcisniety w obecnej).
     * @param buttonCode Kod przycisku myszy.
     * @return True jesli przycisk zostal wlasnie klikniety, false w przeciwnym wypadku.
     */
    bool isMouseButtonClicked(int buttonCode) const;

    /**
     * @brief Sprawdza, czy dany przycisk myszy zostal zwolniony w tej klatce.
     * (tj. byl wcisniety w poprzedniej klatce i jest zwolniony w obecnej).
     * @param buttonCode Kod przycisku myszy.
     * @return True jesli przycisk zostal wlasnie zwolniony, false w przeciwnym wypadku.
     */
    bool isMouseButtonReleased(int buttonCode) const;

    /**
     * @brief Pobiera aktualna pozycje kursora myszy w przestrzeni okna.
     * @return Wektor 2D (glm::vec2) z pozycja X i Y kursora.
     */
    glm::vec2 getMousePosition() const;

    /**
     * @brief Pobiera zmiane pozycji kursora myszy od ostatniej aktualizacji (wywolania metody update()).
     * Przydatne do implementacji sterowania kamera opartego na odpytywaniu.
     * Dla zdarzen ruchu myszy, preferowane jest uzycie MouseMovedEvent.
     * @return Wektor 2D (glm::vec2) z delta X i Y pozycji kursora.
     */
    glm::vec2 getMouseDelta() const;

    /**
     * @brief Ustawia tryb przechwytywania kursora myszy.
     * W trybie przechwyconym kursor jest ukryty, a jego ruch jest nieograniczony,
     * co jest idealne dla sterowania kamera FPS.
     * @param enabled True aby wlaczyc przechwytywanie, false aby wylaczyc.
     */
    void setMouseCapture(bool enabled);

    /**
     * @brief Sprawdza, czy kursor myszy jest aktualnie przechwycony.
     * @return True jesli kursor jest przechwycony, false w przeciwnym wypadku.
     */
    bool isMouseCaptured() const;

    // --- Statyczne funkcje zwrotne (callbacks) dla GLFW ---
    // Sa one publiczne, poniewaz GLFW wymaga do nich dostepu jako do funkcji globalnych lub statycznych.
    // Uzywaja glfwGetWindowUserPointer do uzyskania dostepu do instancji InputManagera.

    /** @brief Statyczny callback dla zdarzen klawiatury GLFW. */
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    /** @brief Statyczny callback dla wprowadzania znakow Unicode GLFW. */
    static void charCallback(GLFWwindow* window, unsigned int codepoint);
    /** @brief Statyczny callback dla zdarzen przyciskow myszy GLFW. */
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    /** @brief Statyczny callback dla zdarzen ruchu kursora myszy GLFW. */
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    /** @brief Statyczny callback dla zdarzen przewijania rolka myszy GLFW. */
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

private:
    GLFWwindow* m_window;             ///< Wskaznik do okna GLFW.
    EventManager* m_eventManager;     ///< Wskaznik do menedzera zdarzen.

    // Stany klawiszy dla metod pollingowych
    bool m_currentKeyStates[MAX_KEYS];      ///< Aktualny stan kazdego klawisza (wcisniety/zwolniony).
    bool m_previousKeyStates[MAX_KEYS];     ///< Poprzedni stan kazdego klawisza.
    int m_keyRepeatCounts[MAX_KEYS];      ///< Licznik powtorzen dla zdarzenia KeyPressedEvent (dla GLFW_REPEAT).

    // Stany przyciskow myszy dla metod pollingowych
    bool m_currentMouseButtonStates[MAX_MOUSE_BUTTONS]; ///< Aktualny stan kazdego przycisku myszy.
    bool m_previousMouseButtonStates[MAX_MOUSE_BUTTONS];///< Poprzedni stan kazdego przycisku myszy.

    // Dane pozycji myszy
    glm::vec2 m_currentMousePosition;               ///< Aktualna pozycja kursora myszy.
    glm::vec2 m_previousMousePosition_forDelta;     ///< Poprzednia pozycja myszy, uzywana do obliczania delty dla pollingu.
    glm::vec2 m_mouseDelta_forPolling;              ///< Delta pozycji myszy obliczona w metodzie update().
    bool m_firstMouseUpdate;                        ///< Flaga wskazujaca, czy to pierwsze zdarzenie ruchu myszy (wazne przy przechwytywaniu).

    bool m_isMouseCaptured;                         ///< Okresla, czy mysz jest aktualnie przechwycona.

    // Usunieto metode resetFrameStates, poniewaz jej glowna funkcjonalnosc (aktualizacja stanow 'previous')
    // jest realizowana na poczatku metody update(). Inne elementy, jak scroll delta czy typed text,
    // sa obslugiwane przez system zdarzen.

    // Metody instancji obslugujace logike callbackow (wywolywane przez statyczne callbacki)
    // Przetwarzaja surowe dane z GLFW i rozglaszaja odpowiednie zdarzenia.
    void onKey(int key, int scancode, int action, int mods);
    void onChar(unsigned int codepoint);
    void onMouseButton(int button, int action, int mods);
    void onMouseMove(double xpos, double ypos);
    void onMouseScroll(double xoffset, double yoffset);
};

#endif // INPUTMANAGER_H
