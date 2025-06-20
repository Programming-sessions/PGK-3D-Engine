#include <glad/glad.h> // Dla funkcji OpenGL
#include "Engine.h"

#include <algorithm> // Dla std::max, std::min, std::remove
#include <vector>    // Uzywane wewnetrznie
#include <cmath>     // Dla funkcji matematycznych (np. w kamerze, fizyce)
#include <iomanip>   // Dla std::fixed, std::setprecision (formatowanie FPS)
#include <sstream>   // Dla std::stringstream (budowanie stringa FPS)
#include <string>    // Dla std::string, std::to_string

#include "Logger.h"
#include "TextRenderer.h"
#include "ResourceManager.h"
#include "Shader.h"
#include "Lighting.h"         // Definicje struktur swiatel
#include "LightingManager.h"
#include "ShadowSystem.h"
#include "CollisionSystem.h"
#include "Event.h"            // Definicje zdarzen
#include "EventManager.h"     // Potrzebny do utworzenia m_eventManager
#include "InputManager.h"     // Potrzebny do utworzenia m_inputManager
#include "GameStateManager.h"
#include "IGameState.h"
#include "SplashScreenState.h" // Przykladowy stan gry dla ekranu powitalnego
#include "Camera.h"            // Potrzebny do utworzenia m_camera
#include "Renderer.h"          // Potrzebny do utworzenia m_renderer

// Inicjalizacja statycznej skladowej dla wzorca Singleton
Engine* Engine::instance = nullptr;

// --- Statyczne funkcje zwrotne (callbacks) dla GLFW ---

void Engine::glfwErrorCallback(int error, const char* description) {
    // Statyczna metoda obslugujaca bledy zglaszane przez GLFW.
    // Loguje informacje o bledzie za pomoca Loggera.
    Logger::getInstance().error("GLFW Error " + std::to_string(error) + ": " + description);
}

void Engine::framebufferSizeCallback(GLFWwindow* window, int newWidth, int newHeight) {
    // Statyczna metoda obslugujaca zmiane rozmiaru bufora ramki (framebuffer).
    // Wywolywana przez GLFW, gdy rozmiar okna ulegnie zmianie.
    Engine* engine_ptr = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine_ptr) {
        if (newWidth == 0 || newHeight == 0) {
            // Okno zostalo zminimalizowane lub ma nieprawidlowy rozmiar.
            // Pomijamy logike zmiany rozmiaru, aby uniknac bledow (np. dzielenia przez zero).
            Logger::getInstance().info("Engine: Rozmiar okna zminimalizowany lub nieprawidlowy, pomijanie logiki zmiany rozmiaru.");
            return;
        }
        // Aktualizacja wewnetrznych wymiarow silnika.
        engine_ptr->m_width = newWidth;
        engine_ptr->m_height = newHeight;

        // Ustawienie nowego obszaru renderowania OpenGL (viewport).
        glViewport(0, 0, newWidth, newHeight);

        // Aktualizacja proporcji obrazu (aspect ratio) kamery.
        if (engine_ptr->m_camera) {
            engine_ptr->m_camera->setAspectRatio(static_cast<float>(newWidth) / static_cast<float>(newHeight));
        }

        // Aktualizacja macierzy projekcji dla renderera tekstu.
        if (engine_ptr->m_textRenderer && engine_ptr->m_textRenderer->isInitialized()) {
            engine_ptr->m_textRenderer->updateProjectionMatrix(newWidth, newHeight);
        }

        // Rozgloszenie zdarzenia zmiany rozmiaru okna.
        if (engine_ptr->m_eventManager) {
            WindowResizeEvent event(static_cast<unsigned int>(newWidth), static_cast<unsigned int>(newHeight));
            engine_ptr->m_eventManager->dispatch(event);
        }
        Logger::getInstance().info("Engine: Rozmiar okna zmieniony na: " + std::to_string(newWidth) + "x" + std::to_string(newHeight));
    }
}

void Engine::windowCloseCallback(GLFWwindow* window) {
    // Statyczna metoda obslugujaca zadanie zamkniecia okna.
    // Wywolywana przez GLFW, gdy uzytkownik probuje zamknac okno.
    Engine* engine_ptr = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine_ptr && engine_ptr->getEventManager()) {
        // Jesli silnik i menedzer zdarzen istnieja, rozglaszamy zdarzenie WindowCloseEvent.
        // Pozwala to innym systemom (np. samemu silnikowi przez onEvent) zareagowac.
        WindowCloseEvent event;
        engine_ptr->getEventManager()->dispatch(event);
    }
    else {
        // W przypadku braku silnika lub menedzera zdarzen, bezposrednio ustawiamy flage zamkniecia okna.
        // Jest to zachowanie awaryjne.
        glfwSetWindowShouldClose(window, 1);
    }
}

// --- Konstruktor i Destruktor ---

Engine::Engine()
    : m_window(nullptr),
    m_width(800), // Domyslna szerokosc
    m_height(600), // Domyslna wysokosc
    m_title("3D Engine"), // Domyslny tytul
    m_fullscreen(false),
    m_eventManager(nullptr),
    m_renderer(nullptr),
    m_camera(nullptr),
    m_inputManager(nullptr),
    m_textRenderer(nullptr),
    m_lightingManager(nullptr),
    m_shadowSystem(nullptr),
    m_collisionSystem(nullptr),
    m_gameStateManager(nullptr),
    m_initialized(false),
    m_lastFrameTime(0.0),
    m_deltaTime(0.0f),
    m_targetFPS(60.0f), // Domyslne docelowe FPS (obecnie nieuzywane do limitowania)
    m_vsyncEnabled(true), // Domyslnie VSync jest wlaczone
    m_autoClear(true),    // Domyslnie automatyczne czyszczenie buforow jest wlaczone
    m_autoSwap(true),     // Domyslnie automatyczna zamiana buforow jest wlaczona
    m_pcfRadius(1),       // Domyslny promien PCF dla cieni
    m_showFPS(false),     // Domyslnie licznik FPS jest wylaczony
    m_currentFPS(0.0),
    m_frameCount(0),
    m_lastFPSTime(0.0)
{
    // Inicjalizacja domyslnego koloru tla (ciemnoszary).
    m_backgroundColor[0] = 0.1f;
    m_backgroundColor[1] = 0.1f;
    m_backgroundColor[2] = 0.1f;
    m_backgroundColor[3] = 1.0f; // Alpha
    Logger::getInstance().info("Konstruktor Engine zostal wywolany.");
}

