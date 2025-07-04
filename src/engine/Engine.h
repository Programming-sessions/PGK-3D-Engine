/**
* @file Engine.h
* @brief Główny silnik gry wykorzystujący bibliotekę GLFW.
*
* Klasa Engine implementuje wzorzec Singleton i zapewnia podstawową
* funkcjonalność silnika gry, w tym zarządzanie oknem oraz główną pętlę gry.
*/
#ifndef ENGINE_H
#define ENGINE_H

#include <string>
#include <memory>   // Dla std::unique_ptr
#include <vector>
#include <glm/glm.hpp> // Dla typow wektorowych i macierzowych

#include <GLFW/glfw3.h> // Dla GLFWwindow i funkcji GLFW

// --- Pełne definicje klas potrzebnych w interfejsie Engine lub jako klasy bazowe ---
// Dołączenie pełnych definicji dla typów używanych jako składowe unique_ptr,
// klas bazowych, lub typów zagnieżdżonych w publicznym API.
#include "IEventListener.h"     // Potrzebne, bo Engine dziedziczy po IEventListener
#include "Camera.h"             // Potrzebne dla Camera::ProjectionType i std::unique_ptr<Camera> m_camera
#include "EventManager.h"       // Dla std::unique_ptr<EventManager> m_eventManager
#include "Renderer.h"           // Dla std::unique_ptr<Renderer> m_renderer
#include "InputManager.h"       // Dla std::unique_ptr<InputManager> m_inputManager
#include "LightingManager.h"    // Dla std::unique_ptr<LightingManager> m_lightingManager
#include "ShadowSystem.h"       // Dla std::unique_ptr<ShadowSystem> m_shadowSystem
#include "CollisionSystem.h"    // Dla std::unique_ptr<CollisionSystem> m_collisionSystem
#include "GameStateManager.h"   // Dla std::unique_ptr<GameStateManager> m_gameStateManager

// --- Deklaracje wyprzedzajace dla pozostałych typów używanych głównie jako wskaźniki/referencje
// --- w parametrach metod lub typach zwracanych, gdzie pełna definicja w Engine.h nie jest krytyczna.
class Logger; // Zakladamy, ze Logger.h jest dolaczony w Engine.cpp
class IRenderable;
class ICollidable;
class ResourceManager; // Zakladamy, ze jest singletonem i jego .h jest dolaczony w .cpp
struct DirectionalLight; // Struktury z Lighting.h (Lighting.h bedzie potrzebny w main.cpp/stanach gry)
struct PointLight;
struct SpotLight;
class Event; // Klasa bazowa zdarzenia
class TextRenderer;

// Stale globalne zwiazane z mapami cieni, uzywane przez Engine i ShadowSystem.
// Umieszczenie ich tutaj moze byc dyskusyjne; alternatywnie moglyby byc
// w ShadowSystem.h lub dedykowanym pliku konfiguracyjnym.

/** @brief Szerokosc tekstury mapy cieni dla swiatel kierunkowych i reflektorow. */
const unsigned int SHADOW_MAP_WIDTH_ENGINE = 4096;
/** @brief Wysokosc tekstury mapy cieni dla swiatel kierunkowych i reflektorow. */
const unsigned int SHADOW_MAP_HEIGHT_ENGINE = 4096;
/** @brief Szerokosc tekstury mapy cieni (cube map) dla swiatel punktowych. */
const unsigned int SHADOW_CUBE_MAP_WIDTH_ENGINE = 1024;
/** @brief Wysokosc tekstury mapy cieni (cube map) dla swiatel punktowych. */
const unsigned int SHADOW_CUBE_MAP_HEIGHT_ENGINE = 1024;


/**
 * @class Engine
 * @brief Glowna klasa silnika gry, zarzadzajaca wszystkimi podsystemami.
 *
 * Engine jest odpowiedzialny za inicjalizacje, glowna petle gry,
 * aktualizacje logiki, renderowanie, zarzadzanie zasobami, obsluge wejscia,
 * system zdarzen, stany gry oraz finalne zwolnienie zasobow.
 * Implementuje wzorzec Singleton (poprzez metode getInstance) oraz
 * interfejs IEventListener do obslugi niektorych zdarzen systemowych (np. zamkniecia okna).
 */
class Engine : public IEventListener {
private:
    /** @brief Statyczny wskaznik do jedynej instancji silnika (Singleton). */
    static Engine* instance;

    /**
     * @brief Prywatny konstruktor domyslny (Singleton).
     * Inicjalizuje skladowe domyslnymi wartosciami.
     */
    Engine();

    // --- Skladowe zwiazane z oknem i kontekstem graficznym ---
    GLFWwindow* m_window;       ///< Wskaznik do okna GLFW.
    int m_width;                ///< Aktualna szerokosc okna w pikselach.
    int m_height;               ///< Aktualna wysokosc okna w pikselach.
    std::string m_title;        ///< Tytul okna.
    bool m_fullscreen;          ///< Flaga okreslajaca, czy okno jest w trybie pelnoekranowym.

