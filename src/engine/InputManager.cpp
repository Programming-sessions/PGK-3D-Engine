#include "InputManager.h"
#include "Logger.h"
// Pliki Event.h i EventManager.h sa juz dolaczone posrednio przez InputManager.h

InputManager::InputManager(GLFWwindow* window, EventManager* eventManager)
    : m_window(window),
    m_eventManager(eventManager),
    m_currentMousePosition(0.0f, 0.0f),
    m_previousMousePosition_forDelta(0.0f, 0.0f),
    m_mouseDelta_forPolling(0.0f, 0.0f),
    m_firstMouseUpdate(true),
    m_isMouseCaptured(false) {

    // Sprawdzenie poprawnosci wskaznikow
    if (!m_window) {
        Logger::getInstance().fatal("InputManager: Otrzymano pusty wskaznik (nullptr) do okna GLFW!");
        // W krytycznej sytuacji nalezaloby rzucic wyjatek lub zakonczyc program
        // np. throw std::runtime_error("InputManager: GLFW window is null!");
        return;
    }
    if (!m_eventManager) {
        Logger::getInstance().fatal("InputManager: Otrzymano pusty wskaznik (nullptr) do EventManagera!");
        // np. throw std::runtime_error("InputManager: EventManager is null!");
        return;
    }

    // Inicjalizacja tablic stanow klawiszy i przyciskow myszy
    for (int i = 0; i < MAX_KEYS; ++i) {
        m_currentKeyStates[i] = false;
        m_previousKeyStates[i] = false;
        m_keyRepeatCounts[i] = 0;
    }
    for (int i = 0; i < MAX_MOUSE_BUTTONS; ++i) {
        m_currentMouseButtonStates[i] = false;
        m_previousMouseButtonStates[i] = false;
    }

    // Ustawienie wskaznika uzytkownika okna GLFW na biezaca instancje InputManagera.
    // Pozwala to statycznym funkcjom callback odnalezc wlasciwa instancje klasy.
    glfwSetWindowUserPointer(m_window, this);

    // Rejestracja statycznych funkcji zwrotnych (callbackow) GLFW
    glfwSetKeyCallback(m_window, InputManager::keyCallback);
    glfwSetCharCallback(m_window, InputManager::charCallback);
    glfwSetMouseButtonCallback(m_window, InputManager::mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, InputManager::cursorPosCallback);
    glfwSetScrollCallback(m_window, InputManager::scrollCallback);
    // Inne callbacki, takie jak focus, close, resize, moga byc obslugiwane
    // przez inne moduly (np. Engine) lub dodane tutaj w razie potrzeby.
    // glfwSetWindowFocusCallback(m_window, InputManager::windowFocusCallback);
    // glfwSetWindowCloseCallback(m_window, InputManager::windowCloseCallback);

    Logger::getInstance().info("InputManager zainicjalizowany. Callbacki GLFW ustawione.");
}

InputManager::~InputManager() {
    // GLFW generalnie automatycznie odlacza callbacki podczas niszczenia okna.
    // Jawne ustawienie callbackow na nullptr nie jest zwykle konieczne,
    // ale moze byc wykonane dla pewnosci, jesli okno zyje dluzej niz InputManager
    // lub jesli chcemy miec pewnosc co do stanu.
    // Przyklad:
    // if (m_window) {
    //     glfwSetKeyCallback(m_window, nullptr);
    //     // ... itd. dla pozostalych callbackow
    // }
    Logger::getInstance().info("InputManager zniszczony.");
}

