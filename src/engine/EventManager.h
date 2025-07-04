/**
* @file EventManager.h
* @brief Definicja klasy EventManager.
*
* Plik ten zawiera deklarację klasy EventManager, która zarządza
* kolejką zdarzeń i powiadamia zarejestrowanych słuchaczy
* o wystąpieniu zdarzeń.
*/
#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <vector>
#include <map>
#include <algorithm>  // Dla std::remove, std::find

#include "Event.h"          // Potrzebne dla definicji EventType oraz klasy bazowej Event
#include "IEventListener.h" // Potrzebne dla definicji interfejsu IEventListener

// Usunieto niebezposrednio uzywane w tym pliku naglowkowym:
// #include <functional> // Bylo dla std::function, ale nie jest uzywane w obecnym API
// #include "Logger.h"    // Logger jest uzywany w .cpp, niekoniecznie potrzebny w .h

// Deklaracja wyprzedzajaca nie jest juz potrzebna dla IEventListener,
// poniewaz IEventListener.h jest dolaczony powyzej.
// class IEventListener;

/**
 * @class EventManager
 * @brief Klasa odpowiedzialna za zarzadzanie systemem zdarzen w aplikacji.
 *
 * Umozliwia obiektom (implementujacym IEventListener) subskrybowanie
 * okreslonych typow zdarzen (EventType) oraz rozglaszanie (dispatching)
 * zdarzen do wszystkich zainteresowanych sluchaczy.
 * EventManager nie przejmuje wlasnosci nad wskaznikami do sluchaczy.
 */
class EventManager {
public:
    /**
     * @brief Konstruktor domyslny EventManagera.
     */
    EventManager();

    /**
     * @brief Destruktor EventManagera.
     * Czysci mapy sluchaczy. Nie zwalnia pamieci po samych sluchaczach,
     * poniewaz nie jest ich wlascicielem.
     */
    ~EventManager();

    /**
     * @brief Subskrybuje sluchacza (listener) na okreslony typ zdarzenia.
     * Gdy zdarzenie danego typu zostanie rozgloszone, metoda onEvent() sluchacza
     * zostanie wywolana.
     * @param type Typ zdarzenia (EventType), na ktore sluchacz chce sie zapisac.
     * @param listener Wskaznik do obiektu implementujacego IEventListener.
     * @note Sluchacz nie zostanie dodany, jesli jest juz zasubskrybowany na dany typ zdarzenia.
     */
    void subscribe(EventType type, IEventListener* listener);

    /**
     * @brief Anuluje subskrypcje sluchacza na okreslony typ zdarzenia.
     * Sluchacz przestanie otrzymywac powiadomienia o zdarzeniach tego typu.
     * @param type Typ zdarzenia (EventType), z ktorego sluchacz ma byc wypisany.
     * @param listener Wskaznik do obiektu sluchacza do wypisania.
     */
    void unsubscribe(EventType type, IEventListener* listener);

    /**
     * @brief Anuluje subskrypcje sluchacza na wszystkie typy zdarzen.
     * Przeszukuje wszystkie listy sluchaczy i usuwa podany obiekt sluchacza
     * z kazdej z nich.
     * @param listener Wskaznik do obiektu sluchacza, ktory ma byc wypisany ze wszystkich zdarzen.
     */
    void unsubscribeAll(IEventListener* listener);

    /**
     * @brief Rozglasza zdarzenie do wszystkich sluchaczy zasubskrybowanych na jego typ.
     * Zdarzenie jest przekazywane przez referencje i moze byc oznaczone jako
     * obsluzone (event.handled = true) przez jednego ze sluchaczy, co moze
     * (w zaleznosci od logiki w dispatch) zatrzymac dalsza propagacje.
     * @param event Referencja do obiektu zdarzenia, ktore ma zostac rozgloszone.
     */
    void dispatch(Event& event);

private:
    /**
     * @brief Mapa przechowujaca sluchaczy dla poszczegolnych typow zdarzen.
     * Kluczem mapy jest EventType, a wartoscia jest wektor wskaznikow
     * do obiektow IEventListener zasubskrybowanych na ten typ zdarzenia.
     */
    std::map<EventType, std::vector<IEventListener*>> m_listeners;
};

#endif // EVENT_MANAGER_H