    // --- Glowne systemy i menedzery silnika ---
    // Uzycie std::unique_ptr zapewnia automatyczne zarzadzanie pamiecia.
    std::unique_ptr<EventManager> m_eventManager;       ///< Menedzer zdarzen.
    std::unique_ptr<Renderer> m_renderer;               ///< System renderujacy.
    std::unique_ptr<Camera> m_camera;                   ///< Glowna kamera sceny (moze byc zarzadzana przez stan gry).
    std::unique_ptr<InputManager> m_inputManager;       ///< Menedzer wejscia (klawiatura, mysz).
    TextRenderer* m_textRenderer;                       ///< Wskaznik do renderera tekstu (singleton lub zarzadzany zewnetrznie).
    // TODO: Rozwazyc zmiane na std::unique_ptr, jesli Engine ma byc wlascicielem.

    std::unique_ptr<LightingManager> m_lightingManager; ///< Menedzer oswietlenia.
    std::unique_ptr<ShadowSystem> m_shadowSystem;       ///< System generowania cieni.
    std::unique_ptr<CollisionSystem> m_collisionSystem; ///< System detekcji kolizji.
    std::unique_ptr<GameStateManager> m_gameStateManager; ///< Menedzer stanow gry.

    // --- Skladowe stanu silnika i petli gry ---
    bool m_initialized;        ///< Flaga okreslajaca, czy silnik zostal poprawnie zainicjalizowany.
    double m_lastFrameTime;    ///< Czas (wg GLFW) zakonczenia poprzedniej klatki.
    float m_deltaTime;         ///< Czas (w sekundach), ktory uplynal od poprzedniej klatki.

    float m_targetFPS;         ///< Docelowa liczba klatek na sekunde (obecnie nieuzywane do limitowania).
    float m_backgroundColor[4];///< Kolor tla (RGBA).
    bool m_vsyncEnabled;       ///< Flaga okreslajaca, czy synchronizacja pionowa (VSync) jest włączona.
    bool m_autoClear;          ///< Flaga okreslajaca, czy bufory maja byc automatycznie czyszczone przed renderowaniem.
    bool m_autoSwap;           ///< Flaga okreslajaca, czy bufory maja byc automatycznie zamieniane po renderowaniu.

    int m_pcfRadius;           ///< Promien dla Percentage Closer Filtering (PCF) przy renderowaniu cieni.

    // --- Skladowe do obliczania i wyswietlania FPS ---
    bool m_showFPS;            ///< Flaga okreslajaca, czy wyswietlac licznik FPS.
    double m_currentFPS;       ///< Aktualna obliczona liczba klatek na sekunde.
    int m_frameCount;          ///< Licznik klatek w biezacej sekundzie (do obliczenia FPS).
    double m_lastFPSTime;      ///< Czas ostatniego pomiaru FPS.

    // --- Prywatne metody pomocnicze inicjalizacji ---
    /** @brief Inicjalizuje biblioteke GLFW. */
    bool initializeGLFW();
    /** @brief Tworzy i konfiguruje okno GLFW. */
    bool initializeWindow();
    /** @brief Inicjalizuje GLAD i podstawowe ustawienia OpenGL. */
    bool initializeOpenGL();
    /** @brief Inicjalizuje wszystkie glowne menedzery i systemy silnika. */
    bool initializeManagersAndSystems();
    /** @brief Inicjalizuje renderer tekstu. */
    bool initializeTextRenderer();
    /** @brief Inicjalizuje menedzera wejscia. */
    bool initializeInputManager();
    /** @brief Ustawia domyslne parametry oswietlenia. */
    void initializeLightingDefaults();
    /** @brief Inicjalizuje menedzera stanow gry. */
    bool initializeGameStateManager();

    // --- Statyczne funkcje zwrotne (callbacks) dla GLFW ---
    // Sa one statyczne, poniewaz GLFW tego wymaga. Uzywaja glfwGetWindowUserPointer
    // do uzyskania dostepu do instancji silnika.
    /** @brief Callback dla bledow GLFW. */
    static void glfwErrorCallback(int error, const char* description);
    /** @brief Callback dla zmiany rozmiaru bufora ramki (framebuffer). */
    static void framebufferSizeCallback(GLFWwindow* window, int newWidth, int newHeight);
    /** @brief Callback dla zadania zamkniecia okna. */
    static void windowCloseCallback(GLFWwindow* window);