void InputManager::update() {
    if (!m_window) return; // Zabezpieczenie na wypadek niepoprawnego stanu

    // Kopiowanie aktualnych stanow klawiszy do poprzednich stanow.
    // Uzywane do wykrywania zdarzen 'typed' (wcisniecie w tej klatce) i 'released' (zwolnienie w tej klatce).
    for (int i = 0; i < MAX_KEYS; ++i) {
        m_previousKeyStates[i] = m_currentKeyStates[i];
    }
    // Kopiowanie aktualnych stanow przyciskow myszy do poprzednich stanow.
    for (int i = 0; i < MAX_MOUSE_BUTTONS; ++i) {
        m_previousMouseButtonStates[i] = m_currentMouseButtonStates[i];
    }

    // Aktualizacja pozycji myszy i obliczenie delty dla metod pollingowych.
    // m_currentMousePosition jest na biezaco aktualizowane przez cursorPosCallback.
    if (m_firstMouseUpdate && !m_isMouseCaptured) {
        // Dla myszy nieprzechwyconej, pierwsza aktualizacja (po uruchomieniu lub zwolnieniu przechwytywania)
        // ustawia poprzednia pozycje na aktualna, aby uniknac duzej, blednej delty.
        m_previousMousePosition_forDelta = m_currentMousePosition;
        m_firstMouseUpdate = false; // Nastepne aktualizacje beda juz obliczac delte.
    }

    // Obliczenie delty myszy dla metody getMouseDelta() (polling).
    m_mouseDelta_forPolling = m_currentMousePosition - m_previousMousePosition_forDelta;
    // Zapisanie aktualnej pozycji jako poprzedniej dla nastepnej klatki.
    m_previousMousePosition_forDelta = m_currentMousePosition;

    // Stany klawiszy (m_currentKeyStates) i przyciskow myszy (m_currentMouseButtonStates)
    // sa aktualizowane bezposrednio w odpowiednich callbackach (onKey, onMouseButton).
    // Metoda update() sluzy glownie do przygotowania danych dla metod odpytujacych (polling).
}

// Metoda resetFrameStates zostala usunieta z .h, poniewaz jej funkcjonalnosc
// zostala zintegrowana z metoda update() lub jest obslugiwana przez system zdarzen.

// --- Klawiatura (metody pollingowe) ---
bool InputManager::isKeyPressed(int keyCode) const {
    if (keyCode < 0 || keyCode >= MAX_KEYS) return false; // Sprawdzenie zakresu
    return m_currentKeyStates[keyCode];
}

bool InputManager::isKeyTyped(int keyCode) const {
    if (keyCode < 0 || keyCode >= MAX_KEYS) return false;
    // Klawisz jest 'typed' jesli jest teraz wcisniety, a w poprzedniej klatce nie byl.
    return m_currentKeyStates[keyCode] && !m_previousKeyStates[keyCode];
}

bool InputManager::isKeyReleased(int keyCode) const {
    if (keyCode < 0 || keyCode >= MAX_KEYS) return false;
    // Klawisz jest 'released' jesli nie jest teraz wcisniety, a w poprzedniej klatce byl.
    return !m_currentKeyStates[keyCode] && m_previousKeyStates[keyCode];
}

// --- Mysz (metody pollingowe) ---
bool InputManager::isMouseButtonPressed(int buttonCode) const {
    if (buttonCode < 0 || buttonCode >= MAX_MOUSE_BUTTONS) return false;
    return m_currentMouseButtonStates[buttonCode];
}

bool InputManager::isMouseButtonClicked(int buttonCode) const {
    if (buttonCode < 0 || buttonCode >= MAX_MOUSE_BUTTONS) return false;
    // Przycisk jest 'clicked' jesli jest teraz wcisniety, a w poprzedniej klatce nie byl.
    return m_currentMouseButtonStates[buttonCode] && !m_previousMouseButtonStates[buttonCode];
}

bool InputManager::isMouseButtonReleased(int buttonCode) const {
    if (buttonCode < 0 || buttonCode >= MAX_MOUSE_BUTTONS) return false;
    // Przycisk jest 'released' jesli nie jest teraz wcisniety, a w poprzedniej klatce byl.
    return !m_currentMouseButtonStates[buttonCode] && m_previousMouseButtonStates[buttonCode];
}

glm::vec2 InputManager::getMousePosition() const {
    // Zwraca pozycje myszy aktualizowana przez cursorPosCallback.
    return m_currentMousePosition;
}

glm::vec2 InputManager::getMouseDelta() const {
    // Zwraca delte myszy obliczona w metodzie update().
    // Dla precyzyjniejszej obslugi ruchu myszy (np. w kamerze),
    // zaleca sie korzystanie ze zdarzenia MouseMovedEvent, ktore dostarcza
    // bezposrednie dane o ruchu, a nie delte obliczona per klatka.
    return m_mouseDelta_forPolling;
}