Engine::~Engine() {
    // Destruktor silnika. Wywoluje metode shutdown() w celu poprawnego
    // zwolnienia wszystkich zasobow i zamkniecia systemow.
    Logger::getInstance().info("Destruktor Engine: Rozpoczynanie procedury wylaczenia (shutdown)...");
    shutdown();
    Logger::getInstance().info("Destruktor Engine: Procedura wylaczenia zakonczona.");
    // Ustawienie instancji na nullptr, jesli jest to scisly singleton,
    // chociaz po zniszczeniu obiektu nie powinno byc juz do niego odwolan.
    if (instance == this) { // Dodatkowe zabezpieczenie
        instance = nullptr;
    }
}

// --- Metoda dostepowa Singletona ---

Engine* Engine::getInstance() {
    // Zwraca jedyna instancje silnika. Jesli instancja nie istnieje, tworzy ja.
    if (instance == nullptr) {
        instance = new Engine();
    }
    return instance;
}

// --- Glowna petla gry i zarzadzanie ---

bool Engine::initialize(int w, int h, const char* t) {
    // Glowna metoda inicjalizujaca silnik.
    if (m_initialized) {
        Logger::getInstance().warning("Engine: Silnik jest juz zainicjalizowany.");
        return true; // Zwracamy true, poniewaz jest juz gotowy.
    }
    Logger::getInstance().info("Engine: Rozpoczynanie inicjalizacji...");

    m_width = w;
    m_height = h;
    m_title = t;

    // Krok 1: Inicjalizacja GLFW.
    if (!initializeGLFW()) {
        Logger::getInstance().fatal("Engine: Blad podczas inicjalizacji GLFW.");
        return false;
    }

    // Krok 2: Tworzenie okna GLFW.
    if (!initializeWindow()) {
        Logger::getInstance().fatal("Engine: Blad podczas tworzenia okna GLFW.");
        glfwTerminate(); // Posprzatanie po GLFW.
        return false;
    }

    // Ustawienie wskaznika uzytkownika okna na biezaca instancje silnika.
    // Pozwala to statycznym callbackom GLFW na dostep do obiektu Engine.
    glfwSetWindowUserPointer(m_window, this);
    // Rejestracja callbackow specyficznych dla okna (rozmiar, zamkniecie).
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
    glfwSetWindowCloseCallback(m_window, windowCloseCallback);

    // Krok 3: Inicjalizacja GLAD i podstawowych ustawien OpenGL.
    if (!initializeOpenGL()) {
        Logger::getInstance().fatal("Engine: Blad podczas inicjalizacji OpenGL (GLAD).");
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    // Krok 4: Inicjalizacja menedzerow i systemow silnika.
    if (!initializeManagersAndSystems()) {
        Logger::getInstance().fatal("Engine: Blad podczas inicjalizacji menedzerow i systemow.");
        // shutdown() moze byc zbyt agresywne tutaj, jesli nie wszystkie zasoby zostaly jeszcze utworzone.
        // Nalezy rozwazyc czesciowe sprzatanie.
        if (m_window) glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    // Krok 5: Utworzenie i konfiguracja kamery.
    // Kamera jest tworzona tutaj, ale jej stan (pozycja, kierunek)
    // powinien byc zarzadzany przez aktywny stan gry.
    m_camera = std::make_unique<Camera>();
    if (!m_camera) {
        Logger::getInstance().fatal("Engine: Nie udalo sie utworzyc obiektu Camera!");
        // Podobnie, rozwazyc czesciowe sprzatanie.
        return false;
    }
    m_camera->setAspectRatio(static_cast<float>(m_width) / static_cast<float>(m_height));
    m_camera->setPosition(glm::vec3(0.0f, 1.0f, 3.0f)); // Domyslna pozycja kamery (oryginalna wartosc)
    // Usunieto: m_camera->lookAt(glm::vec3(0.0f, 0.0f, 0.0f)); // Ta linia powodowala blad, klasa Camera nie ma takiej metody lub ma inna sygnature

    // Krok 6: Utworzenie i inicjalizacja renderera.
    m_renderer = std::make_unique<Renderer>();
    if (!m_renderer || !m_renderer->initialize()) {
        Logger::getInstance().fatal("Engine: Tworzenie lub inicjalizacja Renderera nie powiodla sie!");
        return false;
    }

	if (m_renderer) {
		m_renderer->setCamera(m_camera.get()); // Ustawienie kamery w rendererze
	}
	else {
		Logger::getInstance().error("Engine: Renderer nie zostal poprawnie zainicjalizowany.");
		return false;
	}

    // Krok 7: Subskrypcja silnika na zdarzenie zamkniecia okna.
    // Pozwala to silnikowi na przechwycenie tego zdarzenia i odpowiednie zarzadzenie zamknieciem.
    if (m_eventManager) {
        m_eventManager->subscribe(EventType::WindowClose, this);
    }
    else {
        // To nie powinno sie zdarzyc, jesli initializeManagersAndSystems() powiodlo sie.
        Logger::getInstance().error("Engine: EventManager nie zostal zainicjalizowany. Silnik nie moze subskrybowac zdarzen.");
    }

    // Inicjalizacja czasu dla pierwszej klatki.
    m_lastFrameTime = glfwGetTime();

    // Uruchomienie ekranu powitalnego (splash screen).
    // Ekran powitalny jest prostym stanem gry, ktory wyswietla logo/obrazek
    // przez okreslony czas przed przejsciem do glownego menu lub stanu gry.
    runSplashScreen(); // Ta metoda ustawia m_initialized na true po swoim zakonczeniu.

    Logger::getInstance().info("Engine: Inicjalizacja zakonczona pomyslnie.");
    return true;
}

void Engine::runSplashScreen() {
    // Metoda odpowiedzialna za wyswietlenie ekranu powitalnego.
    if (!m_gameStateManager || !m_window || !m_renderer || !m_textRenderer || !m_inputManager || !m_eventManager) {
        Logger::getInstance().warning("Engine: Nie mozna uruchomic ekranu powitalnego - brakuje GameStateManager, okna, renderera, textRenderera, inputManagera lub eventManagera.");
        // Jesli nie mozna wyswietlic splash screena, ustawiamy silnik jako zainicjalizowany i kontynuujemy.
        m_initialized = true;
        return;
    }

    Logger::getInstance().info("Engine: Uruchamianie ekranu powitalnego (Splash Screen)...");

    // Zapisanie oryginalnych ustawien renderowania, ktore zostana przywrocone po splash screenie.
    bool originalAutoClear = m_autoClear;
    bool originalAutoSwap = m_autoSwap;
    bool originalFpsDisplayState = m_showFPS;
    glm::vec4 originalBgColor = glm::vec4(m_backgroundColor[0], m_backgroundColor[1], m_backgroundColor[2], m_backgroundColor[3]);

    // Ustawienia specyficzne dla splash screena.
    setAutoClear(true);
    setAutoSwap(true);
    if (m_showFPS) toggleFPSDisplay(); // Tymczasowo wylaczamy licznik FPS
    setBackgroundColor(1.0f, 1.0f, 1.0f, 1.0f); // Białe tło

    // Konfiguracja splash screena (sciezka do tekstury, czas trwania).
    // TODO: Przeniesc te wartosci do pliku konfiguracyjnego lub parametrow.
    std::string splashTexturePath = "assets/textures/splash_logo.jpg";
    float splashDuration = 3.0f; // Czas trwania w sekundach

    // Utworzenie i dodanie stanu splash screena do menedzera stanow.
    auto splashState = std::make_unique<SplashScreenState>(splashTexturePath, splashDuration);
    // Potrzebujemy surowego wskaznika, aby sprawdzic, czy stan sie zakonczyl,
    // poniewaz unique_ptr zostanie przeniesiony do GameStateManagera.
    SplashScreenState* rawSplashStatePtr = splashState.get();
    if (!rawSplashStatePtr) {
        Logger::getInstance().error("Engine: Nie udalo sie utworzyc stanu SplashScreenState.");
        m_initialized = true; // Awaryjne ustawienie
        return;
    }
    m_gameStateManager->pushState(std::move(splashState));

    // Petla dla splash screena.
    double splashLastFrameTime = glfwGetTime();
    while (rawSplashStatePtr && !rawSplashStatePtr->isFinished() && !glfwWindowShouldClose(m_window)) {
        double splashCurrentFrameTime = glfwGetTime();
        float splashDeltaTime = static_cast<float>(splashCurrentFrameTime - splashLastFrameTime);
        splashLastFrameTime = splashCurrentFrameTime;

        glfwPollEvents(); // Przetwarzanie zdarzen systemowych GLFW

        // Aktualizacja InputManagera i przekazanie zdarzen do stanu splash screen.
        // Upewniamy sie, ze wskazniki nie sa null.
        if (m_inputManager) m_inputManager->update(); // Aktualizacja stanow wejscia

        // Obsluga zdarzen, aktualizacja logiki i renderowanie stanu splash screen.
        if (m_gameStateManager && !m_gameStateManager->isEmpty()) {
            if (m_inputManager && m_eventManager) {
                m_gameStateManager->handleEventsCurrentState(m_inputManager.get(), m_eventManager.get());
            }
            m_gameStateManager->updateCurrentState(splashDeltaTime);
        }


        // Czyszczenie buforow (kolorem tla ustawionym dla splash screena).
        if (m_autoClear) clearBuffers();

        if (m_gameStateManager && !m_gameStateManager->isEmpty() && m_renderer) {
            m_gameStateManager->renderCurrentState(m_renderer.get());
        }

        // Zamiana buforow.
        if (m_autoSwap) glfwSwapBuffers(m_window);
    }

    // Usuniecie stanu splash screen ze stosu po jego zakonczeniu lub zamknieciu okna.
    // Poprawiony warunek:
    if (m_gameStateManager && !m_gameStateManager->isEmpty()) {
        // Dodatkowe sprawdzenie, czy stan na wierzcholku to faktycznie nasz splash screen
        // moze byc bardziej skomplikowane, jesli nie mamy pewnosci.
        // Na razie zakladamy, ze jesli petla sie zakonczyla, to splash screen jest na gorze.
        Logger::getInstance().info("Engine: Ekran powitalny zakonczony lub okno zamkniete. Usuwanie SplashScreenState...");
        m_gameStateManager->popState();
    }

    // Przywrocenie oryginalnych ustawien renderowania.
    setAutoClear(originalAutoClear);
    setAutoSwap(originalAutoSwap);
    setBackgroundColor(originalBgColor.r, originalBgColor.g, originalBgColor.b, originalBgColor.a);
    if (originalFpsDisplayState && !m_showFPS) toggleFPSDisplay(); // Przywroc stan wyswietlania FPS

    Logger::getInstance().info("Engine: Sekwencja ekranu powitalnego zakonczona.");
    m_initialized = true; // Ustawienie flagi inicjalizacji silnika po splash screenie.
}


void Engine::update() {
    // Glowna metoda aktualizacji logiki silnika, wywolywana w kazdej klatce.
    if (!m_initialized || !m_window) {
        // Jesli silnik nie jest zainicjalizowany lub okno nie istnieje, nie robimy nic.
        return;
    }

    // Obliczenie deltaTime - czasu, ktory uplynal od ostatniej klatki.
    double currentTime = glfwGetTime();
    m_deltaTime = static_cast<float>(currentTime - m_lastFrameTime);
    m_lastFrameTime = currentTime;

    // Przetwarzanie zdarzen systemowych GLFW (np. wejscie, zmiana rozmiaru okna).
    glfwPollEvents();

    // Aktualizacja InputManagera (kopiowanie stanow, obliczanie delty myszy dla pollingu).
    if (m_inputManager) {
        m_inputManager->update();
    }

    // Aktualizacja systemu kolizji.
    // System kolizji moze generowac zdarzenia CollisionEvent, wiec potrzebuje dostepu do EventManagera.
    if (m_collisionSystem && m_eventManager) {
        m_collisionSystem->update(m_eventManager.get());
    }

    // Zarzadzanie stanami gry: obsluga zdarzen i aktualizacja logiki aktywnego stanu.
    if (m_gameStateManager && !m_gameStateManager->isEmpty()) {
        // Przekazanie obslugi zdarzen do aktywnego stanu gry.
        if (m_inputManager && m_eventManager) {
            m_gameStateManager->handleEventsCurrentState(m_inputManager.get(), m_eventManager.get());
        }
        // Aktualizacja logiki aktywnego stanu gry.
        m_gameStateManager->updateCurrentState(m_deltaTime);
    }
    else if (m_gameStateManager && m_gameStateManager->isEmpty() && m_initialized) {
        // Jesli stos stanow jest pusty PO inicjalizacji silnika (np. po splash screenie
        // nie dodano zadnego innego stanu lub wszystkie stany zostaly usuniete),
        // oznacza to, ze aplikacja powinna sie zakonczyc.
        Logger::getInstance().info("Engine: GameStateManager jest pusty. Zgłaszanie żądania zamknięcia okna.");
        if (m_window) glfwSetWindowShouldClose(m_window, 1);
    }
}

void Engine::render() {
    // Glowna metoda renderowania sceny, wywolywana w kazdej klatce.
    if (!m_initialized || !m_renderer || !m_window || !m_lightingManager || !m_shadowSystem || !m_camera) {
        // Sprawdzenie, czy wszystkie krytyczne komponenty sa zainicjalizowane.
        Logger::getInstance().error("Engine::render - Krytyczny komponent nie zostal zainicjalizowany (renderer, okno, oswietlenie, cienie lub kamera).");
        return;
    }

    // Krok 1: Generowanie map cieni.
    // ShadowSystem renderuje scene z perspektywy kazdego aktywnego zrodla swiatla rzucajacego cien,
    // zapisujac informacje o glebokosci do tekstur map cieni.
    // Potrzebuje dostepu do obiektow renderowalnych, ktore sa pobierane z Renderera.
    if (m_renderer) { // Dodatkowe sprawdzenie, chociaz juz jest w warunku powyzej
        m_shadowSystem->generateShadowMaps(*m_lightingManager, m_renderer->getRenderables(), m_width, m_height);
    }


    // Krok 2: Czyszczenie buforow (koloru i glebokosci) przed glownym renderowaniem.
    // Wykonywane tylko jesli flaga m_autoClear jest ustawiona.
    if (m_autoClear) {
        clearBuffers();
    }
    // Ustawienie viewportu na caly obszar okna (moglo sie zmienic np. podczas generowania map cieni).
    glViewport(0, 0, m_width, m_height);

    // Krok 3: Ustawienie globalnych uniformow dla shaderow (np. oswietlenie, pozycja kamery).
    // TODO: To powinno byc bardziej elastyczne. Pobieranie "defaultPrimitiveShader" tutaj
    // jest tymczasowe i nie skaluje sie dobrze. Idealnie, kazdy material/obiekt
    // mialby swoj shader, a system renderujacy zarzadzalby ustawianiem odpowiednich uniformow.
    std::shared_ptr<Shader> defaultShader = ResourceManager::getInstance().getShader("defaultPrimitiveShader");
    if (defaultShader && defaultShader->getID() != 0) {
        defaultShader->use(); // Aktywacja shadera
        // Ustawienie pozycji kamery (widza) w przestrzeni swiata.
        defaultShader->setVec3("viewPos_World", m_camera->getPosition());
        // Ustawienie promienia dla PCF (Percentage Closer Filtering) dla miekkich cieni.
        defaultShader->setInt("u_pcfRadius", m_pcfRadius);
        // Wyslanie danych o wszystkich aktywnych swiatlach do shadera.
        m_lightingManager->uploadGeneralLightUniforms(defaultShader);
        // Wyslanie danych o mapach cieni i macierzach przestrzeni swiatla do shadera.
        m_shadowSystem->uploadShadowUniforms(defaultShader, *m_lightingManager);
    }

    // Krok 4: Renderowanie aktywnego stanu gry.
    // Aktywny stan gry jest odpowiedzialny za renderowanie swoich obiektow.
    if (m_gameStateManager && !m_gameStateManager->isEmpty()) {
        m_gameStateManager->renderCurrentState(m_renderer.get());
    }

    // Krok 5: Opcjonalne wyswietlanie licznika FPS i innych informacji diagnostycznych.
    if (m_showFPS && m_textRenderer != nullptr && m_textRenderer->isInitialized()) {
        calculateFPS(); // Obliczenie aktualnego FPS
        std::stringstream ss;
        ss << "FPS: " << std::fixed << std::setprecision(0) << m_currentFPS;
        ss << " | PCF: " << (2 * m_pcfRadius + 1) << "x" << (2 * m_pcfRadius + 1);
        if (m_shadowSystem) {
            ss << " | SL Shdw: " << m_shadowSystem->getActiveSpotLightShadowCastersCount() << "/" << MAX_SHADOW_CASTING_SPOT_LIGHTS;
            ss << " | PL Shdw: " << m_shadowSystem->getActivePointLightShadowCastersCount() << "/" << MAX_SHADOW_CASTING_POINT_LIGHTS;
        }
        // Renderowanie tekstu w lewym gornym rogu.
        m_textRenderer->renderText(ss.str(), 10.0f, static_cast<float>(m_height) - 30.0f, 1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // Krok 6: Zamiana buforow (przedniego z tylnym), aby wyswietlic wyrenderowana klatke.
    // Wykonywane tylko jesli flaga m_autoSwap jest ustawiona.
    if (m_autoSwap) {
        glfwSwapBuffers(m_window);
    }
}

void Engine::shutdown() {
    // Metoda odpowiedzialna za poprawne zwolnienie wszystkich zasobow i zamkniecie systemow silnika.
    Logger::getInstance().info("Engine: Rozpoczynanie procedury wylaczania silnika (shutdown)...");
    if (!m_initialized && m_window == nullptr) { // Dodatkowe sprawdzenie m_window, bo m_initialized moze byc false po nieudanej inicjalizacji
        Logger::getInstance().warning("Engine: Wywolano shutdown(), ale silnik nie byl zainicjalizowany lub juz zostal wylaczony.");
        return;
    }

    // Kolejnosc zwalniania zasobow jest wazna, aby uniknac problemow z zaleznosciami.
    // Generalnie, systemy powinny byc zamykane w odwrotnej kolejnosci do ich tworzenia.

    // 1. Sprzatanie stanow gry.
    if (m_gameStateManager) {
        m_gameStateManager->cleanup(); // Wywoluje cleanup() na wszystkich stanach.
        m_gameStateManager.reset();    // Zwalnia pamiec po GameStateManager.
        Logger::getInstance().info("Engine: GameStateManager wylaczony.");
    }

    // 2. Anulowanie subskrypcji silnika na zdarzenia (jesli byl subskrybentem).
    if (m_eventManager) {
        m_eventManager->unsubscribeAll(this); // 'this' poniewaz Engine implementuje IEventListener.
    }

    // 3. Zwalnianie systemow (kolejnosc moze miec znaczenie).
    m_collisionSystem.reset();
    Logger::getInstance().info("Engine: CollisionSystem wylaczony.");

    m_shadowSystem.reset(); // ShadowSystem moze uzywac shaderow z ResourceManager
    Logger::getInstance().info("Engine: ShadowSystem wylaczony.");

    m_lightingManager.reset();
    Logger::getInstance().info("Engine: LightingManager wylaczony.");

    // 4. Sprzatanie TextRenderer (jesli byl zainicjalizowany).
    // TextRenderer jest singletonem, wiec jego cleanup moze byc specyficzny.
    if (m_textRenderer && m_textRenderer->isInitialized()) {
        m_textRenderer->cleanup(); // Zakladajac, ze TextRenderer ma metode cleanup.
        // Nie resetujemy wskaznika m_textRenderer, jesli jest to singleton zarzadzany zewnetrznie.
        // Jesli Engine mialby byc wlascicielem (np. unique_ptr), to tutaj bylby reset.
    }
    Logger::getInstance().info("Engine: TextRenderer (jesli istnial) posprzatany.");


    // 5. Zwalnianie menedzerow.
    m_inputManager.reset();
    Logger::getInstance().info("Engine: InputManager wylaczony.");

    m_eventManager.reset();
    Logger::getInstance().info("Engine: EventManager wylaczony.");

    // 6. Zamykanie ResourceManager (singleton).
    ResourceManager::getInstance().shutdown();
    Logger::getInstance().info("Engine: ResourceManager wylaczony.");

    // 7. Zwalnianie Renderera i Kamery.
    m_renderer.reset();
    Logger::getInstance().info("Engine: Renderer wylaczony.");
    m_camera.reset();
    Logger::getInstance().info("Engine: Camera wylaczona.");


    // 8. Niszczenie okna GLFW i terminacja GLFW.
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        Logger::getInstance().info("Engine: Okno GLFW zniszczone.");
    }
    glfwTerminate();
    Logger::getInstance().info("Engine: GLFW zakonczone.");

    m_initialized = false; // Ustawienie flagi, ze silnik nie jest juz zainicjalizowany.
    Logger::getInstance().info("Engine: Procedura wylaczania silnika zakonczona pomyslnie.");
}

void Engine::onEvent(Event& event) {
    // Metoda obslugi zdarzen, na ktore silnik jest zasubskrybowany.
    // Obecnie silnik nasluchuje tylko na WindowCloseEvent.
    if (event.getEventType() == EventType::WindowClose) {
        Logger::getInstance().info("Engine: Otrzymano zdarzenie WindowCloseEvent. Ustawianie flagi zamkniecia okna.");
        if (m_window) {
            glfwSetWindowShouldClose(m_window, 1); // Ustawienie flagi GLFW, aby zakonczyc glowna petle.
        }
        event.handled = true; // Oznaczenie zdarzenia jako obsluzonego.
    }
}

// --- Prywatne metody inicjalizacyjne ---

bool Engine::initializeGLFW() {
    Logger::getInstance().debug("Engine: Inicjalizacja GLFW...");
    // Ustawienie globalnego callbacku dla bledow GLFW.
    // Musi byc wywolane przed glfwInit().
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        Logger::getInstance().fatal("Engine: Nie udalo sie zainicjalizowac GLFW!");
        return false;
    }
    // Ustawienie wskazowek (hints) dla tworzonego kontekstu OpenGL.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Wersja OpenGL 3.x
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // Wersja OpenGL x.3
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Uzycie profilu Core (bez przestarzalych funkcji)
#ifdef __APPLE__
    // Dla systemow macOS moze byc wymagane ustawienie kompatybilnosci wprzod.
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    Logger::getInstance().info("Engine: GLFW zainicjalizowane pomyslnie.");
    return true;
}

bool Engine::initializeWindow() {
    Logger::getInstance().debug("Engine: Tworzenie okna GLFW...");
    // Wybor monitora: glowny monitor dla trybu pelnoekranowego, nullptr dla trybu okienkowego.
    GLFWmonitor* monitor = m_fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), monitor, nullptr);
    if (!m_window) {
        Logger::getInstance().fatal("Engine: Nie udalo sie utworzyc okna GLFW!");
        // Dodatkowe logowanie bledu GLFW, jesli dostepne
        const char* glfwError;
        if (glfwGetError(&glfwError) != GLFW_NO_ERROR) {
            Logger::getInstance().error("Engine: Blad GLFW przy tworzeniu okna: " + std::string(glfwError));
        }
        return false;
    }
    // Ustawienie biezacego kontekstu OpenGL na nowo utworzone okno.
    glfwMakeContextCurrent(m_window);
    // Ustawienie VSync (synchronizacji pionowej).
    glfwSwapInterval(m_vsyncEnabled ? 1 : 0); // 1 - wlaczone, 0 - wylaczone
    Logger::getInstance().info("Engine: Okno GLFW utworzone pomyslnie.");
    return true;
}

