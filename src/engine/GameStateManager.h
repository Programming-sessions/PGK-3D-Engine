/**
* @file GameStateManager.h
* @brief Definicja klasy GameStateManager.
*
* Plik ten zawiera deklarację klasy GameStateManager, która zarządza
* stanami gry, takimi jak menu, rozgrywka czy ekran pauzy.
*/
#ifndef GAME_STATE_MANAGER_H
#define GAME_STATE_MANAGER_H

#include <vector>
#include <memory> // Dla std::unique_ptr

// Deklaracje wyprzedzajace dla klas uzywanych przez GameStateManager.
// Pozwala to uniknac pelnych #include w pliku naglowkowym,
// redukujac zaleznosci kompilacji i potencjalne problemy z cyklicznymi zaleznosciami.
class IGameState;
class Engine;       // Glowna klasa silnika, dostarczajaca kontekst.
class EventManager;
class InputManager;
class Renderer;

/**
 * @class GameStateManager
 * @brief Klasa zarzadzajaca stanami gry w aplikacji.
 *
 * Odpowiada za przechowywanie stosu stanow gry, przelaczanie miedzy nimi
 * (push, pop, change) oraz delegowanie zadan takich jak obsluga zdarzen,
 * aktualizacja logiki i renderowanie do aktualnie aktywnego stanu.
 * Uzywa std::unique_ptr do zarzadzania czasem zycia obiektow stanow.
 */
class GameStateManager {
public:
    /**
     * @brief Konstruktor GameStateManagera.
     * @param engine Wskaznik do glownego obiektu silnika (Engine).
     * Jest on przekazywany do stanow gry podczas ich inicjalizacji,
     * aby umozliwic im dostep do innych systemow silnika.
     */
    GameStateManager(Engine* engine);

    /**
     * @brief Destruktor GameStateManagera.
     * Wywoluje metode cleanup() w celu poprawnego zwolnienia wszystkich zasobow
     * posiadanych przez zarzadzane stany gry przed zniszczeniem menedzera.
     */
    ~GameStateManager();

    // --- Metody API do zarzadzania stanami gry ---

    /**
     * @brief Dodaje nowy stan na wierzch stosu stanow.
     * Nowo dodany stan staje sie stanem aktywnym. Jesli na stosie istnial
     * wczesniej jakis stan, jego metoda pause() jest wywolywana.
     * Nastepnie wywolywana jest metoda init() nowego stanu.
     * @param state Inteligentny wskaznik (std::unique_ptr) do obiektu stanu gry do dodania.
     * Menedzer przejmuje wlasnosc nad tym wskaznikiem.
     */
    void pushState(std::unique_ptr<IGameState> state);

    /**
     * @brief Usuwa aktualnie aktywny stan (znajdujacy sie na wierzchu stosu).
     * Wywolywana jest metoda cleanup() usuwanego stanu. Jesli pod nim
     * na stosie znajduje sie inny stan, jego metoda resume() jest wywolywana,
     * czyniac go nowym stanem aktywnym.
     */
    void popState();

    /**
     * @brief Zastepuje aktualnie aktywny stan nowym stanem.
     * Jest to rownowazne wywolaniu popState() (jesli stos nie jest pusty),
     * a nastepnie pushState() z nowym stanem.
     * @param state Inteligentny wskaznik (std::unique_ptr) do nowego stanu gry.
     */
    void changeState(std::unique_ptr<IGameState> state);

    // --- Metody wywolywane przez glowna petle silnika ---

    /**
     * @brief Deleguje obsluge zdarzen wejsciowych i systemowych do aktualnie aktywnego stanu.
     * Wywoluje metode handleEvents() stanu znajdujacego sie na wierzchu stosu.
     * @param inputManager Wskaznik do menedzera wejscia (InputManager).
     * @param eventManager Wskaznik do menedzera zdarzen (EventManager).
     */
    void handleEventsCurrentState(InputManager* inputManager, EventManager* eventManager);

    /**
     * @brief Deleguje logike aktualizacji do aktualnie aktywnego stanu.
     * Wywoluje metode update() stanu znajdujacego sie na wierzchu stosu.
     * @param deltaTime Czas (w sekundach), ktory uplynal od ostatniej klatki.
     */
    void updateCurrentState(float deltaTime);

    /**
     * @brief Deleguje renderowanie sceny do aktualnie aktywnego stanu.
     * Wywoluje metode render() stanu znajdujacego sie na wierzchu stosu.
     * @param renderer Wskaznik do obiektu renderera.
     */
    void renderCurrentState(Renderer* renderer);

    /**
     * @brief Sprawdza, czy stos stanow gry jest pusty.
     * @return True, jesli stos jest pusty (brak aktywnych stanow), false w przeciwnym wypadku.
     */
    bool isEmpty() const;

    /**
     * @brief Usuwa i sprzata wszystkie stany gry znajdujace sie na stosie.
     * Wywolywana np. podczas zamykania aplikacji lub w destruktorze menedzera.
     * Dla kazdego stanu na stosie (zaczynajac od wierzcholka) wywoluje jego metode cleanup().
     */
    void cleanup();

private:
    /**
     * @brief Wskaznik do glownego obiektu silnika.
     * Przechowywany w celu przekazania kontekstu silnika do poszczegolnych stanow gry
     * podczas ich inicjalizacji (metoda IGameState::init()).
     */
    Engine* m_engine;

    /**
     * @brief Stos stanow gry.
     * Przechowuje inteligentne wskazniki (std::unique_ptr) do obiektow stanow gry.
     * Ostatni element wektora (m_states.back()) jest traktowany jako aktywny stan
     * znajdujacy sie na wierzchu stosu.
     */
    std::vector<std::unique_ptr<IGameState>> m_states;

    // Potencjalne miejsce na kolejke operacji na stanach (Pending Actions),
    // jesli chcemy, aby zmiany stanow (push/pop/change) byly przetwarzane
    // w okreslonym momencie petli gry, a nie natychmiast.
    // struct PendingChange { /* ... */ };
    // std::vector<PendingChange> m_pendingChanges;
};

#endif // GAME_STATE_MANAGER_H
