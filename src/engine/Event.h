/**
* @file Event.h
* @brief Definicja struktur i typów zdarzeń.
*
* Plik ten zawiera definicje różnych typów zdarzeń używanych
* w systemie zarządzania zdarzeniami silnika.
*/
#ifndef EVENT_H
#define EVENT_H

#include <string>      // Dla std::string, std::to_string
#include <glm/glm.hpp> // Dla glm::vec2 w MouseMovedEvent
#include <functional>  // Potencjalnie dla std::function, chociaz obecnie nieuzywane bezposrednio w tym pliku

// Deklaracja wyprzedzajaca dla ICollidable, uzywanego w CollisionEvent.
class ICollidable;

/**
 * @enum EventType
 * @brief Definiuje wszystkie mozliwe typy zdarzen w systemie.
 * Kazde zdarzenie w systemie musi miec przypisany jeden z tych typow.
 */
enum class EventType {
    None = 0,               ///< Typ nieokreslony lub zdarzenie puste.
    WindowClose,            ///< Zdarzenie zamkniecia okna.
    WindowResize,           ///< Zdarzenie zmiany rozmiaru okna.
    WindowFocus,            ///< Zdarzenie uzyskania fokusu przez okno.
    WindowLostFocus,        ///< Zdarzenie utraty fokusu przez okno.
    WindowMoved,            ///< Zdarzenie przesuniecia okna.
    AppTick,                ///< Zdarzenie cyklicznego "tykniecia" aplikacji (np. dla stalego kroku czasowego).
    AppUpdate,              ///< Zdarzenie aktualizacji logiki aplikacji.
    AppRender,              ///< Zdarzenie renderowania klatki przez aplikacje.
    KeyPressed,             ///< Zdarzenie wcisniecia klawisza.
    KeyReleased,            ///< Zdarzenie zwolnienia klawisza.
    KeyTyped,               ///< Zdarzenie wprowadzenia znaku (po wcisnieciu klawisza).
    MouseButtonPressed,     ///< Zdarzenie wcisniecia przycisku myszy.
    MouseButtonReleased,    ///< Zdarzenie zwolnienia przycisku myszy.
    MouseMoved,             ///< Zdarzenie ruchu myszy.
    MouseScrolled,          ///< Zdarzenie przewiniecia rolka myszy.
    Collision               ///< Zdarzenie wykrycia kolizji miedzy dwoma obiektami.
};

/**
 * @def EVENT_CLASS_TYPE(type)
 * @brief Makro pomocnicze do implementacji metod zwiazanych z typem zdarzenia w klasach pochodnych Event.
 * Definiuje statyczna metode getStaticType() zwracajaca EventType,
 * wirtualna metode getEventType() zwracajaca typ zdarzenia instancji,
 * oraz wirtualna metode getName() zwracajaca nazwe typu zdarzenia jako ciag znakow.
 * @param type Nazwa typu zdarzenia z enum class EventType (np. KeyPressed).
 */