bool Engine::initializeOpenGL() {
    Logger::getInstance().debug("Engine: Inicjalizacja OpenGL (GLAD)...");
    // Ladowanie wskaznikow do funkcji OpenGL za pomoca GLAD.
    // gladLoadGLLoader wymaga funkcji pobierajacej adresy funkcji OpenGL (specyficznej dla platformy).
    // glfwGetProcAddress jest odpowiednia funkcja dla GLFW.
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Logger::getInstance().fatal("Engine: Nie udalo sie zainicjalizowac GLAD!");
        return false;
    }
    Logger::getInstance().info("Engine: GLAD zainicjalizowany. Wersja OpenGL: " + std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION))));

    // Podstawowe ustawienia OpenGL.
    glEnable(GL_DEPTH_TEST); // Wlaczenie testu glebokosci.
    glEnable(GL_BLEND);      // Wlaczenie mieszania kolorow (blendowania).
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Standardowa funkcja blendowania dla przezroczystosci.
    // glEnable(GL_CULL_FACE); // Opcjonalnie: wlaczenie odrzucania niewidocznych scian (culling).
    // glCullFace(GL_BACK);    // Odrzucanie tylnych scian.

    // Ustawienie poczatkowego obszaru renderowania (viewport).
    glViewport(0, 0, m_width, m_height);
    Logger::getInstance().info("Engine: Podstawowe ustawienia OpenGL skonfigurowane.");
    return true;
}

