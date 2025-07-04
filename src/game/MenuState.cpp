// Plik: src\game\MenuState.cpp
#define GLM_ENABLE_EXPERIMENTAL // Musi być zdefiniowane PRZED jakimkolwiek include'em GLM
#include "MenuState.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h> // Dla kodów klawiszy GLFW_KEY_*

#include "Engine.h"
#include "InputManager.h"
#include "EventManager.h" // Potrzebny do subskrypcji zdarzeń
#include "Event.h"        // Dla KeyPressedEvent, MouseMovedEvent itp.
#include "Renderer.h"
#include "Camera.h"
#include "Model.h"
#include "Shader.h"
#include "Texture.h" // Dodano dla std::shared_ptr<Texture>
#include "ResourceManager.h"
#include "Logger.h"
#include "TextRenderer.h"     // Do wyświetlania informacji
#include "LightingManager.h"  // Dla ewentualnego podstawowego oświetlenia
#include "Lighting.h"         // Dla struktur DirectionalLight, PointLight, SpotLight

#include "DemoState.h"
//#include "GameplayState.h"

#include <glm/gtx/string_cast.hpp> // Dla glm::to_string do logowania wektorów/kwaternionów
#include <glm/gtx/transform.hpp>   // Dla glm::rotate, glm::translate, glm::scale
#include <iomanip> // Dla std::fixed, std::setprecision
#include <sstream> // Dla std::stringstream

MenuState::MenuState()
    : m_engine(nullptr),
    m_camera(nullptr),
    m_deskModel(nullptr),
    m_clipboardModel(nullptr),
    m_penModel(nullptr),
    m_modelShader(nullptr),
    m_debugMode(false), // Tryb debugowania domyślnie wyłączony
    m_camMoveForward(false),
    m_camMoveBackward(false),
    m_camMoveLeft(false),
    m_camMoveRight(false),
    m_camMoveUp(false),
    m_camMoveDown(false),
    m_mouseCaptured(false), // Mysz domyślnie nieprzechwycona
    m_firstMouse(true),
    m_currentPaperTextureIndex(0) { // Zaczynamy od pierwszej tekstury papieru
    Logger::getInstance().info("Konstruktor MenuState");

    // Inicjalizacja ścieżek do tekstur papieru
    m_paperTexturePaths.push_back("assets/textures/Menu.png");
    m_paperTexturePaths.push_back("assets/textures/Menu2.png");
    m_paperTexturePaths.push_back("assets/textures/Menu3.png");
}

MenuState::~MenuState() {
    Logger::getInstance().info("Destruktor MenuState");
    // cleanup() jest wywoływany przez GameStateManager
}

void MenuState::init() {
    m_engine = Engine::getInstance();
    if (!m_engine) {
        Logger::getInstance().fatal("MenuState::init - Engine instance is null!");
        return;
    }
    Logger::getInstance().info("MenuState: Inicjalizacja...");

    setupCamera();
    loadAssets();
    setupLighting();

    if (m_engine->getEventManager()) {
        m_engine->getEventManager()->subscribe(EventType::KeyPressed, this);
        m_engine->getEventManager()->subscribe(EventType::KeyReleased, this); // Potrzebne dla trybu debug
        m_engine->getEventManager()->subscribe(EventType::MouseMoved, this);  // Potrzebne dla trybu debug
    }

	m_engine->toggleFPSDisplay(); // Wyłączamy wyświetlanie FPS w menu

    // Mysz nie jest przechwytywana na starcie stanu menu
    if (m_engine->getInputManager()) {
        m_engine->getInputManager()->setMouseCapture(false);
        m_mouseCaptured = false;
    }
    m_debugMode = false; // Upewnij się, że debug mode jest wyłączony na starcie
    Logger::getInstance().info("MenuState: Inicjalizacja zakończona. Kamera statyczna. Mysz uwolniona.");
}