void InputManager::setMouseCapture(bool enabled) {
    if (!m_window) return; // Zabezpieczenie
    m_isMouseCaptured = enabled;

    if (enabled) {
        // Wlaczenie trybu przechwyconej myszy: kursor ukryty i zablokowany w oknie.
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        // Jesli wspierane, uzyj surowego ruchu myszy dla wiekszej precyzji (omija akceleracje systemowa).
        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
        // Resetowanie pozycji i flagi 'firstMouseUpdate' przy przechwyceniu.
        // Zapobiega to duzemu skokowi delty myszy przy pierwszym ruchu po przechwyceniu.
        double x, y;
        glfwGetCursorPos(m_window, &x, &y); // Pobierz aktualna pozycje, aby zsynchronizowac
        m_currentMousePosition = glm::vec2(static_cast<float>(x), static_cast<float>(y));
        m_previousMousePosition_forDelta = m_currentMousePosition; // Synchronizacja dla pollingu
        m_firstMouseUpdate = true; // Nastepny ruch w cursorPosCallback ustawi poprawna baze
    }
    else {
        // Wylaczenie trybu przechwyconej myszy: kursor normalny.
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        }
        // Po zwolnieniu przechwytywania, nastepna klatka w update()
        // ustawi m_previousMousePosition_forDelta na m_currentMousePosition.
        m_firstMouseUpdate = true; // Aby update() poprawnie zresetowal delte dla pollingu
    }
}

bool InputManager::isMouseCaptured() const {
    return m_isMouseCaptured;
}

// --- Statyczne Callbacki GLFW ---
// Te funkcje sa wywolywane przez GLFW. Pobieraja wskaznik do instancji InputManagera
// zapisany w oknie (przez glfwSetWindowUserPointer) i przekazuja zdarzenie
// do odpowiedniej metody tej instancji.

void InputManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    InputManager* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (manager) {
        manager->onKey(key, scancode, action, mods);
    }
}

void InputManager::charCallback(GLFWwindow* window, unsigned int codepoint) {
    InputManager* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (manager) {
        manager->onChar(codepoint);
    }
}

void InputManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    InputManager* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (manager) {
        manager->onMouseButton(button, action, mods);
    }
}

void InputManager::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    InputManager* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (manager) {
        manager->onMouseMove(xpos, ypos);
    }
}

void InputManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    InputManager* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
    if (manager) {
        manager->onMouseScroll(xoffset, yoffset);
    }
}

// --- Metody instancji obslugujace callbacki ---
// Te metody zawieraja logike przetwarzania zdarzen wejsciowych i rozglaszania ich.

void InputManager::onKey(int key, int scancode, int action, int mods) {
    if (key < 0 || key >= MAX_KEYS) { // Ignoruj klawisze spoza zakresu (np. specjalne, nieobslugiwane)
        // Logger::getInstance().warning("InputManager::onKey - Nieznany kod klawisza: " + std::to_string(key));
        return;
    }

    if (action == GLFW_PRESS) {
        m_currentKeyStates[key] = true; // Aktualizacja stanu dla pollingu
        m_keyRepeatCounts[key]++;       // Zwiekszenie licznika powtorzen
        KeyPressedEvent event(key, m_keyRepeatCounts[key]); // Stworzenie zdarzenia
        m_eventManager->dispatch(event);                    // Rozgloszenie zdarzenia
    }
    else if (action == GLFW_RELEASE) {
        m_currentKeyStates[key] = false; // Aktualizacja stanu dla pollingu
        m_keyRepeatCounts[key] = 0;      // Reset licznika powtorzen przy zwolnieniu
        KeyReleasedEvent event(key);     // Stworzenie zdarzenia
        m_eventManager->dispatch(event); // Rozgloszenie zdarzenia
    }
    else if (action == GLFW_REPEAT) {
        // GLFW_REPEAT jest obslugiwane przez ponowne wyslanie KeyPressedEvent
        // z odpowiednio zwiekszonym licznikiem powtorzen.
        m_keyRepeatCounts[key]++;
        KeyPressedEvent event(key, m_keyRepeatCounts[key]);
        m_eventManager->dispatch(event);
    }
}