bool Engine::initializeManagersAndSystems() {
    Logger::getInstance().debug("Engine: Inicjalizacja menedzerow i systemow...");

    // EventManager - musi byc pierwszy, poniewaz inne systemy moga chciec sie na nim rejestrowac.
    m_eventManager = std::make_unique<EventManager>();
    if (!m_eventManager) {
        Logger::getInstance().fatal("Engine: Nie udalo sie utworzyc EventManagera!");
        return false;
    }
    Logger::getInstance().info("Engine: EventManager utworzony.");

    // ResourceManager (Singleton)
    if (!ResourceManager::getInstance().initialize()) { // Zakladamy, ze ResourceManager ma metode initialize()
        Logger::getInstance().fatal("Engine: Nie udalo sie zainicjalizowac ResourceManager!");
        return false;
    }
    Logger::getInstance().info("Engine: ResourceManager zainicjalizowany.");

    // InputManager - potrzebuje okna i EventManagera.
    if (!initializeInputManager()) {
        Logger::getInstance().fatal("Engine: Nie udalo sie zainicjalizowac InputManagera!");
        return false;
    }
    // Logger jest juz w initializeInputManager()

    // LightingManager
    m_lightingManager = std::make_unique<LightingManager>();
    if (!m_lightingManager) {
        Logger::getInstance().fatal("Engine: Nie udalo sie utworzyc LightingManagera!");
        return false;
    }
    Logger::getInstance().info("Engine: LightingManager utworzony.");
    initializeLightingDefaults(); // Ustawienie domyslnych swiatel.

    // ShadowSystem - potrzebuje ResourceManager do ladowania shaderow cieni.
    m_shadowSystem = std::make_unique<ShadowSystem>(
        SHADOW_MAP_WIDTH_ENGINE, SHADOW_MAP_HEIGHT_ENGINE,
        SHADOW_CUBE_MAP_WIDTH_ENGINE, SHADOW_CUBE_MAP_HEIGHT_ENGINE
    );
    if (!m_shadowSystem) {
        Logger::getInstance().fatal("Engine: Nie udalo sie utworzyc ShadowSystem!");
        return false;
    }
    // Sciezki do shaderow cieni - mozna je przeniesc do konfiguracji.
    // Zakladamy, ze ResourceManager potrafi zaladowac shadery po nazwie, jesli zostaly wczesniej dodane,
    // lub ShadowSystem sam je laduje, jesli dostanie sciezki.
    // Tutaj ShadowSystem::initialize przyjmuje nazwe shadera i sciezki.
    if (!m_shadowSystem->initialize(ResourceManager::getInstance(), "depthPassShader", "assets/shaders/depth_shader.vert", "assets/shaders/depth_shader.frag")) {
        Logger::getInstance().fatal("Engine: Nie udalo sie zainicjalizowac ShadowSystem!");
        m_shadowSystem.reset(); // Zwolnienie pamieci, jesli inicjalizacja sie nie powiodla.
        return false;
    }
    Logger::getInstance().info("Engine: ShadowSystem zainicjalizowany.");

    // CollisionSystem
    m_collisionSystem = std::make_unique<CollisionSystem>();
    if (!m_collisionSystem) {
        Logger::getInstance().fatal("Engine: Nie udalo sie utworzyc CollisionSystem!");
        return false;
    }
    Logger::getInstance().info("Engine: CollisionSystem utworzony.");

    // GameStateManager - potrzebuje wskaznika do Engine.
    if (!initializeGameStateManager()) {
        Logger::getInstance().fatal("Engine: Nie udalo sie zainicjalizowac GameStateManagera!");
        return false;
    }
    // Logger jest juz w initializeGameStateManager()

    // TextRenderer - potrzebuje okna.
    if (!initializeTextRenderer()) {
        Logger::getInstance().warning("Engine: Nie udalo sie zainicjalizowac TextRenderer (blad niekrytyczny).");
    }
    // Logger jest juz w initializeTextRenderer()

    Logger::getInstance().info("Engine: Wszystkie glowne menedzery i systemy zainicjalizowane.");
    return true;
}

