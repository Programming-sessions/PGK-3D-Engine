/**
* @file IEventListener.h
* @brief Definicja interfejsu IEventListener.
*
* Plik ten zawiera deklarację interfejsu IEventListener, który musi być
* implementowany przez klasy chcące nasłuchiwać na zdarzenia
* w systemie zdarzeń.
*/
#ifndef IEVENT_LISTENER_H
#define IEVENT_LISTENER_H

// Deklaracja wyprzedzajaca dla klasy Event.
// Pelna definicja Event (z Event.h) bedzie potrzebna w plikach .cpp
// implementujacych IEventListener lub w plikach naglowkowych klas pochodnych,
// jesli operuja one na konkretnych typach zdarzen i ich skladowych.
class Event;

/**
 * @interface IEventListener
 * @brief Interfejs dla klas, ktore chca nasluchiwac i reagowac na zdarzenia systemowe.
 *
 * Klasy implementujace ten interfejs moga byc rejestrowane w EventManagerze,
 * aby otrzymywac powiadomienia o wystapieniu zdarzen, na ktore sa zainteresowane.
 */
class IEventListener {
public:
    /**
     * @brief Wirtualny destruktor.
     * Niezbedny dla poprawnego zarzadzania pamiecia przy polimorfizmie,
     * gdy obiekty nasluchujace sa przechowywane jako wskazniki do IEventListener.
     */
    virtual ~IEventListener() = default;

    /**
     * @brief Metoda wywolywana przez EventManager, gdy zdarzenie zostanie rozgloszone.
     *
     * Obiekt nasluchujacy jest odpowiedzialny za sprawdzenie typu zdarzenia
     * (np. za pomoca event.getEventType()) i, jesli jest to zdarzenie, na ktore oczekuje,
     * za odpowiednie rzutowanie referencji 'event' na konkretny typ zdarzenia
     * (np. static_cast<KeyPressedEvent&>(event)), aby uzyskac dostep
     * do specyficznych danych tego zdarzenia.
     *
     * @param event Referencja do obiektu bazowego zdarzenia.
     * @note Implementacja tej metody powinna rowniez rozwazyc ustawienie flagi
     * event.handled = true;, jesli zdarzenie zostalo w pelni obsluzone
     * i nie powinno byc dalej propagowane do innych sluchaczy (jesli system zdarzen to wspiera). TODO ???
     *
     * @par Przyklad uzycia w metodzie onEvent:
     * @code
     * if (event.getEventType() == EventType::KeyPressed) {
     * KeyPressedEvent& kpEvent = static_cast<KeyPressedEvent&>(event);
     * // Uzyj kpEvent.getKeyCode() do obslugi zdarzenia
     * // ...
     * event.handled = true; // Opcjonalnie, jesli zdarzenie zostalo obsluzone
     * } else if (event.getEventType() == EventType::MouseMoved) {
     * // ...
     * }
     * @endcode
     */
    virtual void onEvent(Event& event) = 0;
};

#endif // IEVENT_LISTENER_H