void InputManager::onChar(unsigned int codepoint) {
    // GLFW dostarcza znak jako kod Unicode (unsigned int).
    // KeyTypedEvent w Event.h przechowuje 'int keyCode', wiec rzutujemy.
    // Dla pelnej obslugi Unicode, KeyTypedEvent mogloby przechowywac unsigned int
    // lub std::string (dla znakow wielobajtowych, choc GLFW_CHAR_CALLBACK daje pojedyncze codepointy).
    KeyTypedEvent event(static_cast<int>(codepoint));
    m_eventManager->dispatch(event);
}

void InputManager::onMouseButton(int button, int action, int mods) {
    if (button < 0 || button >= MAX_MOUSE_BUTTONS) { // Ignoruj przyciski spoza zakresu
        // Logger::getInstance().warning("InputManager::onMouseButton - Nieznany kod przycisku myszy: " + std::to_string(button));
        return;
    }

    // Pozycja myszy jest juz aktualizowana przez cursorPosCallback.
    // Jesli byloby to konieczne, mozna by ja tutaj pobrac:
    // double xpos, ypos;
    // glfwGetCursorPos(m_window, &xpos, &ypos);
    // glm::vec2 clickPos(static_cast<float>(xpos), static_cast<float>(ypos));

    if (action == GLFW_PRESS) {
        m_currentMouseButtonStates[button] = true; // Aktualizacja stanu dla pollingu
        // Mozna dodac pozycje klikniecia do zdarzenia, jesli jest potrzebna
        MouseButtonPressedEvent event(button /*, clickPos.x, clickPos.y */);
        m_eventManager->dispatch(event);
    }
    else if (action == GLFW_RELEASE) {
        m_currentMouseButtonStates[button] = false; // Aktualizacja stanu dla pollingu
        MouseButtonReleasedEvent event(button /*, clickPos.x, clickPos.y */);
        m_eventManager->dispatch(event);
    }
}

void InputManager::onMouseMove(double xpos, double ypos) {
    glm::vec2 newPos(static_cast<float>(xpos), static_cast<float>(ypos));

    if (m_firstMouseUpdate && m_isMouseCaptured) {
        // Pierwsze zdarzenie ruchu myszy po jej przechwyceniu.
        // Ustawiamy aktualna pozycje jako bazowa i nie wysylamy zdarzenia,
        // aby uniknac duzej, niepoprawnej delty wynikajacej z ewentualnego skoku kursora.
        m_currentMousePosition = newPos;
        m_previousMousePosition_forDelta = newPos; // Synchronizacja dla delty pollingowej
        m_firstMouseUpdate = false;                // Nastepne ruchy beda juz normalnie przetwarzane
        return;
    }

    // Zaktualizuj biezaca pozycje myszy (uzywana przez getMousePosition() i update()).
    m_currentMousePosition = newPos;

    // MouseMovedEvent powinien przekazywac aktualna pozycje myszy.
    // Obliczenie delty (newPos - m_previousMousePosition_forDelta) moze byc wykonane
    // przez sluchacza zdarzenia (np. klase kamery), jesli potrzebuje on delty,
    // lub mozna ja dodac do samego zdarzenia.
    // W tym przypadku, przekazujemy tylko nowa pozycje.
    MouseMovedEvent event(newPos.x, newPos.y);
    m_eventManager->dispatch(event);

    // Upewnij sie, ze flaga m_firstMouseUpdate jest wylaczona po pierwszym rzeczywistym ruchu
    // w trybie przechwyconym (po wykonaniu powyzszego 'return').
    if (m_isMouseCaptured) {
        m_firstMouseUpdate = false;
    }
}

void InputManager::onMouseScroll(double xoffset, double yoffset) {
    // Przekazanie wartosci przewiniecia (offsetow) do zdarzenia.
    MouseScrolledEvent event(static_cast<float>(xoffset), static_cast<float>(yoffset));
    m_eventManager->dispatch(event);
}