bool Engine::initializeInputManager() {
    // Sprawdzenie warunkow wstepnych.
    if (!m_window) {
        Logger::getInstance().error("Engine: Nie mozna zainicjalizowac InputManagera - wskaznik do okna jest pusty.");
        return false;
    }
    if (!m_eventManager) {
        Logger::getInstance().fatal("Engine: Nie mozna zainicjalizowac InputManagera - wskaznik do EventManagera jest pusty.");
        return false;
    }
    // Utworzenie instancji InputManagera.
    m_inputManager = std::make_unique<InputManager>(m_window, m_eventManager.get());
    if (!m_inputManager) {
        Logger::getInstance().fatal("Engine: Nie udalo sie utworzyc instancji InputManagera.");
        return false;
    }
    Logger::getInstance().info("Engine: InputManager zainicjalizowany.");
    return true;
}

void Engine::initializeLightingDefaults() {
    // Ustawienie domyslnych parametrow oswietlenia.
    if (!m_lightingManager) {
        Logger::getInstance().error("Engine: Nie mozna zainicjalizowac domyslnego oswietlenia - LightingManager jest pusty.");
        return;
    }
    Logger::getInstance().debug("Engine: Inicjalizacja domyslnych ustawien oswietlenia poprzez LightingManager...");

    DirectionalLight defaultDirLight; // Tworzymy domyslne swiatlo kierunkowe.
    defaultDirLight.direction = glm::normalize(glm::vec3(0.5f, -0.8f, -0.5f)); // Kierunek "z gory, lekko z boku"
    defaultDirLight.ambient = glm::vec3(0.2f, 0.2f, 0.22f);   // Delikatne swiatlo otoczenia
    defaultDirLight.diffuse = glm::vec3(0.7f, 0.7f, 0.65f);   // Glowne swiatlo rozproszone
    defaultDirLight.specular = glm::vec3(0.5f, 0.5f, 0.5f);  // Odbicia lustrzane
    defaultDirLight.enabled = true;                           // Swiatlo jest wlaczone
    m_lightingManager->setDirectionalLight(defaultDirLight); // Ustawienie swiatla w menedzerze

    // Mozna tutaj dodac rowniez domyslne swiatla punktowe lub reflektory, jesli potrzebne.
    // Przyklad:
    // PointLight defaultPointLight;
    // defaultPointLight.position = glm::vec3(0.0f, 2.0f, 0.0f);
    // defaultPointLight.diffuse = glm::vec3(0.8f, 0.0f, 0.0f); // Czerwone swiatlo
    // m_lightingManager->addPointLight(defaultPointLight);

    Logger::getInstance().info("Engine: Domyslne swiatlo kierunkowe skonfigurowane w LightingManager.");
}