#define EVENT_CLASS_TYPE(type) \
    static EventType getStaticType() { return EventType::type; } \
    virtual EventType getEventType() const override { return getStaticType(); } \
    virtual const char* getName() const override { return #type; }

 /**
  * @enum EventCategory
  * @brief Definiuje kategorie, do ktorych moga nalezec zdarzenia.
  * Uzywane do filtrowania zdarzen. Zdarzenie moze nalezec do wielu kategorii jednoczesnie
  * poprzez kombinacje bitowe flag.
  */
enum EventCategory {
    None = 0,      ///< Brak kategorii.
    EventCategoryApplication = 1 << 0, ///< Kategoria dla zdarzen zwiazanych z cyklem zycia aplikacji.
    EventCategoryInput = 1 << 1, ///< Kategoria dla ogolnych zdarzen wejsciowych.
    EventCategoryKeyboard = 1 << 2, ///< Kategoria dla zdarzen klawiatury (podkategoria Input).
    EventCategoryMouse = 1 << 3, ///< Kategoria dla zdarzen myszy (podkategoria Input).
    EventCategoryMouseButton = 1 << 4, ///< Kategoria dla zdarzen przyciskow myszy (podkategoria Mouse).
    EventCategoryGameLogic = 1 << 5  ///< Kategoria dla zdarzen zwiazanych z logika gry (np. kolizje).
};

/**
 * @def EVENT_CLASS_CATEGORY(category)
 * @brief Makro pomocnicze do implementacji metody getCategoryFlags() w klasach pochodnych Event.
 * Definiuje wirtualna metode getCategoryFlags() zwracajaca flagi kategorii zdarzenia.
 * @param category Kombinacja bitowa flag z enum EventCategory.
 */
#define EVENT_CLASS_CATEGORY(category) \
    virtual int getCategoryFlags() const override { return category; }

 /**
  * @class Event
  * @brief Klasa bazowa dla wszystkich zdarzen w systemie.
  *
  * Definiuje podstawowy interfejs dla zdarzen, wlaczajac w to metody
  * do pobierania typu zdarzenia, jego nazwy, kategorii oraz statusu obslugi.
  * Kazde konkretne zdarzenie (np. KeyPressedEvent) powinno dziedziczyc po tej klasie.
  */
class Event {
public:
    /** @brief Wirtualny destruktor. */
    virtual ~Event() = default;

    /**
     * @brief Flaga wskazujaca, czy zdarzenie zostalo juz obsluzone.
     * Sluchacz zdarzen moze ustawic te flage na true, aby zasygnalizowac,
     * ze zdarzenie zostalo przetworzone i potencjalnie zatrzymac jego dalsza propagacje.
     */
    bool handled = false;

    /**
     * @brief Wirtualna metoda zwracajaca typ zdarzenia.
     * @return Wartosc EventType odpowiadajaca typowi zdarzenia.
     */
    virtual EventType getEventType() const = 0;

    /**
     * @brief Wirtualna metoda zwracajaca nazwe typu zdarzenia jako ciag znakow.
     * Przydatne do debugowania i logowania.
     * @return Staly wskaznik do ciagu znakow reprezentujacego nazwe zdarzenia.
     */
    virtual const char* getName() const = 0;

    /**
     * @brief Wirtualna metoda zwracajaca flagi kategorii, do ktorych nalezy zdarzenie.
     * @return Kombinacja bitowa flag z enum EventCategory.
     */
    virtual int getCategoryFlags() const = 0;

    /**
     * @brief Wirtualna metoda zwracajaca reprezentacje zdarzenia jako string.
     * Domyślnie zwraca nazwe zdarzenia. Moze byc nadpisana przez klasy pochodne
     * w celu dostarczenia bardziej szczegolowych informacji.
     * @return String opisujacy zdarzenie.
     */
    virtual std::string toString() const { return getName(); }

    /**
     * @brief Sprawdza, czy zdarzenie nalezy do podanej kategorii.
     * @param category Kategoria do sprawdzenia (wartosc z enum EventCategory).
     * @return True, jesli zdarzenie nalezy do podanej kategorii, false w przeciwnym wypadku.
     */
    bool isInCategory(EventCategory category) const {
        return getCategoryFlags() & category;
    }
};

// --- Zdarzenia Okna ---

/**
 * @class WindowResizeEvent
 * @brief Zdarzenie generowane podczas zmiany rozmiaru okna aplikacji.
 * Przechowuje nowy szerokosc i wysokosc okna.
 */
class WindowResizeEvent : public Event {
public:
    /**
     * @brief Konstruktor.
     * @param width Nowa szerokosc okna.
     * @param height Nowa wysokosc okna.
     */
    WindowResizeEvent(unsigned int width, unsigned int height) : m_Width(width), m_Height(height) {}

    /** @brief Zwraca nowa szerokosc okna. */
    unsigned int getWidth() const { return m_Width; }
    /** @brief Zwraca nowa wysokosc okna. */
    unsigned int getHeight() const { return m_Height; }

    std::string toString() const override { return "WindowResizeEvent: " + std::to_string(m_Width) + ", " + std::to_string(m_Height); }

    EVENT_CLASS_TYPE(WindowResize)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
private:
    unsigned int m_Width, m_Height;
};

/**
 * @class WindowCloseEvent
 * @brief Zdarzenie generowane, gdy uzytkownik probuje zamknac okno aplikacji.
 */
class WindowCloseEvent : public Event {
public:
    WindowCloseEvent() = default;
    EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
};

// --- Zdarzenia Klawiatury ---

/**
 * @class KeyEvent
 * @brief Klasa bazowa dla zdarzen zwiazanych z klawiatura.
 * Przechowuje kod wcisnietego/zwolnionego klawisza.
 */
class KeyEvent : public Event {
public:
    /** @brief Zwraca kod klawisza (np. GLFW_KEY_A). */
    int getKeyCode() const { return m_KeyCode; }

    EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
protected:
    /**
     * @brief Chroniony konstruktor.
     * @param keycode Kod klawisza.
     */
    KeyEvent(int keycode) : m_KeyCode(keycode) {}
    int m_KeyCode;
};

/**
 * @class KeyPressedEvent
 * @brief Zdarzenie generowane przy wcisnieciu klawisza.
 * Moze zawierac informacje o liczbie powtorzen (dla przytrzymanych klawiszy).
 */
class KeyPressedEvent : public KeyEvent {
public:
    /**
     * @brief Konstruktor.
     * @param keycode Kod wcisnietego klawisza.
     * @param repeatCount Liczba powtorzen (0 dla pierwszego wcisniecia, >0 dla kolejnych przy przytrzymaniu).
     */
    KeyPressedEvent(int keycode, int repeatCount) : KeyEvent(keycode), m_RepeatCount(repeatCount) {}

    /** @brief Zwraca liczbe powtorzen wcisniecia klawisza. */
    int getRepeatCount() const { return m_RepeatCount; }

    std::string toString() const override { return "KeyPressedEvent: " + std::to_string(m_KeyCode) + " (" + std::to_string(m_RepeatCount) + " repeats)"; }

    EVENT_CLASS_TYPE(KeyPressed)
private:
    int m_RepeatCount;
};

/**
 * @class KeyReleasedEvent
 * @brief Zdarzenie generowane przy zwolnieniu klawisza.
 */
class KeyReleasedEvent : public KeyEvent {
public:
    /**
     * @brief Konstruktor.
     * @param keycode Kod zwolnionego klawisza.
     */
    KeyReleasedEvent(int keycode) : KeyEvent(keycode) {}

    std::string toString() const override { return "KeyReleasedEvent: " + std::to_string(m_KeyCode); }

    EVENT_CLASS_TYPE(KeyReleased)
};

/**
 * @class KeyTypedEvent
 * @brief Zdarzenie generowane po wcisnieciu klawisza, reprezentujace wprowadzony znak (kod Unicode).
 * Rozni sie od KeyPressedEvent, poniewaz KeyTyped odnosi sie do znaku, a nie fizycznego klawisza
 * (np. Shift + 'a' da KeyTyped dla 'A').
 */
class KeyTypedEvent : public KeyEvent {
public:
    /**
     * @brief Konstruktor.
     * @param keycode Kod Unicode wprowadzonego znaku.
     */
    KeyTypedEvent(int keycode) : KeyEvent(keycode) {} // m_KeyCode tutaj przechowuje codepoint

    std::string toString() const override {
        // Proba wyswietlenia znaku, jesli jest drukowalny ASCII
        if (m_KeyCode >= 32 && m_KeyCode <= 126) {
            return "KeyTypedEvent: '" + std::string(1, static_cast<char>(m_KeyCode)) + "' (codepoint: " + std::to_string(m_KeyCode) + ")";
        }
        return "KeyTypedEvent: (codepoint: " + std::to_string(m_KeyCode) + ")";
    }

    EVENT_CLASS_TYPE(KeyTyped)
};

// --- Zdarzenia Myszy ---

/**
 * @class MouseMovedEvent
 * @brief Zdarzenie generowane podczas ruchu kursora myszy.
 * Przechowuje nowa pozycje X i Y kursora.
 */
class MouseMovedEvent : public Event {
public:
    /**
     * @brief Konstruktor.
     * @param x Nowa pozycja X kursora.
     * @param y Nowa pozycja Y kursora.
     */
    MouseMovedEvent(float x, float y) : m_MouseX(x), m_MouseY(y) {}

    /** @brief Zwraca nowa pozycje X kursora. */
    float getX() const { return m_MouseX; }
    /** @brief Zwraca nowa pozycje Y kursora. */
    float getY() const { return m_MouseY; }
    /** @brief Zwraca nowa pozycje kursora jako glm::vec2. */
    glm::vec2 getPosition() const { return { m_MouseX, m_MouseY }; }

    std::string toString() const override { return "MouseMovedEvent: " + std::to_string(m_MouseX) + ", " + std::to_string(m_MouseY); }

    EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
private:
    float m_MouseX, m_MouseY;
};

/**
 * @class MouseScrolledEvent
 * @brief Zdarzenie generowane podczas przewijania rolka myszy.
 * Przechowuje przesuniecie w osi X i Y.
 */
class MouseScrolledEvent : public Event {
public:
    /**
     * @brief Konstruktor.
     * @param xOffset Przesuniecie rolki w osi X.
     * @param yOffset Przesuniecie rolki w osi Y.
     */
    MouseScrolledEvent(float xOffset, float yOffset) : m_XOffset(xOffset), m_YOffset(yOffset) {}

    /** @brief Zwraca przesuniecie rolki w osi X. */
    float getXOffset() const { return m_XOffset; }
    /** @brief Zwraca przesuniecie rolki w osi Y. */
    float getYOffset() const { return m_YOffset; }

    std::string toString() const override { return "MouseScrolledEvent: " + std::to_string(m_XOffset) + ", " + std::to_string(m_YOffset); }

    EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)
private:
    float m_XOffset, m_YOffset;
};

