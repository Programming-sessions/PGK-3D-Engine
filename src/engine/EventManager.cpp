#include "EventManager.h"
#include "Logger.h" // Potrzebne do logowania informacji, ostrzezen.
#include <string>

EventManager::EventManager() {
    // Konstruktor EventManagera.
    // W momencie tworzenia, mapa sluchaczy m_listeners jest pusta.
    Logger::getInstance().info("EventManager utworzony.");
}

EventManager::~EventManager() {
    // Destruktor EventManagera.
    // Sluchacze (IEventListener*) sa przechowywani jako surowe wskazniki.
    // EventManager nie jest wlascicielem tych obiektow, wiec nie powinien
    // probowac ich usuwac (delete). Odpowiedzialnosc za cykl zycia sluchaczy
    // spoczywa na kodzie, ktory je tworzy i rejestruje.
    // Wystarczy wyczyscic mape, co zwolni wektory, ale nie same obiekty sluchaczy.
    m_listeners.clear();
    Logger::getInstance().info("EventManager zniszczony. Wszystkie listy sluchaczy wyczyszczone.");
}

void EventManager::subscribe(EventType type, IEventListener* listener) {
    // Sprawdzenie, czy przekazany wskaznik do sluchacza nie jest pusty.
    if (!listener) {
        Logger::getInstance().warning("EventManager: Proba subskrypcji pustego sluchacza (nullptr) dla typu zdarzenia: " + std::to_string(static_cast<int>(type)));
        return; // Przerywamy operacje, jesli sluchacz jest nieprawidlowy.
    }

    // Pobranie referencji do wektora sluchaczy dla danego typu zdarzenia.
    // Jesli dla danego typu zdarzenia nie ma jeszcze zadnych sluchaczy,
    // operator[] utworzy nowy pusty wektor w mapie.
    std::vector<IEventListener*>& listenersForType = m_listeners[type];

    // Sprawdzenie, czy sluchacz nie jest juz zasubskrybowany na ten typ zdarzenia.
    // Zapobiega to duplikatom na liscie sluchaczy.
    if (std::find(listenersForType.begin(), listenersForType.end(), listener) == listenersForType.end()) {
        // Jesli sluchacz nie zostal znaleziony, dodajemy go do wektora.
        listenersForType.push_back(listener);
        Logger::getInstance().debug("EventManager: Sluchacz zasubskrybowany na typ zdarzenia: " + std::to_string(static_cast<int>(type)));
    }
    else {
        // Sluchacz jest juz na liscie, wiec nie robimy nic.
        Logger::getInstance().debug("EventManager: Sluchacz jest juz zasubskrybowany na typ zdarzenia: " + std::to_string(static_cast<int>(type)));
    }
}

void EventManager::unsubscribe(EventType type, IEventListener* listener) {
    // Sprawdzenie, czy przekazany wskaznik do sluchacza nie jest pusty.
    if (!listener) {
        Logger::getInstance().warning("EventManager: Proba anulowania subskrypcji pustego sluchacza (nullptr) dla typu zdarzenia: " + std::to_string(static_cast<int>(type)));
        return; // Przerywamy, jesli sluchacz jest nieprawidlowy.
    }

    // Proba znalezienia wektora sluchaczy dla danego typu zdarzenia w mapie.
    auto mapIterator = m_listeners.find(type);
    if (mapIterator != m_listeners.end()) {
        // Jesli znaleziono wpis dla danego typu zdarzenia, uzyskujemy dostep do wektora sluchaczy.
        std::vector<IEventListener*>& listenersForType = mapIterator->second;

        // Uzycie idiom "erase-remove" do usuniecia wszystkich wystapien danego sluchacza z wektora.
        // std::remove przesuwa elementy do usuniecia na koniec wektora i zwraca iterator
        // do pierwszego z tych elementow. Nastepnie erase usuwa elementy od tego iteratora do konca.
        auto newEnd = std::remove(listenersForType.begin(), listenersForType.end(), listener);
        if (newEnd != listenersForType.end()) {
            listenersForType.erase(newEnd, listenersForType.end());
            Logger::getInstance().debug("EventManager: Sluchacz anulowal subskrypcje dla typu zdarzenia: " + std::to_string(static_cast<int>(type)));
        }
        else {
            // Sluchacz nie zostal znaleziony w wektorze dla tego typu zdarzenia.
            Logger::getInstance().debug("EventManager: Sluchacz nie znaleziony dla typu zdarzenia: " + std::to_string(static_cast<int>(type)) + " podczas proby anulowania subskrypcji.");
        }

        // Jesli wektor sluchaczy dla danego typu zdarzenia stal sie pusty,
        // mozna usunac caly wpis (klucz-wartosc) z mapy, aby ja uporzadkowac.
        // if (listenersForType.empty()) {
        //     m_listeners.erase(mapIterator);
        //     Logger::getInstance().debug("EventManager: Usunieto pusty wpis dla typu zdarzenia: " + std::to_string(static_cast<int>(type)) + " z mapy sluchaczy.");
        // }
    }
    else {
        // Nie znaleziono zadnych sluchaczy dla tego typu zdarzenia.
        Logger::getInstance().debug("EventManager: Brak sluchaczy dla typu zdarzenia: " + std::to_string(static_cast<int>(type)) + " podczas proby anulowania subskrypcji.");
    }
}