bool Engine::initializeGameStateManager() {
    // Utworzenie instancji GameStateManagera.
    // Przekazujemy 'this' (wskaznik do biezacej instancji Engine) jako kontekst.
    m_gameStateManager = std::make_unique<GameStateManager>(this);
    if (!m_gameStateManager) {
        Logger::getInstance().fatal("Engine: Nie udalo sie utworzyc GameStateManagera!");
        return false;
    }
    Logger::getInstance().info("Engine: GameStateManager utworzony.");
    // Poczatkowy stan gry (np. MainMenuState, SplashScreenState) jest zazwyczaj dodawany
    // do GameStateManagera po pelnej inicjalizacji silnika, np. w metodzie initialize() silnika
    // lub w glownej funkcji main.cpp. W tym przypadku, SplashScreen jest dodawany w runSplashScreen().
    return true;
}

bool Engine::initializeTextRenderer() {
    // Sprawdzenie warunkow wstepnych.
    if (m_window == nullptr) {
        Logger::getInstance().error("Engine: Nie mozna zainicjalizowac TextRenderer - wskaznik do okna jest pusty.");
        return false;
    }
    // TextRenderer jest singletonem.
    m_textRenderer = TextRenderer::getInstance();
    // Sciezka do czcionki i jej nazwa - mozna przeniesc do konfiguracji.
    if (!m_textRenderer->initialize(m_window, "assets/fonts/arial.ttf", "arial", 24, m_width, m_height)) {
        Logger::getInstance().error("Engine: Inicjalizacja TextRenderer nie powiodla sie.");
        // Nie resetujemy m_textRenderer, poniewaz jest to singleton.
        // Jesli nie jest krytyczny, mozemy zwrocic true lub false w zaleznosci od polityki.
        return false; // Traktujemy jako blad, ale nie krytyczny dla samego silnika.
    }
    Logger::getInstance().info("Engine: TextRenderer zainicjalizowany.");
    return true;
}