/**
 * @class MouseButtonEvent
 * @brief Klasa bazowa dla zdarzen zwiazanych z przyciskami myszy.
 * Przechowuje kod wcisnietego/zwolnionego przycisku.
 */
class MouseButtonEvent : public Event {
public:
    /** @brief Zwraca kod przycisku myszy (np. GLFW_MOUSE_BUTTON_LEFT). */
    int getMouseButton() const { return m_Button; }

    EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput | EventCategoryMouseButton)
protected:
    /**
     * @brief Chroniony konstruktor.
     * @param button Kod przycisku myszy.
     */
    MouseButtonEvent(int button) : m_Button(button) {}
    int m_Button;
};

/**
 * @class MouseButtonPressedEvent
 * @brief Zdarzenie generowane przy wcisnieciu przycisku myszy.
 */
class MouseButtonPressedEvent : public MouseButtonEvent {
public:
    /**
     * @brief Konstruktor.
     * @param button Kod wcisnietego przycisku myszy.
     */
    MouseButtonPressedEvent(int button) : MouseButtonEvent(button) {}

    std::string toString() const override { return "MouseButtonPressedEvent: " + std::to_string(m_Button); }

    EVENT_CLASS_TYPE(MouseButtonPressed)
};

/**
 * @class MouseButtonReleasedEvent
 * @brief Zdarzenie generowane przy zwolnieniu przycisku myszy.
 */