    /** @brief Uruchamia i zarzadza wyswietlaniem ekranu powitalnego (splash screen). */
    void runSplashScreen();
    /** @brief Oblicza aktualna liczbe klatek na sekunde. */
    void calculateFPS();

public:
    /**
     * @brief Zwraca jedyna instancje silnika (Singleton).
     * Jesli instancja nie istnieje, tworzy ja.
     * @return Wskaznik do instancji Engine.
     */
    static Engine* getInstance();

    /**
     * @brief Destruktor. Wywoluje metode shutdown() w celu zwolnienia zasobow.
     */
    ~Engine();

    // Zapobieganie kopiowaniu i przypisywaniu (Singleton).
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    /**
     * @brief Glowna metoda inicjalizujaca silnik.
     * Tworzy okno, inicjalizuje OpenGL, menedzery i systemy.
     * @param width Szerokosc okna. Domyslnie 800.
     * @param height Wysokosc okna. Domyslnie 600.
     * @param title Tytul okna. Domyslnie "3D Engine".
     * @return True jesli inicjalizacja przebiegla pomyslnie, false w przeciwnym wypadku.
     */
    bool initialize(int width = 800, int height = 600, const char* title = "3D Engine");

    /**
     * @brief Aktualizuje logike silnika i aktywnego stanu gry.
     * Wywolywana w kazdej klatce glownej petli gry.
     * Obsluguje zdarzenia, aktualizuje systemy (np. kolizji) i aktywny stan gry.
     */
    void update();

    /**
     * @brief Renderuje scene dla aktywnego stanu gry.
     * Wywolywana w kazdej klatce glownej petli gry po aktualizacji logiki.
     * Generuje mapy cieni, czysci bufory (jesli autoClear jest wlaczone),
     * ustawia uniformy dla oswietlenia i cieni, renderuje aktywny stan gry
     * oraz opcjonalnie wyswietla licznik FPS.
     */
    void render();

    /**
     * @brief Sprawdza, czy silnik powinien zakonczyc dzialanie.
     * Zwraca true, jesli okno GLFW otrzymalo zadanie zamkniecia
     * LUB jesli stos stanow gry jest pusty (po inicjalizacji).
     * @return True jesli silnik powinien sie zamknac, false w przeciwnym wypadku.
     */
    bool shouldClose() const;

    /**
     * @brief Zwalnia wszystkie zasoby i zamyka systemy silnika.
     * Wywolywana automatycznie w destruktorze lub moze byc wywolana recznie.
     */
    void shutdown();

    /**
     * @brief Metoda obslugi zdarzen, implementacja interfejsu IEventListener.
     * Silnik nasluchuje na zdarzenie WindowClose, aby poprawnie zakonczyc dzialanie.
     * @param event Referencja do obiektu zdarzenia.
     */
    void onEvent(Event& event) override;

    // --- Metody dostepowe i modyfikujace ustawienia silnika ---

    /** @brief Ustawia tryb pelnoekranowy dla okna. */
    void setFullscreen(bool fullscreen);
    /** @brief Ustawia docelowa liczbe klatek na sekunde (obecnie nieuzywane do limitowania). */
    void setTargetFPS(float fps);
    /** @brief Ustawia kolor tla (RGBA). */
    void setBackgroundColor(float r, float g, float b, float a = 1.0f);
    /** @brief Wlacza lub wylacza synchronizacje pionowa (VSync). */
    void setVSync(bool enabled);
    /** @brief Ustawia jakosc filtrowania cieni PCF poprzez promien probkowania. */
    void setPCFQuality(int radius);
    /** @brief Zwraca aktualnie ustawiony promien dla PCF. */
    int getPCFQuality() const;
    /** @brief Przelacza wyswietlanie licznika FPS. */
    void toggleFPSDisplay();

    // --- Gettery dla systemow i menedzerow ---
    // Zwracaja surowe wskazniki do obiektow zarzadzanych przez unique_ptr.
    // Uzytkownik nie powinien usuwac tych wskaznikow.

    /** @brief Zwraca wskaznik do menedzera zdarzen. */
    EventManager* getEventManager() const { return m_eventManager.get(); }
    /** @brief Zwraca wskaznik do menedzera wejscia. */
    InputManager* getInputManager() const { return m_inputManager.get(); }
    /** @brief Zwraca referencje do menedzera zasobow (Singleton). */
    ResourceManager& getResourceManager() const; // Implementacja w .cpp, aby uniknac #include ResourceManager.h tutaj
    /** @brief Zwraca wskaznik do menedzera oswietlenia. */
    LightingManager* getLightingManager() const { return m_lightingManager.get(); }
    /** @brief Zwraca wskaznik do systemu cieni. */
    ShadowSystem* getShadowSystem() const { return m_shadowSystem.get(); }
    /** @brief Zwraca wskaznik do systemu kolizji. */
    CollisionSystem* getCollisionSystem() const { return m_collisionSystem.get(); }
    /** @brief Zwraca wskaznik do menedzera stanow gry. */
    GameStateManager* getGameStateManager() const { return m_gameStateManager.get(); }
    /** @brief Zwraca wskaznik do renderera. */
    Renderer* getRenderer() const { return m_renderer.get(); }
    /** @brief Zwraca wskaznik do glownej kamery. Uwaga: kamera moze byc zarzadzana przez stan gry. */
    Camera* getCamera() const { return m_camera.get(); }