// --- Metody publiczne ---

void Engine::calculateFPS() {
    // Oblicza i aktualizuje licznik klatek na sekunde (FPS).
    m_frameCount++;
    double currentTime = glfwGetTime();
    // Aktualizuj FPS co sekunde.
    if (currentTime - m_lastFPSTime >= 1.0) { // 1.0 sekunda
        m_currentFPS = static_cast<double>(m_frameCount) / (currentTime - m_lastFPSTime);
        m_frameCount = 0; // Reset licznika klatek
        m_lastFPSTime = currentTime; // Zapisz czas ostatniej aktualizacji FPS
    }
}

bool Engine::shouldClose() const {
    // Sprawdza, czy silnik powinien zakonczyc dzialanie.
    if (!m_window) {
        // Jesli okno nie istnieje, nie ma sensu kontynuowac.
        Logger::getInstance().warning("Engine::shouldClose() - Okno nie istnieje, zamykanie.");
        return true;
    }
    // Sprawdzenie, czy GLFW zglosilo zadanie zamkniecia okna (np. przez klikniecie 'X').
    bool glfwRequestsClose = glfwWindowShouldClose(m_window);

    // Dodatkowy warunek: jesli silnik jest zainicjalizowany, ale GameStateManager
    // jest pusty (np. wszystkie stany zostaly usuniete), rowniez konczymy.
    if (m_initialized && m_gameStateManager && m_gameStateManager->isEmpty()) {
        Logger::getInstance().info("Engine::shouldClose() - Brak aktywnych stanow gry, silnik zostanie zamkniety.");
        return true;
    }
    return glfwRequestsClose;
}

// Implementacja gettera dla ResourceManager, ktory byl tylko zadeklarowany w .h
ResourceManager& Engine::getResourceManager() const {
    return ResourceManager::getInstance();
}


// --- Settery i Gettery ---
// Komentarze dla tych metod sa glownie w pliku .h, poniewaz sa to proste operacje.

void Engine::setFullscreen(bool fs) {
    if (!m_window) return;
    if (m_fullscreen == fs) return; // Bez zmian, jesli stan jest juz taki sam.
    m_fullscreen = fs;

    GLFWmonitor* monitorToUse = m_fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    if (m_fullscreen) {
        // Przejscie do trybu pelnoekranowego.
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        if (mode) {
            glfwSetWindowMonitor(m_window, monitorToUse, 0, 0, mode->width, mode->height, mode->refreshRate);
            Logger::getInstance().info("Engine: Ustawiono tryb pelnoekranowy (" + std::to_string(mode->width) + "x" + std::to_string(mode->height) + ").");
        }
        else {
            Logger::getInstance().error("Engine: Nie udalo sie pobrac trybu wideo dla glownego monitora.");
            m_fullscreen = false; // Powrot do poprzedniego stanu, jesli blad.
        }
    }
    else {
        // Powrot do trybu okienkowego.
        // Uzywamy zapisanych wczesniej wymiarow m_width, m_height lub domyslnych.
        // Pozycje (100, 100) sa przykladowe.
        glfwSetWindowMonitor(m_window, nullptr, 100, 100, m_width, m_height, 0);
        Logger::getInstance().info("Engine: Ustawiono tryb okienkowy (" + std::to_string(m_width) + "x" + std::to_string(m_height) + ").");
    }
    // Po zmianie trybu, VSync mogl sie zresetowac, wiec ustawiamy go ponownie.
    if (m_window) glfwSwapInterval(m_vsyncEnabled ? 1 : 0);
}

void Engine::setTargetFPS(float fps) {
    m_targetFPS = fps;
    // Logger::getInstance().info("Engine: Docelowy FPS ustawiony na: " + std::to_string(fps));
    // Obecnie ta wartosc nie jest uzywana do aktywnego limitowania FPS.
}

void Engine::setBackgroundColor(float r, float g, float b, float a) {
    m_backgroundColor[0] = r;
    m_backgroundColor[1] = g;
    m_backgroundColor[2] = b;
    m_backgroundColor[3] = a;
}

void Engine::setVSync(bool enabled) {
    if (!m_window) return;
    m_vsyncEnabled = enabled;
    glfwSwapInterval(enabled ? 1 : 0);
    Logger::getInstance().info("Engine: VSync " + std::string(enabled ? "wlaczony" : "wylaczony") + ".");
}

void Engine::setPCFQuality(int radius) {
    // Ograniczenie promienia PCF do sensownego zakresu (np. 0-4).
    // Zbyt duzy promien moze byc kosztowny obliczeniowo.
    m_pcfRadius = std::max(0, std::min(radius, 4)); // Przyklad ograniczenia
    Logger::getInstance().info("Engine: Jakosc PCF (promien) ustawiona na: " + std::to_string(m_pcfRadius) +
        " (Rozmiar jadra: " + std::to_string(2 * m_pcfRadius + 1) + "x" + std::to_string(2 * m_pcfRadius + 1) + ")");
}

int Engine::getPCFQuality() const {
    return m_pcfRadius;
}

void Engine::toggleFPSDisplay() {
    m_showFPS = !m_showFPS;
    Logger::getInstance().info("Engine: Wyswietlanie FPS " + std::string(m_showFPS ? "wlaczone" : "wylaczone") + ".");
}

// --- Metody zwiazane z oswietleniem (delegacja do LightingManager) ---

bool Engine::enableSpotLightShadow(int globalLightIndex, bool enable) {
    if (!m_shadowSystem || !m_lightingManager) {
        Logger::getInstance().error("Engine::enableSpotLightShadow - ShadowSystem lub LightingManager jest pusty.");
        return false;
    }
    // Delegacja do ShadowSystem, ktory zarzadza slotami cieni.
    return m_shadowSystem->enableSpotLightShadow(globalLightIndex, enable, *m_lightingManager);
}

bool Engine::enablePointLightShadow(int globalLightIndex, bool enable) {
    if (!m_shadowSystem || !m_lightingManager) {
        Logger::getInstance().error("Engine::enablePointLightShadow - ShadowSystem lub LightingManager jest pusty.");
        return false;
    }
    return m_shadowSystem->enablePointLightShadow(globalLightIndex, enable, *m_lightingManager);
}

void Engine::setDirectionalLight(const DirectionalLight& light) {
    if (m_lightingManager) {
        m_lightingManager->setDirectionalLight(light);
    }
    else {
        Logger::getInstance().warning("Engine: Proba ustawienia swiatla kierunkowego, ale LightingManager jest pusty.");
    }
}