void EventManager::unsubscribeAll(IEventListener* listener) {
    // Sprawdzenie, czy przekazany wskaznik do sluchacza nie jest pusty.
    if (!listener) {
        Logger::getInstance().warning("EventManager: Proba anulowania subskrypcji pustego sluchacza (nullptr) ze wszystkich zdarzen.");
        return; // Przerywamy, jesli sluchacz jest nieprawidlowy.
    }

    Logger::getInstance().debug("EventManager: Anulowanie subskrypcji sluchacza ze wszystkich typow zdarzen...");
    // Iteracja przez wszystkie pary (typ zdarzenia, wektor sluchaczy) w mapie.
    for (auto& pair : m_listeners) {
        // Dla kazdego typu zdarzenia, uzyskujemy referencje do jego wektora sluchaczy.
        std::vector<IEventListener*>& listenersForType = pair.second;
        // Uzycie idiom "erase-remove" do usuniecia sluchacza z biezacego wektora.
        listenersForType.erase(std::remove(listenersForType.begin(), listenersForType.end(), listener), listenersForType.end());
        // Nie ma potrzeby sprawdzac, czy element zostal usuniety, poniewaz jesli go nie bylo, nic sie nie stanie.
    }
    // po usunieciu sluchacza ze wszystkich list, mozna by przejsc przez mape
    // i usunac wpisy, ktorych wektory staly sie puste.
    // for (auto it = m_listeners.begin(); it != m_listeners.end(); ) {
    //     if (it->second.empty()) {
    //         it = m_listeners.erase(it);
    //     } else {
    //         ++it;
    //     }
    // }
    Logger::getInstance().debug("EventManager: Sluchacz anulowal subskrypcje ze wszystkich typow zdarzen.");
}


void EventManager::dispatch(Event& event) {
    EventType type = event.getEventType();
    // Logowanie informacji o rozglaszanym zdarzeniu (moze byc bardzo gadatliwe).
    // Logger::getInstance().debug("EventManager: Rozglaszanie zdarzenia typu: " + std::to_string(static_cast<int>(type)) + ", Nazwa: " + event.getName());

    // Proba znalezienia wektora sluchaczy dla typu rozglaszanego zdarzenia.
    auto mapIterator = m_listeners.find(type);
    if (mapIterator != m_listeners.end()) {
        // Jesli sa jacys sluchacze dla tego typu zdarzenia:
        // Tworzymy kopie wektora sluchaczy przed iteracja.
        // Jest to wazne zabezpieczenie, poniewaz sluchacz w swojej metodzie onEvent()
        // moze chciec anulowac swoja subskrypcje lub subskrypcje innego sluchacza,
        // co modyfikowaloby oryginalny wektor m_listeners[type] podczas iteracji po nim,
        // prowadzac do uniewaznienia iteratorow i potencjalnych bledow.
        // Praca na kopii pozwala uniknac tych problemow.
        std::vector<IEventListener*> listenersCopy = mapIterator->second;
        // Logger::getInstance().debug("EventManager: Znaleziono " + std::to_string(listenersCopy.size()) + " sluchaczy dla zdarzenia typu " + std::to_string(static_cast<int>(type)));

        for (IEventListener* listener : listenersCopy) {
            // Sprawdzenie, czy zdarzenie zostalo juz oznaczone jako "obsluzone"
            // przez poprzedniego sluchacza w tej samej iteracji.
            // Pozwala to na zaimplementowanie mechanizmu, gdzie pierwszy sluchacz,
            // ktory w pelni obsluzy zdarzenie, moze zatrzymac jego dalsza propagacje.
            if (event.handled) {
                int event_type_value = static_cast<int>(type);
                std::string message = "EventManager: Brak sluchaczy dla zdarzenia typu: " + std::to_string(event_type_value);
                Logger::getInstance().debug(message);
                break; // Przerywamy petle, nie wysylamy do kolejnych sluchaczy.
            }

            // Dodatkowe sprawdzenie (opcjonalne, ale moze zwiekszyc odpornosc):
            // Upewniamy sie, ze sluchacz, do ktorego chcemy wyslac zdarzenie (z kopii),
            // nadal istnieje w oryginalnej liscie sluchaczy. Mogl zostac usuniety
            // przez innego sluchacza w tej samej petli dispatch, zanim doszlismy do niego.
            // Dla wiekszosci przypadkow iteracja po kopii jest wystarczajaca.
            // Ten dodatkowy find moze byc kosztowny, jesli listy sa dlugie.
            // if (std::find(m_listeners[type].begin(), m_listeners[type].end(), listener) != m_listeners[type].end()) {
            //     listener->onEvent(event);
            // } else {
            //     Logger::getInstance().debug("EventManager: Sluchacz zostal usuniety w trakcie rozglaszania, pomijanie.");
            // }
            listener->onEvent(event); // Bezposrednie wywolanie na podstawie kopii
        }
    }
    else {
        // Brak sluchaczy dla tego typu zdarzenia.
        // Logger::getInstance().debug("EventManager: Brak sluchaczy dla zdarzenia typu: " + std::to_string(static_cast<int>(type)));
    }
}
