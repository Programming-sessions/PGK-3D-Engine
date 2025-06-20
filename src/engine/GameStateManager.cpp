#include <glad/glad.h> // TODO: Wyjaśnić dlaczego to jest tu potrzebne
#include "GameStateManager.h"
#include "IGameState.h"   // Potrzebne dla definicji interfejsu IGameState
#include "Engine.h"       // Potrzebne dla wskaznika m_engine i przekazywania kontekstu
#include "Logger.h"

GameStateManager::GameStateManager(Engine* engine) : m_engine(engine) {
    if (!m_engine) {
        // W przypadku krytycznego bledu, jakim jest brak kontekstu silnika,
        // logujemy blad. mogloby to byc
        // rzucenie wyjatku lub inne dzialanie przerywajace.  TODO ???
        Logger::getInstance().error("GameStateManager: Konstruktor otrzymal pusty wskaznik (nullptr) do obiektu Engine!");
        // np. throw std::runtime_error("Engine context cannot be null for GameStateManager");
    }
    Logger::getInstance().info("GameStateManager utworzony.");
}

GameStateManager::~GameStateManager() {
    // Wywolanie metody cleanup() w destruktorze zapewnia, ze wszystkie
    // zarzadzane stany gry zostana poprawnie zwolnione (ich metody cleanup() wywolane)
    // przed zniszczeniem samego menedzera stanow.
    Logger::getInstance().info("GameStateManager: Rozpoczynanie sprzatania w destruktorze...");
    cleanup();
    Logger::getInstance().info("GameStateManager zniszczony.");
}

void GameStateManager::pushState(std::unique_ptr<IGameState> state) {
    // Sprawdzenie, czy probujemy dodac prawidlowy (niepusty) stan.
    if (!state) {
        Logger::getInstance().warning("GameStateManager: Proba dodania pustego stanu (null unique_ptr) na stos.");
        return; // Przerywamy operacje, jesli stan jest nieprawidlowy.
    }

    Logger::getInstance().info("GameStateManager: Dodawanie nowego stanu na stos (pushState)...");

    // Jesli na stosie znajduja sie juz jakies stany, pauzujemy ten na wierzcholku.
    // Pozwala to np. na wyswietlenie menu pauzy nad stanem gry bez zatrzymywania go calkowicie.
    if (!m_states.empty()) {
        Logger::getInstance().debug("GameStateManager: Pauzowanie poprzedniego stanu...");
        m_states.back()->pause();
    }

    // Inicjalizacja nowego stanu. Przekazujemy wskaznik do silnika,
    // aby stan mogl uzyskac dostep do innych systemow (np. InputManager, Renderer).
    Logger::getInstance().debug("GameStateManager: Inicjalizacja nowego stanu...");
    state->init();

    // Dodanie nowego stanu na wierzch stosu.
    // std::move jest uzywane do transferu wlasnosci unique_ptr.
    m_states.push_back(std::move(state));
    Logger::getInstance().info("GameStateManager: Nowy stan dodany. Liczba aktywnych stanow: " + std::to_string(m_states.size()));
}

void GameStateManager::popState() {
    // Sprawdzenie, czy stos nie jest pusty, aby uniknac bledu przy probie usuniecia.
    if (m_states.empty()) {
        Logger::getInstance().warning("GameStateManager: Proba usuniecia stanu (popState) z pustego stosu.");
        return; // Przerywamy, jesli nie ma stanow do usuniecia.
    }

    Logger::getInstance().info("GameStateManager: Usuwanie stanu z wierzchu stosu (popState)...");

    // Wywolanie metody cleanup() dla stanu, ktory ma byc usuniety.
    // Pozwala to na zwolnienie zasobow specyficznych dla tego stanu.
    Logger::getInstance().debug("GameStateManager: Sprzatanie usuwanego stanu...");
    m_states.back()->cleanup();

    // Fizyczne usuniecie wskaznika do stanu ze stosu (wektora).
    // unique_ptr automatycznie zwolni pamiec po obiekcie stanu.
    m_states.pop_back();
    Logger::getInstance().debug("GameStateManager: Stan usuniety ze stosu.");

    // Jesli po usunieciu na stosie pozostal jakis stan, wznawiamy jego dzialanie.
    // Staje sie on nowym aktywnym stanem.
    if (!m_states.empty()) {
        Logger::getInstance().debug("GameStateManager: Wznawianie dzialania nowego stanu na wierzcholku...");
        m_states.back()->resume();
    }
    Logger::getInstance().info("GameStateManager: Stan usuniety. Liczba aktywnych stanow: " + std::to_string(m_states.size()));
    if (m_states.empty()) {
        Logger::getInstance().info("GameStateManager: Stos stanow jest teraz pusty.");
    }
}