class MouseButtonReleasedEvent : public MouseButtonEvent {
public:
    /**
     * @brief Konstruktor.
     * @param button Kod zwolnionego przycisku myszy.
     */
    MouseButtonReleasedEvent(int button) : MouseButtonEvent(button) {}

    std::string toString() const override { return "MouseButtonReleasedEvent: " + std::to_string(m_Button); }

    EVENT_CLASS_TYPE(MouseButtonReleased)
};

// --- Zdarzenia Aplikacji ---
// Implementacje tych klas sa skrocone, poniewaz nie przechowuja dodatkowych danych.

/** @class AppTickEvent @brief Zdarzenie cyklicznego "tykniecia" aplikacji. */
class AppTickEvent : public Event { public: AppTickEvent() = default; EVENT_CLASS_TYPE(AppTick) EVENT_CLASS_CATEGORY(EventCategoryApplication) };
/** @class AppUpdateEvent @brief Zdarzenie aktualizacji logiki aplikacji. */
class AppUpdateEvent : public Event { public: AppUpdateEvent() = default; EVENT_CLASS_TYPE(AppUpdate) EVENT_CLASS_CATEGORY(EventCategoryApplication) };
/** @class AppRenderEvent @brief Zdarzenie renderowania klatki przez aplikacje. */
class AppRenderEvent : public Event { public: AppRenderEvent() = default; EVENT_CLASS_TYPE(AppRender) EVENT_CLASS_CATEGORY(EventCategoryApplication) };


// --- Zdarzenie Kolizji ---

/**
 * @class CollisionEvent
 * @brief Zdarzenie generowane, gdy system detekcji kolizji wykryje zderzenie miedzy dwoma obiektami.
 * Przechowuje wskazniki do dwoch obiektow (ICollidable), ktore weszly w kolizje.
 */
class CollisionEvent : public Event {
public:
    /**
     * @brief Konstruktor.
     * @param a Wskaznik do pierwszego obiektu kolidujacego.
     * @param b Wskaznik do drugiego obiektu kolidujacego.
     */
    CollisionEvent(ICollidable* a, ICollidable* b)
        : m_CollidableA(a), m_CollidableB(b) {
    }

    /** @brief Zwraca wskaznik do pierwszego obiektu bioracego udzial w kolizji. */
    ICollidable* getCollidableA() const { return m_CollidableA; }
    /** @brief Zwraca wskaznik do drugiego obiektu bioracego udzial w kolizji. */
    ICollidable* getCollidableB() const { return m_CollidableB; }

    std::string toString() const override {
        // Mozna dodac bardziej szczegolowe informacje, jesli ICollidable
        // udostepnia metody takie jak getName() lub getID().
        // Na przyklad:
        // return "CollisionEvent: [ObjectA_ID] vs [ObjectB_ID]";
        return "CollisionEvent"; // Prosta reprezentacja
    }

    EVENT_CLASS_TYPE(Collision)
        EVENT_CLASS_CATEGORY(EventCategoryGameLogic) // Kolizje sa czescia logiki gry

private:
    ICollidable* m_CollidableA; ///< Wskaznik do pierwszego obiektu kolizji.
    ICollidable* m_CollidableB; ///< Wskaznik do drugiego obiektu kolizji.
    // W przyszlosci mozna dodac wiecej informacji o kolizji:
    // glm::vec3 m_collisionPoint;  ///< Punkt kolizji w przestrzeni swiata.
    // glm::vec3 m_collisionNormal; ///< Normalna kolizji (np. wskazujaca od A do B).
    // float m_penetrationDepth;    ///< Glebokosc penetracji.
};


#endif // EVENT_H