    // --- Metody zwiazane z oswietleniem (delegujace do LightingManager) ---
    /** @brief Wlacza lub wylacza generowanie cieni dla danego reflektora. */
    bool enableSpotLightShadow(int globalLightIndex, bool enable);
    /** @brief Wlacza lub wylacza generowanie cieni dla danego swiatla punktowego. */
    bool enablePointLightShadow(int globalLightIndex, bool enable);

    /** @brief Ustawia parametry globalnego swiatla kierunkowego. */
    void setDirectionalLight(const DirectionalLight& light);
    /** @brief Zwraca stala referencje do globalnego swiatla kierunkowego. */
    const DirectionalLight& getDirectionalLight() const;
    /** @brief Zwraca modyfikowalna referencje do globalnego swiatla kierunkowego. */
    DirectionalLight& getMutableDirectionalLight();

    /** @brief Dodaje swiatlo punktowe do sceny. */
    void addPointLight(const PointLight& light);
    /** @brief Zwraca referencje do wektora modyfikowalnych swiatel punktowych. */
    std::vector<PointLight>& getPointLights();
    /** @brief Zwraca stala referencje do wektora swiatel punktowych. */
    const std::vector<PointLight>& getPointLights() const;
    /** @brief Usuwa wszystkie swiatla punktowe ze sceny i ich konfiguracje cieni. */
    void clearPointLights();

    /** @brief Dodaje reflektor do sceny. */
    void addSpotLight(const SpotLight& light);
    /** @brief Zwraca referencje do wektora modyfikowalnych reflektorow. */
    std::vector<SpotLight>& getSpotLights();
    /** @brief Zwraca stala referencje do wektora reflektorow. */
    const std::vector<SpotLight>& getSpotLights() const;
    /** @brief Usuwa wszystkie reflektory ze sceny i ich konfiguracje cieni. */
    void clearSpotLights();

    // --- Gettery dla podstawowych informacji o silniku ---
    /** @brief Zwraca wskaznik do okna GLFW. */
    GLFWwindow* getWindow() const { return m_window; }
    /** @brief Zwraca czas trwania ostatniej klatki (delta time). */
    float getDeltaTime() const { return m_deltaTime; }
    /** @brief Zwraca aktualna szerokosc okna. */
    int getWindowWidth() const { return m_width; }
    /** @brief Zwraca aktualna wysokosc okna. */
    int getWindowHeight() const { return m_height; }
    /** @brief Sprawdza, czy silnik zostal zainicjalizowany. */
    bool isInitialized() const { return m_initialized; }

    // --- Metody zwiazane z kamera (delegujace do obiektu Camera) ---
    /** @brief Ustawia pozycje kamery. */
    void setCameraPosition(const glm::vec3& position);
    /** @brief Ustawia typ projekcji kamery (perspektywiczna/ortogonalna). */
    void setCameraProjectionType(Camera::ProjectionType type);
    /** @brief Ustawia pole widzenia (FOV) kamery (dla projekcji perspektywicznej). */
    void setCameraFOV(float fov);
    /** @brief Ustawia plaszczyzny przycinania (bliska i daleka) kamery. */
    void setCameraClippingPlanes(float nearClip, float farClip);

    // --- Metody do zarzadzania obiektami renderowalnymi i kolidujacymi ---
    // UWAGA: Te metody moga byc uznane za przestarzale lub niezalecane.
    // Zarzadzanie obiektami sceny powinno odbywac sie glownie w ramach stanow gry.
    /**
     * @brief Dodaje obiekt renderowalny do globalnej listy renderera i systemu kolizji.
     */
    void addRenderable(IRenderable* renderable);
    /**
    * @brief Usuwa obiekt renderowalny z globalnej listy renderera i systemu kolizji.
    */
    void removeRenderable(IRenderable* renderable);

    // --- Inne metody konfiguracyjne ---
    /** @brief Ustawia tryb przechwytywania kursora myszy. */
    void setMouseCapture(bool enabled);
    /** @brief Wlacza/wylacza automatyczne czyszczenie buforow przed renderowaniem. */
    void setAutoClear(bool enabled);
    /** @brief Wlacza/wylacza automatyczna zamiane buforow po renderowaniu. */
    void setAutoSwap(bool enabled);
    /** @brief Calkowicie czysci bufory koloru i glebokosci. */
    void clearBuffers();
};

#endif // ENGINE_H