void GameStateManager::changeState(std::unique_ptr<IGameState> state) {
    // Sprawdzenie, czy probujemy zmienic na prawidlowy (niepusty) stan.
    if (!state) {
        Logger::getInstance().warning("GameStateManager: Proba zmiany na pusty stan (null unique_ptr).");
        return; // Przerywamy, jesli nowy stan jest nieprawidlowy.
    }

    Logger::getInstance().info("GameStateManager: Zmiana aktywnego stanu (changeState)...");

    // Jesli na stosie znajduje sie jakis stan, usuwamy go.
    // Obejmuje to wywolanie jego metody cleanup().
    if (!m_states.empty()) {
        Logger::getInstance().debug("GameStateManager: Sprzatanie i usuwanie poprzedniego stanu...");
        m_states.back()->cleanup();
        m_states.pop_back();
    }

    // Inicjalizacja nowego stanu, ktory ma stac sie aktywny.
    Logger::getInstance().debug("GameStateManager: Inicjalizacja nowego stanu...");
    state->init();

    // Dodanie nowego stanu na (teraz pusty lub krotszy) stos.
    m_states.push_back(std::move(state));
    Logger::getInstance().info("GameStateManager: Stan zmieniony. Liczba aktywnych stanow: " + std::to_string(m_states.size()));
}

void GameStateManager::handleEventsCurrentState(InputManager* inputManager, EventManager* eventManager) {
    // Sprawdzenie, czy na stosie jest jakikolwiek aktywny stan.
    if (!m_states.empty()) {
        // Przekazanie obslugi zdarzen do stanu znajdujacego sie na wierzchu stosu.
        m_states.back()->handleEvents(inputManager, eventManager);
    }
    else {
        Logger::getInstance().warning("GameStateManager: Proba obslugi zdarzen na pustym stosie stanow.");
    }
}

void GameStateManager::updateCurrentState(float deltaTime) {
    // Sprawdzenie, czy na stosie jest jakikolwiek aktywny stan.
    if (!m_states.empty()) {
        // Przekazanie logiki aktualizacji do stanu znajdujacego sie na wierzchu stosu.
        m_states.back()->update(deltaTime);
    }
    else {
        Logger::getInstance().warning("GameStateManager: Proba aktualizacji logiki na pustym stosie stanow.");
    }
}

void GameStateManager::renderCurrentState(Renderer* renderer) {
    // Sprawdzenie, czy na stosie jest jakikolwiek aktywny stan.
    if (!m_states.empty()) {
        // Przekazanie renderowania do stanu znajdujacego sie na wierzchu stosu.
        // Domyślnie renderowany jest tylko stan na samej górze.
        m_states.back()->render(renderer);

        // TODO: Rozwazyc implementacje renderowania wielu stanow, jesli potrzebne.
        // Przyklad: renderowanie przezroczystego menu pauzy nad stanem gry.
        // Wymagaloby to iteracji po czesci stosu (lub calym) i odpowiedniego
        // zarzadzania przezroczystoscia lub buforem ramki.
        // for (const auto& state : m_states) { // Lub od pewnego momentu
        //     state->render(renderer); // Wymaga przemyslenia kolejnosci i efektow
        // }
    }
    else {
        Logger::getInstance().warning("GameStateManager: Proba renderowania na pustym stosie stanow.");
    }
}

bool GameStateManager::isEmpty() const {
    // Zwraca true, jesli wektor (traktowany jako stos) stanow jest pusty.
    return m_states.empty();
}

void GameStateManager::cleanup() {
    Logger::getInstance().info("GameStateManager: Rozpoczynanie sprzatania wszystkich stanow...");
    // Petla usuwajaca wszystkie stany ze stosu, zaczynajac od wierzcholka.
    // Dla kazdego stanu wywolywana jest jego metoda cleanup() przed usunieciem.
    while (!m_states.empty()) {
        Logger::getInstance().debug("GameStateManager: Sprzatanie stanu z wierzchu stosu...");
        m_states.back()->cleanup();
        m_states.pop_back(); // unique_ptr automatycznie zwolni pamiec
    }
    Logger::getInstance().info("GameStateManager: Wszystkie stany gry zostaly sprzatniete i usuniete ze stosu.");
}

// TODO: Pomyslec o implementacji "Pending Actions" (oczekujacych dzialan).
// Czasami bezposrednie modyfikowanie stosu stanow (push/pop/change) w trakcie
// iteracji po nim (np. w metodzie update stanu) moze byc problematyczne.
// System oczekujacych dzialan polegalby na dodawaniu zadanych zmian do kolejki,
// a nastepnie przetwarzaniu tej kolejki w bezpiecznym momencie, np. na koncu
// glownej petli aktualizacji GameStateManagera.
// void GameStateManager::processPendingActions() {
//     for (const auto& action : m_pendingQueue) {
//         // wykonaj akcje (push, pop, change)
//     }
//     m_pendingQueue.clear();
// }