const DirectionalLight& Engine::getDirectionalLight() const {
    if (!m_lightingManager) {
        Logger::getInstance().error("Engine::getDirectionalLight (const) - LightingManager jest pusty. Zwracanie statycznego obiektu tymczasowego.");
        static DirectionalLight dummyLight; // Zwracamy obiekt tymczasowy, aby uniknac bledu.
        return dummyLight;
    }
    return m_lightingManager->getDirectionalLight();
}

DirectionalLight& Engine::getMutableDirectionalLight() {
    if (!m_lightingManager) {
        Logger::getInstance().fatal("Engine::getMutableDirectionalLight - LightingManager jest pusty. Jest to blad krytyczny. Zwracanie statycznego obiektu tymczasowego.");
        static DirectionalLight dummyLight; // To nie jest idealne, ale zapobiega awarii.
        return dummyLight;
    }
    // Zakladamy, ze LightingManager::getDirectionalLight() zwraca referencje do modyfikowalnego obiektu.
    return m_lightingManager->getDirectionalLight();
}

void Engine::addPointLight(const PointLight& light) {
    if (m_lightingManager) {
        m_lightingManager->addPointLight(light);
    }
    else {
        Logger::getInstance().warning("Engine: Proba dodania swiatla punktowego, ale LightingManager jest pusty.");
    }
}

std::vector<PointLight>& Engine::getPointLights() {
    if (!m_lightingManager) {
        Logger::getInstance().error("Engine::getPointLights - LightingManager jest pusty. Zwracanie statycznego pustego wektora.");
        static std::vector<PointLight> emptyVec;
        return emptyVec;
    }
    return m_lightingManager->getPointLights();
}

const std::vector<PointLight>& Engine::getPointLights() const {
    if (!m_lightingManager) {
        Logger::getInstance().error("Engine::getPointLights (const) - LightingManager jest pusty. Zwracanie statycznego pustego wektora.");
        static std::vector<PointLight> emptyVec;
        return emptyVec;
    }
    return m_lightingManager->getPointLights();
}

void Engine::clearPointLights() {
    if (!m_lightingManager || !m_shadowSystem) {
        Logger::getInstance().error("Engine::clearPointLights - LightingManager lub ShadowSystem jest pusty.");
        return;
    }
    // Przed wyczyszczeniem swiatel z LightingManagera, musimy poinformowac ShadowSystem,
    // aby zwolnil sloty cieni zajmowane przez te swiatla.
    const auto& lights = m_lightingManager->getPointLights();
    for (size_t i = 0; i < lights.size(); ++i) {
        if (lights[i].castsShadow && lights[i].shadowDataIndex != -1) { // Sprawdzamy tez shadowDataIndex
            m_shadowSystem->enablePointLightShadow(static_cast<int>(i), false, *m_lightingManager);
        }
    }
    m_lightingManager->clearPointLights(); // Teraz czyscimy swiatla.
    Logger::getInstance().info("Engine: Wszystkie swiatla punktowe zostaly wyczyszczone.");
}

void Engine::addSpotLight(const SpotLight& light) {
    if (m_lightingManager) {
        m_lightingManager->addSpotLight(light);
    }
    else {
        Logger::getInstance().warning("Engine: Proba dodania reflektora, ale LightingManager jest pusty.");
    }
}

std::vector<SpotLight>& Engine::getSpotLights() {
    if (!m_lightingManager) {
        Logger::getInstance().error("Engine::getSpotLights - LightingManager jest pusty. Zwracanie statycznego pustego wektora.");
        static std::vector<SpotLight> emptyVec;
        return emptyVec;
    }
    return m_lightingManager->getSpotLights();
}

const std::vector<SpotLight>& Engine::getSpotLights() const {
    if (!m_lightingManager) {
        Logger::getInstance().error("Engine::getSpotLights (const) - LightingManager jest pusty. Zwracanie statycznego pustego wektora.");
        static std::vector<SpotLight> emptyVec;
        return emptyVec;
    }
    return m_lightingManager->getSpotLights();
}

void Engine::clearSpotLights() {
    if (!m_lightingManager || !m_shadowSystem) {
        Logger::getInstance().error("Engine::clearSpotLights - LightingManager lub ShadowSystem jest pusty.");
        return;
    }
    // Analogicznie do clearPointLights.
    const auto& lights = m_lightingManager->getSpotLights();
    for (size_t i = 0; i < lights.size(); ++i) {
        if (lights[i].castsShadow && lights[i].shadowDataIndex != -1) {
            m_shadowSystem->enableSpotLightShadow(static_cast<int>(i), false, *m_lightingManager);
        }
    }
    m_lightingManager->clearSpotLights();
    Logger::getInstance().info("Engine: Wszystkie reflektory zostaly wyczyszczone.");
}


// --- Metody zwiazane z kamera (delegacja do Camera) ---

void Engine::setCameraPosition(const glm::vec3& position) {
    if (m_camera) m_camera->setPosition(position);
}
void Engine::setCameraProjectionType(Camera::ProjectionType type) {
    if (m_camera) m_camera->setProjectionType(type);
}
void Engine::setCameraFOV(float fov) {
    if (m_camera) m_camera->setFOV(fov);
}
void Engine::setCameraClippingPlanes(float nearClip, float farClip) {
    if (m_camera) m_camera->setClippingPlanes(nearClip, farClip);
}

// --- Metody zarzadzania obiektami renderowalnymi/kolidujacymi ---
// UWAGA: Te metody sa oznaczone jako @deprecated w .h.
// Zarzadzanie obiektami powinno odbywac sie w ramach stanow gry.

void Engine::addRenderable(IRenderable* renderable) {
    if (m_renderer) {
        m_renderer->addRenderable(renderable);
    }
    // Jesli obiekt jest rowniez kolidowalny, dodajemy go do systemu kolizji.
    if (m_collisionSystem) {
        ICollidable* collidable = dynamic_cast<ICollidable*>(renderable);
        if (collidable) {
            m_collisionSystem->addCollidable(collidable);
        }
    }
}

void Engine::removeRenderable(IRenderable* renderable) {
    if (m_renderer) {
        m_renderer->removeRenderable(renderable);
    }
    if (m_collisionSystem) {
        ICollidable* collidable = dynamic_cast<ICollidable*>(renderable);
        if (collidable) {
            m_collisionSystem->removeCollidable(collidable);
        }
    }
}

// --- Inne metody konfiguracyjne ---

void Engine::setMouseCapture(bool enabled) {
    if (m_inputManager) m_inputManager->setMouseCapture(enabled);
}
void Engine::setAutoClear(bool enabled) {
    m_autoClear = enabled;
}
void Engine::setAutoSwap(bool enabled) {
    m_autoSwap = enabled;
}

void Engine::clearBuffers() {
    // Czysci bufory koloru i glebokosci przy uzyciu aktualnie ustawionego koloru tla.
    glClearColor(m_backgroundColor[0], m_backgroundColor[1], m_backgroundColor[2], m_backgroundColor[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}