void MenuState::setupCamera() {
    m_camera = std::make_unique<Camera>();
    if (m_engine && m_engine->getWindowWidth() > 0 && m_engine->getWindowHeight() > 0) {
        m_camera->setAspectRatio(static_cast<float>(m_engine->getWindowWidth()) / static_cast<float>(m_engine->getWindowHeight()));
    }
    // Ustawienie statycznej pozycji i orientacji kamery
    m_camera->setPosition(glm::vec3(0.35f, 1.08f, -0.05f));
    m_camera->setYaw(-181.28f);
    m_camera->setPitch(-73.40f);
    m_camera->setMovementSpeed(2.0f); // Prędkość pozostaje na wypadek włączenia trybu debug
    m_camera->setMouseSensitivity(0.08f); // Czułość myszy również

    Logger::getInstance().info("MenuState: Kamera skonfigurowana (statyczna).");


}

void MenuState::loadAssets() {
    ResourceManager& resManager = ResourceManager::getInstance();

    m_modelShader = resManager.getShader("lightingShader");
    if (!m_modelShader) {
        Logger::getInstance().warning("MenuState: Nie udało się pobrać shadera 'lightingShader'. Próba załadowania 'default_shader'.");
        m_modelShader = resManager.loadShader("lightingShader", "assets/shaders/default_shader.vert", "assets/shaders/default_shader.frag");
    }

    if (!m_modelShader) {
        Logger::getInstance().error("MenuState: Nie udało się załadować shadera dla modeli! Modele nie będą renderowane poprawnie.");
    }
    else {
        // Logger::getInstance().info("MenuState: Shader dla modeli: " + m_modelShader->getName());
    }

    std::shared_ptr<ModelAsset> deskAsset = resManager.loadModel("deskModel", "assets/models/desk.obj");
    if (deskAsset && m_modelShader) {
        m_deskModel = std::make_shared<Model>("Desk", deskAsset, m_modelShader);
        m_deskModel->setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        m_deskModel->setScale(glm::vec3(1.0f));
        if (m_camera) {
            m_deskModel->setCameraForLighting(m_camera.get());
        }
        // Logger::getInstance().info("MenuState: Model biurka załadowany.");
    }
    else {
        if (!deskAsset) Logger::getInstance().error("MenuState: Nie udało się załadować zasobu modelu biurka (desk.obj).");
        if (deskAsset && !m_modelShader) Logger::getInstance().error("MenuState: Zasób modelu biurka załadowany, ale shader nie jest dostępny.");
    }

    std::shared_ptr<ModelAsset> clipboardAsset = resManager.loadModel("clipboardModel", "assets/models/objClipboard.obj");
    m_clipboardPaperMeshIndices.clear();

    if (clipboardAsset && m_modelShader) {
        m_clipboardModel = std::make_shared<Model>("Clipboard", clipboardAsset, m_modelShader);
        m_clipboardModel->setPosition(glm::vec3(0.36f, 0.785f, -0.08f));
        m_clipboardModel->setScale(glm::vec3(0.02f));
        glm::quat rotationZ = glm::angleAxis(glm::radians(65.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        m_clipboardModel->setRotation(rotationZ);
        if (m_camera) {
            m_clipboardModel->setCameraForLighting(m_camera.get());
        }
        Logger::getInstance().info("MenuState: Model clipboardu załadowany.");

        // Zidentyfikuj siatki clipboardu używające oryginalnej tekstury "paper.png" (lub jej odpowiednika w modelu)
        const auto& meshRenderers = m_clipboardModel->getMeshRenderers();
        for (size_t i = 0; i < meshRenderers.size(); ++i) {
            const auto& materialProps = meshRenderers[i].materialProperties;
            if (materialProps.diffuseTexture && materialProps.diffuseTexture->ID != 0) {
                std::string texturePathLower = materialProps.diffuseTexture->path;
                std::transform(texturePathLower.begin(), texturePathLower.end(), texturePathLower.begin(),
                    [](unsigned char c) { return std::tolower(c); });

                // Ważne: Ta logika identyfikuje siatkę na podstawie tekstury, którą miała *pierwotnie*
                // załadowaną z pliku modelu. Jeśli plik MTL clipboardu nadal odnosi się do "paper.png"
                // dla tej części, to poniższy warunek jest poprawny.
                // Jeśli plik MTL został zmieniony, aby odnosić się np. do "menu.png" jako domyślnej,
                // wtedy poniższy string do wyszukania również powinien zostać zmieniony na "menu.png".
                // Zakładam, że identyfikacja części modelu (papieru) nadal opiera się na "paper.png" w jego oryginalnych danych.
                if (texturePathLower.find("menu.png") != std::string::npos) {
                    m_clipboardPaperMeshIndices.push_back(i);
                    Logger::getInstance().info("MenuState: Siatka clipboardu " + std::to_string(i) + " zidentyfikowana do zmiany tekstury (oryginalnie: " + materialProps.diffuseTexture->path + ")");
                }
            }
        }
        if (m_clipboardPaperMeshIndices.empty()) {
            Logger::getInstance().warning("MenuState: Nie znaleziono siatek w modelu clipboardu z oryginalną teksturą 'paper.png'. Zmiana tekstur może nie działać zgodnie z oczekiwaniami.");
        }

        // Ładowanie nowych tekstur "Menu.png", "Menu2.png", "Menu3.png"
        m_paperTextures.clear();
        for (const std::string& path : m_paperTexturePaths) {
            std::string textureKey = "menuState_menuPage_" + path; // Klucz może być bardziej opisowy
            std::shared_ptr<Texture> tex = resManager.loadTexture(textureKey, path, "diffuse_menu_page");
            if (tex) {
                m_paperTextures.push_back(tex);
            }
            else {
                Logger::getInstance().error("MenuState: Nie udało się załadować tekstury menu: " + path);
            }
        }

        if (!m_paperTextures.empty() && !m_clipboardPaperMeshIndices.empty()) {
            applyCurrentPaperTexture();
        }
        else {
            if (m_paperTextures.empty()) Logger::getInstance().error("MenuState: Nie załadowano żadnych tekstur MenuN.png dla clipboardu.");
        }

    }
    else {
        if (!clipboardAsset) Logger::getInstance().error("MenuState: Nie udało się załadować zasobu modelu clipboardu (objClipboard.obj).");
        if (clipboardAsset && !m_modelShader) Logger::getInstance().error("MenuState: Zasób modelu clipboardu załadowany, ale shader nie jest dostępny.");
    }

    // Ładowanie modelu długopisu (pen.obj)
    std::shared_ptr<ModelAsset> penAsset = resManager.loadModel("penModelAsset", "assets/models/pen.obj"); // Użyj unikalnej nazwy dla zasobu
    if (penAsset && m_modelShader) {
        m_penModel = std::make_shared<Model>("Pen", penAsset, m_modelShader);

        // Ustawienie początkowej pozycji, rotacji i skali dla długopisu
        // Te wartości są przykładowe i mogą wymagać precyzyjnego dostosowania,
        // aby długopis leżał naturalnie na biurku lub obok clipboardu.
        m_penModel->setPosition(glm::vec3(0.2f, 0.785f, -0.08f)); // Przykładowa pozycja na biurku
        m_penModel->setScale(glm::vec3(0.008f)); // Skala może wymagać znacznego dostosowania

        // Przykładowa rotacja, aby długopis leżał np. pod kątem
        glm::quat penRotationY = glm::angleAxis(glm::radians(-30.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Obrót wokół osi Y
        glm::quat penRotationX = glm::angleAxis(glm::radians(5.0f), glm::vec3(1.0f, 0.0f, 0.0f));   // Lekkie pochylenie
        m_penModel->setRotation(penRotationY * penRotationX); // Połączenie rotacji

        if (m_camera) { // Ustawienie kamery dla oświetlenia, jeśli istnieje
            m_penModel->setCameraForLighting(m_camera.get());
        }
        Logger::getInstance().info("MenuState: Model długopisu (pen.obj) załadowany.");
    }
    else {
        if (!penAsset) Logger::getInstance().error("MenuState: Nie udało się załadować zasobu modelu długopisu (pen.obj).");
        if (penAsset && !m_modelShader) Logger::getInstance().error("MenuState: Zasób modelu długopisu załadowany, ale shader nie jest dostępny.");
    }

}

void MenuState::applyCurrentPaperTexture() {
    if (!m_clipboardModel || m_paperTextures.empty() || m_clipboardPaperMeshIndices.empty() || m_currentPaperTextureIndex < 0 || static_cast<size_t>(m_currentPaperTextureIndex) >= m_paperTextures.size()) {
        // Logger::getInstance().warning("MenuState::applyCurrentPaperTexture - Nie można zastosować tekstury: model, tekstury, indeksy siatek lub indeks tekstury nieprawidłowe.");
        return;
    }

    std::shared_ptr<Texture> textureToApply = m_paperTextures[m_currentPaperTextureIndex];
    if (textureToApply) {
        // Pobierz referencję do wektora rendererów siatek (aby uniknąć kopiowania)
        auto& meshRenderers = m_clipboardModel->getMeshRenderers();
        for (size_t meshIndex : m_clipboardPaperMeshIndices) {
            if (meshIndex < meshRenderers.size()) { // Upewnij się, że indeks jest w granicach
                meshRenderers[meshIndex].materialProperties.diffuseTexture = textureToApply;
            }
        }
        Logger::getInstance().info("MenuState: Zastosowano teksturę papieru: " + m_paperTexturePaths[m_currentPaperTextureIndex] + " do " + std::to_string(m_clipboardPaperMeshIndices.size()) + " siatek.");
    }
    else {
        Logger::getInstance().warning("MenuState: Próba zastosowania niezaładowanej tekstury papieru o indeksie: " + std::to_string(m_currentPaperTextureIndex));
    }
}

void MenuState::cyclePaperTexture(bool cycleUp) {
    if (m_paperTextures.empty()) return;

    if (cycleUp) { // W, ArrowUp
        m_currentPaperTextureIndex--;
        if (m_currentPaperTextureIndex < 0) {
            m_currentPaperTextureIndex = static_cast<int>(m_paperTextures.size()) - 1;
        }
    }
    else { // S, ArrowDown
        m_currentPaperTextureIndex++;
        if (static_cast<size_t>(m_currentPaperTextureIndex) >= m_paperTextures.size()) {
            m_currentPaperTextureIndex = 0;
        }
    }
    applyCurrentPaperTexture();
}

void MenuState::setupLighting() {
    if (!m_engine || !m_engine->getLightingManager()) {
        Logger::getInstance().error("MenuState::setupLighting - Brak silnika lub LightingManagera.");
        return;
    }

    LightingManager* lightManager = m_engine->getLightingManager();

    // 1. Wyłączanie światła kierunkowego
    DirectionalLight& dirLight = lightManager->getDirectionalLight();
    dirLight.enabled = false;
    Logger::getInstance().info("MenuState: Światło kierunkowe wyłączone.");

    // 2. Czyszczenie istniejących świateł punktowych i reflektorów
    lightManager->clearPointLights();
    lightManager->clearSpotLights();
    Logger::getInstance().info("MenuState: Istniejące światła punktowe i reflektory wyczyszczone.");

    // 3. Dodawanie nowego światła punktowego

    PointLight pointLight1;
    pointLight1.enabled = true;

    pointLight1.position = glm::vec3(-0.5f, 1.5f, 0.75f);

    // Właściwości światła 
    pointLight1.ambient = glm::vec3(0.08f, 0.08f, 0.1f);
    pointLight1.diffuse = glm::vec3(0.9f, 0.85f, 0.75f);
    pointLight1.specular = glm::vec3(0.7f, 0.7f, 0.7f);

    pointLight1.constant = 1.0f;
    pointLight1.linear = 0.18f;
    pointLight1.quadratic = 0.10f;

    pointLight1.castsShadow = false;

    lightManager->addPointLight(pointLight1);
    Logger::getInstance().info("MenuState: Dodano światło punktowe o stałej pozycji: (" +
        std::to_string(pointLight1.position.x) + ", " +
        std::to_string(pointLight1.position.y) + ", " +
        std::to_string(pointLight1.position.z) + ")");
}

void MenuState::cleanup() {
    Logger::getInstance().info("MenuState: Sprzątanie...");
    if (m_engine && m_engine->getEventManager()) {
        m_engine->getEventManager()->unsubscribe(EventType::KeyPressed, this);
        m_engine->getEventManager()->unsubscribe(EventType::KeyReleased, this);
        m_engine->getEventManager()->unsubscribe(EventType::MouseMoved, this);
    }
    if (m_mouseCaptured && m_engine && m_engine->getInputManager()) {
        m_engine->getInputManager()->setMouseCapture(false);
        m_mouseCaptured = false;
    }
    m_deskModel.reset();
    m_clipboardModel.reset();
    m_modelShader.reset();
    m_paperTextures.clear();
    m_camera.reset();
    m_penModel.reset();
    Logger::getInstance().info("MenuState: Sprzątanie zakończone.");
}

void MenuState::pause() {
    Logger::getInstance().info("MenuState: Pauza.");
    if (m_mouseCaptured && m_engine && m_engine->getInputManager()) { // Jeśli mysz była przechwycona w trybie debug
        m_engine->getInputManager()->setMouseCapture(false);
        // Nie zmieniamy m_mouseCaptured, aby resume() wiedziało, czy przywrócić
    }
}

void MenuState::resume() {
    Logger::getInstance().info("MenuState: Wznowienie.");
    if (m_engine && m_engine->getInputManager()) {
        // Mysz jest domyślnie nieprzechwycona po wznowieniu, chyba że tryb debug jest aktywny
        if (m_debugMode) {
            m_engine->getInputManager()->setMouseCapture(true);
            m_mouseCaptured = true; // Upewnij się, że flaga jest zgodna
        }
        else {
            m_engine->getInputManager()->setMouseCapture(false);
            m_mouseCaptured = false;
        }
        m_firstMouse = true;
        if (m_mouseCaptured) { // Jeśli mysz jest przechwycona (co oznacza tryb debug)
            m_lastMousePos = m_engine->getInputManager()->getMousePosition();
        }
    }
}

void MenuState::handleEvents(InputManager* inputManager, EventManager* eventManager) {
    // Ta metoda jest teraz mniej istotna, ponieważ większość logiki jest w onEvent.
}

void MenuState::onEvent(Event& event) {
    if (!m_engine) return; // Dodatkowe zabezpieczenie

    if (event.getEventType() == EventType::KeyPressed) {
        KeyPressedEvent& e = static_cast<KeyPressedEvent&>(event);
        int key = e.getKeyCode();

        if (key == GLFW_KEY_F11) {
            m_debugMode = !m_debugMode;
            Logger::getInstance().info("MenuState: Tryb debugowania przełączony na: " + std::string(m_debugMode ? "WŁ." : "WYŁ."));
            if (m_engine->getInputManager()) {
                m_engine->getInputManager()->setMouseCapture(m_debugMode);
                m_mouseCaptured = m_debugMode;
                m_firstMouse = true;
                if (m_mouseCaptured) {
                    m_lastMousePos = m_engine->getInputManager()->getMousePosition();
                }
            }
            if (!m_debugMode) {
                m_camMoveForward = m_camMoveBackward = m_camMoveLeft = m_camMoveRight = m_camMoveUp = m_camMoveDown = false;
            }
            event.handled = true;
            return;
        }

        // Logika dla klawisza ENTER - akcje menu
        if (key == GLFW_KEY_ENTER) {
            if (!m_debugMode) { // Akcje menu działają tylko gdy NIE jesteśmy w trybie debug
                switch (m_currentPaperTextureIndex) {
                case 0: // Odpowiada Menu.png (pierwsza tekstura)
                    Logger::getInstance().info("MenuState: ENTER - Wybrano opcję 1 (Uruchom DemoState).");
                    if (m_engine->getGameStateManager()) {
                        m_engine->getGameStateManager()->changeState(std::make_unique<DemoState>());
                    }
                    break;
                case 1: // Odpowiada Menu2.png (druga tekstura)
                    Logger::getInstance().info("MenuState: ENTER - Wybrano opcję 2 (Uruchom GameplayState - NIEZAIMPLEMENTOWANE).");
                    // Tutaj byłaby logika zmiany stanu na GameplayState, gdyby istniał:
                    // if (m_engine->getGameStateManager()) {
                    //     // #include "GameplayState.h" // Gdyby istniał
                    //     // m_engine->getGameStateManager()->changeState(std::make_unique<GameplayState>());
                    // }
                    break;
                case 2: // Odpowiada Menu3.png (trzecia tekstura)
                    Logger::getInstance().info("MenuState: ENTER - Wybrano opcję 3 (Wyjdź z gry).");
                    { // Użycie bloku dla zmiennej lokalnej
                        WindowCloseEvent closeEvent;
                        if (m_engine->getEventManager()) {
                            m_engine->getEventManager()->dispatch(closeEvent);
                        }
                        else {
                            m_engine->shutdown(); // Awaryjne zamknięcie
                        }
                    }
                    break;
                default:
                    Logger::getInstance().warning("MenuState: ENTER naciśnięty, ale currentPaperTextureIndex jest nieprawidłowy: " + std::to_string(m_currentPaperTextureIndex));
                    break;
                }
            }
            else {
                Logger::getInstance().info("MenuState: ENTER naciśnięty w trybie debugowania - brak akcji menu.");
            }
            event.handled = true; // Klawisz ENTER został obsłużony
            return; // Zakończ obsługę tego zdarzenia klawisza
        }


        if (m_debugMode) {
            // Sterowanie kamerą tylko w trybie debug
            if (key == GLFW_KEY_W) m_camMoveForward = true;
            else if (key == GLFW_KEY_S) m_camMoveBackward = true;
            else if (key == GLFW_KEY_A) m_camMoveLeft = true;
            else if (key == GLFW_KEY_D) m_camMoveRight = true;
            else if (key == GLFW_KEY_SPACE) m_camMoveUp = true;
            else if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) m_camMoveDown = true;

        }
        else {
            // Zmiana tekstur papieru tylko gdy NIE jesteśmy w trybie debug
            if (key == GLFW_KEY_W || key == GLFW_KEY_UP) {
                cyclePaperTexture(true);
            }
            else if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN) {
                cyclePaperTexture(false);
            }
        }

        // Logowanie transformacji kamery (F1) - zawsze dostępne
        if (key == GLFW_KEY_F1) {
            logCameraTransform();
        }
        // Wyjście z menu (ESC) - zawsze dostępne
        else if (key == GLFW_KEY_ESCAPE) {
            Logger::getInstance().info("MenuState: Wciśnięto ESC.");
            if (m_engine->getGameStateManager()) {
                // Zdjęcie MenuState ze stosu. Jeśli jest to ostatni stan, silnik obsłuży zamknięcie.
                m_engine->getGameStateManager()->popState();
            }
            else {
                m_engine->shutdown(); // Awaryjne zamknięcie, jeśli brak GameStateManagera
            }
        }
        // event.handled = true; // Można rozważyć, czy inne akcje klawiszy mają być blokowane
                              // Jeśli powyższe if/else if nie złapały klawisza,
                              // to handled pozostanie false, co jest OK.
                              // Jeśli któryś z powyższych warunków (F11, ENTER, ruch kamerą, zmiana kartki, F1, ESC)
                              // został spełniony, event.handled powinno być już true.
    } // Koniec obsługi KeyPressedEvent
    else if (event.getEventType() == EventType::KeyReleased) {
        KeyReleasedEvent& e = static_cast<KeyReleasedEvent&>(event);
        int key = e.getKeyCode();
        if (m_debugMode) {
            if (key == GLFW_KEY_W) m_camMoveForward = false;
            else if (key == GLFW_KEY_S) m_camMoveBackward = false;
            else if (key == GLFW_KEY_A) m_camMoveLeft = false;
            else if (key == GLFW_KEY_D) m_camMoveRight = false;
            else if (key == GLFW_KEY_SPACE) m_camMoveUp = false;
            else if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) m_camMoveDown = false;
        }
        event.handled = true;
    } // Koniec obsługi KeyReleasedEvent
    else if (event.getEventType() == EventType::MouseMoved) {
        if (m_debugMode && m_mouseCaptured && m_camera) {
            MouseMovedEvent& e = static_cast<MouseMovedEvent&>(event);
            glm::vec2 currentPos = e.getPosition();
            if (m_firstMouse) {
                m_lastMousePos = currentPos;
                m_firstMouse = false;
            }
            float xoffset = currentPos.x - m_lastMousePos.x;
            float yoffset = m_lastMousePos.y - currentPos.y;
            m_lastMousePos = currentPos;
            m_camera->processMouseMovement(xoffset, yoffset);
        }
        event.handled = true;
    } // Koniec obsługi MouseMovedEvent
}

void MenuState::update(float deltaTime) {
    if (!m_camera) return;

    if (m_debugMode) { // Aktualizuj ruch kamery tylko w trybie debug
        float camSpeed = m_camera->getMovementSpeed() * deltaTime;
        if (m_camMoveForward) m_camera->processKeyboard(Camera::Direction::Forward, camSpeed);
        if (m_camMoveBackward) m_camera->processKeyboard(Camera::Direction::Backward, camSpeed);
        if (m_camMoveLeft) m_camera->processKeyboard(Camera::Direction::Left, camSpeed);
        if (m_camMoveRight) m_camera->processKeyboard(Camera::Direction::Right, camSpeed);
        if (m_camMoveUp) m_camera->processKeyboard(Camera::Direction::Up, camSpeed);
        if (m_camMoveDown) m_camera->processKeyboard(Camera::Direction::Down, camSpeed);
    }
}

void MenuState::render(Renderer* renderer) {
    if (!m_engine || !renderer || !m_camera) {
        Logger::getInstance().error("MenuState::render - Brak silnika, renderera lub kamery.");
        return;
    }

    // Ustawienie kamery w rendererze silnika (Engine::m_camera jest główną kamerą dla cyklu renderowania Engine)
    // MenuState używa własnej instancji kamery m_camera, więc to ją przekazujemy
    // Engine powinien w swojej metodzie render() używać kamery ustawionej dla aktywnego stanu lub swojej domyślnej.
    // W tym przypadku, jeśli chcemy, aby *ten* stan kontrolował kamerę dla Engine, musielibyśmy ją ustawić:
    // m_engine->setCamera(m_camera.get()); // Uwaga: to nadpisze główną kamerę silnika, co może nie być pożądane jeśli Engine jej używa inaczej.
    // Bezpieczniej jest, aby renderer przekazany tutaj używał kamery tego stanu.

    glm::mat4 projectionMatrix = m_camera->getProjectionMatrix();
    glm::mat4 viewMatrix = m_camera->getViewMatrix();

    LightingManager* lightManager = m_engine->getLightingManager();
    if (lightManager && m_modelShader) {
        m_modelShader->use();
        // Przekaż pozycję kamery MenuState do shadera, a nie kamery silnika (jeśli są różne)
        m_modelShader->setVec3("viewPos_World", m_camera->getPosition());
        lightManager->uploadGeneralLightUniforms(m_modelShader);
        // ShadowSystem uniforms should be set up by Engine if shadows are active globally
        if (m_engine->getShadowSystem()) {
            m_engine->getShadowSystem()->uploadShadowUniforms(m_modelShader, *lightManager);
        }
    }

    if (m_deskModel && m_modelShader) {
        m_deskModel->render(viewMatrix, projectionMatrix);
    }

    if (m_clipboardModel && m_modelShader) {
        m_clipboardModel->render(viewMatrix, projectionMatrix);
    }

    if (m_penModel && m_modelShader) {
        m_penModel->render(viewMatrix, projectionMatrix);
    }
}

void MenuState::logCameraTransform() {
    if (!m_camera) {
        Logger::getInstance().warning("MenuState: Próba logowania transformacji kamery, ale kamera jest null.");
        return;
    }
    glm::vec3 pos = m_camera->getPosition();
    glm::vec3 front = m_camera->getFront();
    float pitch = m_camera->getPitch();
    float yaw = m_camera->getYaw();

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "Camera Info: Pos(" << pos.x << ", " << pos.y << ", " << pos.z << "), ";
    ss << "Front(" << front.x << ", " << front.y << ", " << front.z << "), ";
    ss << "Pitch: " << pitch << " deg, Yaw: " << yaw << " deg.";

    Logger::getInstance().info(ss.str());
}