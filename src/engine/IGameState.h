/**
* @file IGameState.h
* @brief Definicja interfejsu IGameState.
*
* Plik ten zawiera deklarację interfejsu IGameState, który definiuje
* podstawowe metody dla wszystkich stanów gry.
*/
#ifndef IGAME_STATE_H
#define IGAME_STATE_H

class Engine;
class EventManager;
class InputManager;
class Renderer;

/**
 * @interface IGameState
 * @brief Interfejs definiujacy podstawowy kontrakt dla stanow gry.
 *
 * Kazdy konkretny stan gry (np. menu glowne, rozgrywka, ekran opcji)
 * powinien implementowac ten interfejs, aby mogl byc zarzadzany
 * przez GameStateManager. Definiuje on metody cyklu zycia stanu
 * oraz metody do obslugi zdarzen, aktualizacji logiki i renderowania.
 */
class IGameState {
public:
    /**
     * @brief Wirtualny destruktor.
     * Niezbedny dla poprawnego zarzadzania pamiecia przy polimorfizmie.
     */
    virtual ~IGameState() = default;

    /**
     * @brief Metoda inicjalizujaca stan gry.
     * Wywolywana jednorazowo, gdy stan staje sie aktywny (np. po dodaniu na stos stanow).
     * Powinna ladowac zasoby specyficzne dla stanu, tworzyc obiekty gry, itp.
     * @param engine Wskaznik do glownego obiektu silnika, umozliwiajacy dostep do innych systemow.
     */
	virtual void init() = 0;      //TODO: Wywalić Engine* engine, bo jest singletonem i nie trzeba go przekazywać do każdego stanu

    /**
     * @brief Metoda sprzatajaca po stanie gry.
     * Wywolywana jednorazowo, tuz przed usunieciem stanu (np. przed zdjeciem ze stosu).
     * Powinna zwalniac zasoby zaalokowane przez stan.
     */
    virtual void cleanup() = 0;

    /**
     * @brief Metoda wywolywana, gdy stan jest przykrywany przez inny stan na stosie.
     * (np. gdy stan pauzy jest nakladany na stan rozgrywki).
     * Pozwala na zatrzymanie lub zawieszenie logiki, ktora nie powinna dzialac w tle.
     */
    virtual void pause() = 0;

    /**
     * @brief Metoda wywolywana, gdy stan powraca na wierzch stosu stanow.
     * (np. gdy stan pauzy jest zdejmowany ze stosu, a stan rozgrywki staje sie ponownie aktywny).
     * Pozwala na wznowienie logiki stanu.
     */
    virtual void resume() = 0;

    /**
     * @brief Metoda obslugujaca zdarzenia wejsciowe i systemowe dla aktywnego stanu.
     * Powinna byc wywolywana w kazdej klatce przed aktualizacja logiki.
     * @param inputManager Wskaznik do menedzera wejscia silnika.
     * @param eventManager Wskaznik do menedzera zdarzen silnika.
     */
    virtual void handleEvents(InputManager* inputManager, EventManager* eventManager) = 0;

    /**
     * @brief Metoda aktualizujaca logike stanu gry.
     * Powinna byc wywolywana w kazdej klatce.
     * @param deltaTime Czas (w sekundach), ktory uplynal od ostatniej klatki.
     */
    virtual void update(float deltaTime) = 0;

    /**
     * @brief Metoda renderujaca scene dla aktywnego stanu gry.
     * Powinna byc wywolywana w kazdej klatce po aktualizacji logiki.
     * @param renderer Wskaznik do renderera silnika.
     */
    virtual void render(Renderer* renderer) = 0;

protected:
    /**
     * @brief Chroniony konstruktor domyslny.
     * Zapobiega bezposredniemu tworzeniu instancji IGameState, poniewaz jest to klasa abstrakcyjna (interfejs).
     * Klasy pochodne beda mialy swoje wlasne konstruktory.
     */
    IGameState() = default;

    /**
     * @brief Wskaznik do glownego obiektu silnika.
     * Moze byc przechowywany przez konkretne stany gry po zainicjalizowaniu w metodzie init(),
     * aby umozliwic dostep do innych systemow silnika (np. ResourceManager, AudioManager).
     * Inicjalizowany na nullptr.
     */
    Engine* m_engine = nullptr;
};

#endif // IGAME_STATE_H
