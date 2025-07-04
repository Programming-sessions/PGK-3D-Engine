#define GLM_ENABLE_EXPERIMENTAL
#include "DemoState.h"

#include <glad/glad.h> // Dla funkcji OpenGL, jeśli są używane bezpośrednio

#include "Engine.h"
#include "InputManager.h"
#include "EventManager.h"
#include "Event.h"
#include "Renderer.h"
#include "Camera.h"
#include "LightingManager.h"
#include "TextRenderer.h"
#include "ResourceManager.h" // Dla ładowania zasobów (tekstur, modeli, shaderów)
#include "Shader.h"
#include "Texture.h"
#include "Model.h"
#include "Primitives.h"      // Dla definicji Cube, Plane, Sphere itp.
#include "Logger.h"
#include "MenuState.h"       // Do powrotu do menu

#include <glm/gtx/transform.hpp> // Dla glm::rotate, glm::translate, glm::scale
#include <glm/gtc/constants.hpp> // Dla glm::pi
#include <algorithm>             // Dla std::min, std::max
#include <vector>
#include <string>
#include <iomanip>               // Dla std::fixed, std::setprecision

namespace { // Anonimowa przestrzeń nazw dla funkcji pomocniczych specyficznych dla tego pliku
    // Funkcja pomocnicza do konwersji typu wyboru na string
    std::string selectionTypeToString(ActiveSelectionType type) {
        switch (type) {
        case ActiveSelectionType::NONE: return "None";
        case ActiveSelectionType::PRIMITIVE: return "Prymityw";
        case ActiveSelectionType::MODEL: return "Model";
        case ActiveSelectionType::POINT_LIGHT: return "Swiatlo Pkt.";
        case ActiveSelectionType::SPOT_LIGHT: return "Reflektor";
        case ActiveSelectionType::DIR_LIGHT: return "Swiatlo Kier.";
        default: return "Unknown";
        }
    }
} // koniec anonimowej przestrzeni nazw

DemoState::DemoState()
    : m_camera(nullptr), m_lightingManager(nullptr), m_textRenderer(nullptr),
    m_currentSelection(ActiveSelectionType::PRIMITIVE), // Domyślnie wybieramy prymitywy
    m_activePrimitiveIndex(0),      // Domyślnie pierwszy prymityw
    m_activeModelIndex(-1),         // Domyślnie żaden model nie jest wybrany
    m_activePointLightIndex(-1),    // Domyślnie żadne światło punktowe
    m_activeSpotLightIndex(-1),     // Domyślnie żaden reflektor
    m_spotLight0_shadow_enabled(true), // Domyślne wartości cieni
    m_pointLight0_shadow_enabled(true),
    m_camMoveForward(false), m_camMoveBackward(false), m_camMoveLeft(false), m_camMoveRight(false),
    m_objMoveZ_neg(false), m_objMoveZ_pos(false), m_objMoveX_neg(false), m_objMoveX_pos(false), m_objMoveY_pos(false), m_objMoveY_neg(false),
    m_objRotateX_pos(false), m_objRotateX_neg(false), m_objRotateY_pos(false), m_objRotateY_neg(false), m_objRotateZ_pos(false), m_objRotateZ_neg(false),
    m_objScaleUp(false), m_objScaleDown(false),
    m_mouseCaptured(false),     // Mysz domyślnie nieprzechwycona przy wejściu do stanu (ustawiane w resume/init)
    m_triggerExitToMenu(false), // Domyślnie nie chcemy wychodzić do menu
    m_firstMouse(true),         // Dla inicjalizacji pozycji myszy
    m_moveSpeed(5.0f),          // Prędkość ruchu obiektów/kamery
    m_rotationSpeed(70.0f),     // Prędkość rotacji obiektów
    m_scaleSpeed(0.8f)          // Prędkość skalowania obiektów
{
    Logger::getInstance().info("DemoState constructor");
}

DemoState::~DemoState() {
    Logger::getInstance().info("DemoState destructor");
    // cleanup() jest wywoływany przez GameStateManager
    // Dodatkowe odsubskrybowanie zdarzeń, jeśli subskrypcje są zarządzane tutaj
    if (IGameState::m_engine && IGameState::m_engine->getEventManager()) {
        IGameState::m_engine->getEventManager()->unsubscribe(EventType::KeyPressed, this);
        IGameState::m_engine->getEventManager()->unsubscribe(EventType::KeyReleased, this);
        IGameState::m_engine->getEventManager()->unsubscribe(EventType::MouseMoved, this);
        IGameState::m_engine->getEventManager()->unsubscribe(EventType::MouseScrolled, this);
    }
}

void DemoState::init() {
	IGameState::m_engine = Engine::getInstance();
    Logger::getInstance().info("DemoState init");

    // Pobierz wskaźniki do kluczowych systemów silnika
    m_camera = IGameState::m_engine->getCamera();
    m_lightingManager = IGameState::m_engine->getLightingManager();
    m_textRenderer = TextRenderer::getInstance(); // TextRenderer jest singletonem
    ResourceManager& resManager = ResourceManager::getInstance(); // ResourceManager jest singletonem

    // Sprawdzenie, czy systemy zostały poprawnie pobrane
    if (!m_camera) Logger::getInstance().error("DemoState: Camera is null!");
    if (!m_lightingManager) Logger::getInstance().error("DemoState: LightingManager is null! Oświetlenie nie będzie działać.");
    if (!m_textRenderer || !m_textRenderer->isInitialized()) Logger::getInstance().warning("DemoState: TextRenderer not usable.");

    // Subskrypcja na zdarzenia wejściowe
    if (IGameState::m_engine && IGameState::m_engine->getEventManager()) {
        Logger::getInstance().info("DemoState subscribing to events: KeyPressed, KeyReleased, MouseMoved, MouseScrolled");
        IGameState::m_engine->getEventManager()->subscribe(EventType::KeyPressed, this);
        IGameState::m_engine->getEventManager()->subscribe(EventType::KeyReleased, this);
        IGameState::m_engine->getEventManager()->subscribe(EventType::MouseMoved, this);
        IGameState::m_engine->getEventManager()->subscribe(EventType::MouseScrolled, this);
    }
    else {
        Logger::getInstance().error("DemoState: Cannot subscribe to events, EventManager is null.");
    }

    // --- Ładowanie i konfiguracja obiektów sceny (prymitywy, modele) ---
    // Kopiujemy logikę tworzenia sceny z oryginalnego GameplayState (z main.cpp)

   // --- Load Primitives ---
    auto ground = std::make_unique<Plane>(glm::vec3(0.0f, -1.0f, 0.0f), 20.0f, 20.0f, glm::vec4(0.4f, 0.6f, 0.4f, 1.0f));
    ground->setMaterialShininess(16.0f);
    ground->setCastsShadow(false); // Podłoga nie musi rzucać cienia
    ground->setCollisionsEnabled(false); // Wyłączamy kolizje dla podłogi, jeśli nie są potrzebne do testów
    m_scenePrimitives.push_back(std::move(ground));

    auto cube = std::make_unique<Cube>(glm::vec3(-1.5f, 0.0f, -3.0f), 1.5f, glm::vec4(0.8f, 0.3f, 0.3f, 1.0f));
    cube->setMaterialShininess(32.0f);
    std::shared_ptr<Texture> woodDiffuseTexture = resManager.loadTexture("woodCrateDiffuse", "assets/textures/Wood.jpg", "diffuse");
    if (woodDiffuseTexture) cube->setDiffuseTexture(woodDiffuseTexture);
    std::shared_ptr<Texture> woodSpecularTexture = resManager.loadTexture("woodCrateSpecular", "assets/textures/specularWood.jpg", "specular");
    if (woodSpecularTexture) cube->setSpecularTexture(woodSpecularTexture);
    cube->setCastsShadow(true);
    cube->setCollisionsEnabled(true);
    m_scenePrimitives.push_back(std::move(cube));

    auto sphere = std::make_unique<Sphere>(glm::vec3(1.5f, 0.25f, -2.0f), 1.0f, 36, 18, glm::vec4(0.3f, 0.3f, 0.8f, 1.0f));
    sphere->setMaterialShininess(64.0f);
    sphere->setCollisionsEnabled(true);
    m_scenePrimitives.push_back(std::move(sphere));

    auto cylinder = std::make_unique<Cylinder>(glm::vec3(4.0f, 0.0f, -1.0f), 0.75f, 2.0f, 32, glm::vec4(0.8f, 0.8f, 0.2f, 1.0f));
    cylinder->setMaterialShininess(128.0f);
    cylinder->setCastsShadow(true);
    cylinder->setCollisionsEnabled(true);
    m_scenePrimitives.push_back(std::move(cylinder));

    auto pyramid = std::make_unique<SquarePyramid>(glm::vec3(-4.0f, -1.0f, -1.0f), 1.5f, 2.0f, glm::vec4(0.9f, 0.5f, 0.2f, 1.0f));
    pyramid->setMaterialShininess(64.0f);
    pyramid->setCastsShadow(true);
    pyramid->setCollisionsEnabled(true);
    m_scenePrimitives.push_back(std::move(pyramid));

    auto cone = std::make_unique<Cone>(glm::vec3(0.0f, -1.0f, 2.0f), 1.0f, 2.5f, 32, glm::vec4(0.2f, 0.5f, 0.9f, 1.0f));
    cone->setMaterialShininess(256.0f);
    cone->setCastsShadow(true);
    cone->setCollisionsEnabled(true);
    m_scenePrimitives.push_back(std::move(cone));




    // --- Load Models ---
    std::shared_ptr<Shader> modelShader = resManager.getShader("lightingShader"); // Zakładamy, że ten shader jest już załadowany
    if (!modelShader) { // Jeśli nie, załaduj go (może to być domyślny shader z zasobów)
        modelShader = resManager.loadShader("standardLighting", "assets/shaders/default_shader.vert", "assets/shaders/default_shader.frag");
    }

    if (!modelShader) {
        Logger::getInstance().error("DemoState: Failed to load or get shader for models!");
    }
    else {
        Logger::getInstance().info("DemoState: Using shader '" + modelShader->getName() + "' for models.");
    }

    std::shared_ptr<ModelAsset> cheeseAsset = resManager.loadModel("cheeseModel", "assets/models/Cheese.obj");
    if (cheeseAsset && modelShader) {
        auto cheeseModel = std::make_unique<Model>("Cheese", cheeseAsset, modelShader);
        cheeseModel->setPosition(glm::vec3(0.0f, -0.5f, -1.0f));
        cheeseModel->setScale(glm::vec3(0.3f));
        if (m_camera) cheeseModel->setCameraForLighting(m_camera); // Przekaż kamerę do modelu dla oświetlenia
        m_sceneModels.push_back(std::move(cheeseModel));
        Logger::getInstance().info("Cheese model loaded and added to scene.");
    }
    else {
        if (!cheeseAsset) Logger::getInstance().error("DemoState: Failed to load cheese model asset!");
        if (!modelShader && cheeseAsset) Logger::getInstance().error("DemoState: Cheese model asset loaded, but no shader available for it!");
    }

    // Ustawienie początkowego wyboru
    if (!m_scenePrimitives.empty()) {
        m_currentSelection = ActiveSelectionType::PRIMITIVE;
        m_activePrimitiveIndex = 0;
    }
    else if (!m_sceneModels.empty()) {
        m_currentSelection = ActiveSelectionType::MODEL;
        m_activeModelIndex = 0;
    }
    else {
        m_currentSelection = ActiveSelectionType::NONE;
        m_activePrimitiveIndex = -1;
        m_activeModelIndex = -1;
    }

    // Dodanie obiektów do renderera i systemu kolizji silnika
    if (m_engine) {
        for (const auto& prim : m_scenePrimitives) {
            m_engine->addRenderable(prim.get()); // Engine zarządza listą renderowalnych
        }
        for (const auto& model : m_sceneModels) {
            m_engine->addRenderable(model.get());
        }
    }


    // --- Konfiguracja oświetlenia ---
    if (m_lightingManager) {
        m_lightingManager->clearPointLights();
        m_lightingManager->clearSpotLights();

        // Światło punktowe 1
        PointLight pLight1;
        pLight1.position = glm::vec3(2.0f, 2.0f, 1.0f);
        pLight1.diffuse = glm::vec3(1.0f, 0.7f, 0.3f);
        pLight1.ambient = pLight1.diffuse * 0.1f;
        pLight1.specular = pLight1.diffuse;
        pLight1.linear = 0.07f;
        pLight1.quadratic = 0.017f;
        pLight1.castsShadow = m_pointLight0_shadow_enabled; // Użyj flagi
        pLight1.shadowNearPlane = 0.1f;
        pLight1.shadowFarPlane = 25.0f;
        m_lightingManager->addPointLight(pLight1);
        if (m_engine) m_engine->enablePointLightShadow(0, pLight1.castsShadow); // Włącz cienie dla tego światła przez silnik


        // Reflektor 1
        SpotLight sLight1;
        sLight1.position = glm::vec3(-2.0f, 3.0f, 1.0f);
        sLight1.direction = glm::normalize(glm::vec3(0.5f, -1.0f, -0.3f));
        sLight1.diffuse = glm::vec3(0.5f, 0.8f, 1.0f);
        sLight1.ambient = sLight1.diffuse * 0.05f;
        sLight1.specular = sLight1.diffuse;
        sLight1.cutOff = glm::cos(glm::radians(15.5f));
        sLight1.outerCutOff = glm::cos(glm::radians(22.5f));
        sLight1.enabled = true;
        sLight1.castsShadow = m_spotLight0_shadow_enabled; // Użyj flagi
        m_lightingManager->addSpotLight(sLight1);
        if (m_engine) m_engine->enableSpotLightShadow(0, sLight1.castsShadow);

        // Światło kierunkowe (jest już domyślnie w LightingManager, można je tu dostosować)
        DirectionalLight& dirLight = m_lightingManager->getDirectionalLight();
        dirLight.enabled = true;
        dirLight.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
        // dirLight.castsShadow = true; // Jeśli światło kierunkowe ma rzucać cienie (potrzebny ShadowMapper w ShadowSystem)
    }

    // --- Konfiguracja kamery ---
    if (m_camera) {
        m_camera->setPosition(glm::vec3(0.0f, 1.5f, 6.0f));
        m_camera->setYaw(-90.0f);   // Patrzenie wzdłuż -Z
        m_camera->setPitch(-10.0f); // Lekkie spojrzenie w dół
    }

    // --- Stan myszy ---
    m_mouseCaptured = IGameState::m_engine->getInputManager() ? IGameState::m_engine->getInputManager()->isMouseCaptured() : false;
    if (!m_mouseCaptured && IGameState::m_engine->getInputManager()) {
        IGameState::m_engine->getInputManager()->setMouseCapture(true); // Przechwyć mysz przy wejściu
        m_mouseCaptured = true;
    }
    m_firstMouse = true; // Resetuj flagę pierwszej aktualizacji myszy
    if (m_mouseCaptured && IGameState::m_engine->getInputManager()) {
        m_lastMousePos = IGameState::m_engine->getInputManager()->getMousePosition();
    }
    m_triggerExitToMenu = false; // Zresetuj flagę wyjścia
}

void DemoState::cleanup() {
    Logger::getInstance().info("DemoState cleanup");

    // Odsubskrybuj zdarzenia (jeśli były subskrybowane w init)
    if (IGameState::m_engine && IGameState::m_engine->getEventManager()) {
        Logger::getInstance().info("DemoState unsubscribing from events.");
        IGameState::m_engine->getEventManager()->unsubscribe(EventType::KeyPressed, this);
        IGameState::m_engine->getEventManager()->unsubscribe(EventType::KeyReleased, this);
        IGameState::m_engine->getEventManager()->unsubscribe(EventType::MouseMoved, this);
        IGameState::m_engine->getEventManager()->unsubscribe(EventType::MouseScrolled, this);
    }

    // Usuń obiekty ze sceny z renderera i systemu kolizji silnika
    if (m_engine) {
        for (const auto& prim : m_scenePrimitives) {
            m_engine->removeRenderable(prim.get());
        }
        for (const auto& model : m_sceneModels) {
            m_engine->removeRenderable(model.get());
        }
    }
    m_scenePrimitives.clear(); // unique_ptr automatycznie zwolni pamięć
    m_sceneModels.clear();     // unique_ptr automatycznie zwolni pamięć

    // Zwolnij przechwycenie myszy, jeśli było aktywne
    if (IGameState::m_engine->getInputManager() && IGameState::m_engine->getInputManager()->isMouseCaptured()) {
        IGameState::m_engine->getInputManager()->setMouseCapture(false);
    }
}

void DemoState::pause() {
    Logger::getInstance().info("DemoState paused");
    // Zwolnij przechwycenie myszy, jeśli stan jest pauzowany
    if (m_mouseCaptured && IGameState::m_engine && IGameState::m_engine->getInputManager()) {
        IGameState::m_engine->getInputManager()->setMouseCapture(false);
        // Nie zmieniamy m_mouseCaptured, aby po resume() wiedzieć, czy przywrócić przechwycenie
    }
}

void DemoState::resume() {
    Logger::getInstance().info("DemoState resumed");
    // Przywróć przechwycenie myszy, jeśli było aktywne przed pauzą
    if (IGameState::m_engine && IGameState::m_engine->getInputManager()) {
        if (m_mouseCaptured) { // Jeśli stan przed pauzą miał przechwyconą mysz
            IGameState::m_engine->getInputManager()->setMouseCapture(true);
        }
        // Alternatywnie, można by zawsze przechwytywać mysz przy resume:
        // IGameState::m_engine->getInputManager()->setMouseCapture(true);
        // m_mouseCaptured = true;
    }
    m_firstMouse = true; // Zawsze resetuj flagę pierwszej aktualizacji myszy po wznowieniu
    if (m_mouseCaptured && IGameState::m_engine->getInputManager()) { // Zaktualizuj pozycję myszy, jeśli jest przechwycona
        m_lastMousePos = IGameState::m_engine->getInputManager()->getMousePosition();
    }
    m_triggerExitToMenu = false; // Zresetuj flagę wyjścia
}

void DemoState::onEvent(Event& event) {
    // Ta metoda jest wywoływana przez EventManager dla zdarzeń, na które stan jest zasubskrybowany.
    // Logika obsługi zdarzeń (KeyPressed, KeyReleased, MouseMoved, MouseScrolled)
    // skopiowana z oryginalnego GameplayState.

    if (event.getEventType() == EventType::KeyPressed) {
        KeyPressedEvent& e = static_cast<KeyPressedEvent&>(event);
        int key = e.getKeyCode();

        // Sterowanie kamerą
        if (key == GLFW_KEY_W) m_camMoveForward = true;
        else if (key == GLFW_KEY_S) m_camMoveBackward = true;
        else if (key == GLFW_KEY_A) m_camMoveLeft = true;
        else if (key == GLFW_KEY_D) m_camMoveRight = true;
        // Sterowanie obiektami (ruch)
        else if (key == GLFW_KEY_UP) m_objMoveZ_neg = true;
        else if (key == GLFW_KEY_DOWN) m_objMoveZ_pos = true;
        else if (key == GLFW_KEY_LEFT) m_objMoveX_neg = true;
        else if (key == GLFW_KEY_RIGHT) m_objMoveX_pos = true;
        else if (key == GLFW_KEY_PAGE_UP) m_objMoveY_pos = true;
        else if (key == GLFW_KEY_PAGE_DOWN) m_objMoveY_neg = true;
        // Sterowanie obiektami (rotacja)
        else if (key == GLFW_KEY_I) m_objRotateX_pos = true;    // Obrót X+
        else if (key == GLFW_KEY_K) m_objRotateX_neg = true;    // Obrót X-
        else if (key == GLFW_KEY_J) m_objRotateY_pos = true;    // Obrót Y+
        else if (key == GLFW_KEY_L) m_objRotateY_neg = true;    // Obrót Y-
        else if (key == GLFW_KEY_U) m_objRotateZ_pos = true;    // Obrót Z+
        else if (key == GLFW_KEY_O) m_objRotateZ_neg = true;    // Obrót Z-
        // Sterowanie obiektami (skala)
        else if (key == GLFW_KEY_KP_ADD || key == GLFW_KEY_EQUAL) m_objScaleUp = true;
        else if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS) m_objScaleDown = true;
        else { // Wybór obiektów i inne akcje
            bool handledThisKey = false; // Zmienna pomocnicza, aby oznaczyć, czy klawisz został już obsłużony

            // Wybór prymitywów (klawisze 1-9, 0)
            for (int i = 0; i < 9 && static_cast<size_t>(i) < m_scenePrimitives.size(); ++i) {
                if (key == (GLFW_KEY_1 + i)) {
                    m_currentSelection = ActiveSelectionType::PRIMITIVE;
                    m_activePrimitiveIndex = i;
                    m_activeModelIndex = -1; m_activePointLightIndex = -1; m_activeSpotLightIndex = -1;
                    Logger::getInstance().info("Wybrano prymityw: " + std::to_string(i + 1));
                    handledThisKey = true; break;
                }
            }
            if (!handledThisKey && m_scenePrimitives.size() >= 10 && key == GLFW_KEY_0) {
                if (static_cast<size_t>(9) < m_scenePrimitives.size()) { // Upewnij się, że 10-ty prymityw istnieje
                    m_currentSelection = ActiveSelectionType::PRIMITIVE;
                    m_activePrimitiveIndex = 9;
                    m_activeModelIndex = -1; m_activePointLightIndex = -1; m_activeSpotLightIndex = -1;
                    Logger::getInstance().info("Wybrano prymityw: 10");
                    handledThisKey = true;
                }
            }

            // Wybór modeli (F11 - cyklicznie)
            if (!handledThisKey && key == GLFW_KEY_F11 && !m_sceneModels.empty()) {
                m_currentSelection = ActiveSelectionType::MODEL;
                m_activeModelIndex = (m_activeModelIndex + 1) % static_cast<int>(m_sceneModels.size());
                m_activePrimitiveIndex = -1; m_activePointLightIndex = -1; m_activeSpotLightIndex = -1;
                Logger::getInstance().info("Wybrano model: " + m_sceneModels[m_activeModelIndex]->getName() + " (Indeks: " + std::to_string(m_activeModelIndex) + ")");
                handledThisKey = true;
            }

            // Wybór świateł (F2, F3, F4)
            if (!handledThisKey && key == GLFW_KEY_F2 && m_lightingManager && !m_lightingManager->getPointLights().empty()) {
                m_currentSelection = ActiveSelectionType::POINT_LIGHT;
                m_activePointLightIndex = (m_activePointLightIndex + 1) % static_cast<int>(m_lightingManager->getPointLights().size());
                m_activePrimitiveIndex = -1; m_activeModelIndex = -1; m_activeSpotLightIndex = -1;
                Logger::getInstance().info("Wybrano swiatlo punktowe: " + std::to_string(m_activePointLightIndex + 1));
                handledThisKey = true;
            }
            if (!handledThisKey && key == GLFW_KEY_F3 && m_lightingManager && !m_lightingManager->getSpotLights().empty()) {
                m_currentSelection = ActiveSelectionType::SPOT_LIGHT;
                m_activeSpotLightIndex = (m_activeSpotLightIndex + 1) % static_cast<int>(m_lightingManager->getSpotLights().size());
                m_activePrimitiveIndex = -1; m_activeModelIndex = -1; m_activePointLightIndex = -1;
                Logger::getInstance().info("Wybrano reflektor: " + std::to_string(m_activeSpotLightIndex + 1));
                handledThisKey = true;
            }
            if (!handledThisKey && key == GLFW_KEY_F4) {
                m_currentSelection = ActiveSelectionType::DIR_LIGHT;
                m_activePrimitiveIndex = -1; m_activeModelIndex = -1; m_activePointLightIndex = -1; m_activeSpotLightIndex = -1;
                Logger::getInstance().info("Wybrano swiatlo kierunkowe.");
                handledThisKey = true;
            }

            // Przełączanie stanu światła kierunkowego (L)
            if (!handledThisKey && key == GLFW_KEY_P && m_lightingManager) {
                DirectionalLight& dirLight = m_lightingManager->getDirectionalLight();
                dirLight.enabled = !dirLight.enabled;
                Logger::getInstance().info("Swiatlo kierunkowe: " + std::string(dirLight.enabled ? "ON" : "OFF"));
                handledThisKey = true;
            }

            // Przełączanie cieni (F5 dla reflektora, F6 dla światła punktowego)
            if (!handledThisKey && key == GLFW_KEY_F5 && m_lightingManager && IGameState::m_engine->getShadowSystem()) {
                int idxToToggle = (m_activeSpotLightIndex != -1) ? m_activeSpotLightIndex : (!m_lightingManager->getSpotLights().empty() ? 0 : -1);
                if (idxToToggle != -1 && static_cast<size_t>(idxToToggle) < m_lightingManager->getSpotLights().size()) {
                    SpotLight* sl = m_lightingManager->getSpotLight(idxToToggle);
                    sl->castsShadow = !sl->castsShadow;
                    IGameState::m_engine->enableSpotLightShadow(idxToToggle, sl->castsShadow); // Powiadom silnik o zmianie
                    Logger::getInstance().info("Cien reflektora " + std::to_string(idxToToggle + 1) + ": " + (sl->castsShadow ? "ON" : "OFF"));
                    if (idxToToggle == 0) m_spotLight0_shadow_enabled = sl->castsShadow; // Aktualizuj flagę dla pierwszego światła
                }
                handledThisKey = true;
            }
            if (!handledThisKey && key == GLFW_KEY_F6 && m_lightingManager && IGameState::m_engine->getShadowSystem()) {
                int idxToToggle = (m_activePointLightIndex != -1) ? m_activePointLightIndex : (!m_lightingManager->getPointLights().empty() ? 0 : -1);
                if (idxToToggle != -1 && static_cast<size_t>(idxToToggle) < m_lightingManager->getPointLights().size()) {
                    PointLight* pl = m_lightingManager->getPointLight(idxToToggle);
                    pl->castsShadow = !pl->castsShadow;
                    IGameState::m_engine->enablePointLightShadow(idxToToggle, pl->castsShadow); // Powiadom silnik
                    Logger::getInstance().info("Cien sw. punktowego " + std::to_string(idxToToggle + 1) + ": " + (pl->castsShadow ? "ON" : "OFF"));
                    if (idxToToggle == 0) m_pointLight0_shadow_enabled = pl->castsShadow; // Aktualizuj flagę dla pierwszego światła
                }
                handledThisKey = true;
            }

            if (handledThisKey) e.handled = true; // Oznacz zdarzenie jako obsłużone, jeśli któryś z powyższych warunków zadziałał
        }
        // Koniec obsługi zdarzenia KeyPressed
    }
    else if (event.getEventType() == EventType::KeyReleased) {
        KeyReleasedEvent& e = static_cast<KeyReleasedEvent&>(event);
        int key = e.getKeyCode();
        // Resetowanie flag ruchu/transformacji po zwolnieniu klawisza
        if (key == GLFW_KEY_W) m_camMoveForward = false;
        else if (key == GLFW_KEY_S) m_camMoveBackward = false;
        else if (key == GLFW_KEY_A) m_camMoveLeft = false;
        else if (key == GLFW_KEY_D) m_camMoveRight = false;
        else if (key == GLFW_KEY_UP) m_objMoveZ_neg = false;
        else if (key == GLFW_KEY_DOWN) m_objMoveZ_pos = false;
        else if (key == GLFW_KEY_LEFT) m_objMoveX_neg = false;
        else if (key == GLFW_KEY_RIGHT) m_objMoveX_pos = false;
        else if (key == GLFW_KEY_PAGE_UP) m_objMoveY_pos = false;
        else if (key == GLFW_KEY_PAGE_DOWN) m_objMoveY_neg = false;
        else if (key == GLFW_KEY_I) m_objRotateX_pos = false;
        else if (key == GLFW_KEY_K) m_objRotateX_neg = false;
        else if (key == GLFW_KEY_J) m_objRotateY_pos = false;
        else if (key == GLFW_KEY_L) m_objRotateY_neg = false;
        else if (key == GLFW_KEY_U) m_objRotateZ_pos = false;
        else if (key == GLFW_KEY_O) m_objRotateZ_neg = false;
        else if (key == GLFW_KEY_KP_ADD || key == GLFW_KEY_EQUAL) m_objScaleUp = false;
        else if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS) m_objScaleDown = false;
        // Koniec obsługi zdarzenia KeyReleased
    }
    else if (event.getEventType() == EventType::MouseMoved) {
        MouseMovedEvent& e = static_cast<MouseMovedEvent&>(event);
        if (m_mouseCaptured && m_camera && IGameState::m_engine->getInputManager()) {
            glm::vec2 currentMousePos = e.getPosition();
            if (m_firstMouse) { // Pierwszy ruch myszy po przechwyceniu
                m_lastMousePos = currentMousePos;
                m_firstMouse = false;
            }
            glm::vec2 offset = currentMousePos - m_lastMousePos;
            m_lastMousePos = currentMousePos;

            // Odwracamy offset.y, ponieważ współrzędne Y ekranu rosną w dół, a pitch kamery w górę
            if (offset.x != 0 || offset.y != 0) { // Tylko jeśli był ruch
                m_camera->processMouseMovement(offset.x, -offset.y);
            }
        }
        // Koniec obsługi zdarzenia MouseMoved
    }
    else if (event.getEventType() == EventType::MouseScrolled) {
        MouseScrolledEvent& e = static_cast<MouseScrolledEvent&>(event);
        if (m_camera) {
            m_camera->processMouseScroll(e.getYOffset()); // Przekaż offset Y do kamery (zoom)
        }
        // Koniec obsługi zdarzenia MouseScrolled
    }
}


void DemoState::handleEvents(InputManager* inputManager, EventManager* eventManager) {
    // Ta metoda jest wywoływana przez GameStateManager. W tym przypadku, większość obsługi
    // zdarzeń jest w onEvent(), ponieważ subskrybujemy zdarzenia.
    // Można tu dodać logikę, która powinna być sprawdzana co klatkę na podstawie InputManagera,
    // niezależnie od systemu zdarzeń (np. ciągłe trzymanie klawisza, które nie generuje KeyPressedEvent co klatkę).
    // Logika z oryginalnego GameplayState::handleEvents:
    if (!inputManager) return;

    // Wyjście do menu (ESC)
    if (inputManager->isKeyTyped(GLFW_KEY_ESCAPE)) { // Zmieniono na isKeyTyped dla jednorazowej reakcji
        m_triggerExitToMenu = true;
    }
    // Przełączanie przechwytywania myszy (M)
    if (inputManager->isKeyTyped(GLFW_KEY_M)) { // Zmieniono na isKeyTyped
        bool currentCaptureState = inputManager->isMouseCaptured();
        m_mouseCaptured = !currentCaptureState;
        inputManager->setMouseCapture(m_mouseCaptured);
        Logger::getInstance().info("Mouse capture toggled via M key to: " + std::string(m_mouseCaptured ? "ON" : "OFF"));
        m_firstMouse = true; // Zresetuj flagę przy każdej zmianie trybu myszy
        if (m_mouseCaptured) { // Jeśli właśnie przechwycono, zaktualizuj ostatnią pozycję
            m_lastMousePos = inputManager->getMousePosition();
        }
    }
    // Przełączanie wyświetlania FPS (F1)
    if (inputManager->isKeyTyped(GLFW_KEY_F1)) { // Zmieniono na isKeyTyped
        IGameState::m_engine->toggleFPSDisplay();
    }
}

void DemoState::update(float deltaTime) {
    // Zmiana stanu, jeśli została wyzwolona
    if (m_triggerExitToMenu) {
        Logger::getInstance().info("DemoState: Changing to MenuState.");
        IGameState::m_engine->getGameStateManager()->changeState(std::make_unique<MenuState>());
        m_triggerExitToMenu = false; // Zresetuj flagę
        return; // Ważne, aby zakończyć update po zmianie stanu
    }

    // Aktualizacja kamery, jeśli jest aktywna
    if (!m_camera) return; // Zabezpieczenie

    float camSpeed = m_camera->getMovementSpeed() * deltaTime; // Prędkość kamery w tej klatce
    if (m_camMoveForward) m_camera->processKeyboard(Camera::Direction::Forward, camSpeed);
    if (m_camMoveBackward) m_camera->processKeyboard(Camera::Direction::Backward, camSpeed);
    if (m_camMoveLeft) m_camera->processKeyboard(Camera::Direction::Left, camSpeed);
    if (m_camMoveRight) m_camera->processKeyboard(Camera::Direction::Right, camSpeed);

    // Zastosuj transformacje do wybranego obiektu
    applyTransformationsToSelected(deltaTime);

    // Inna logika specyficzna dla stanu gry (np. aktualizacja fizyki, AI)
}

void DemoState::applyTransformationsToSelected(float deltaTime) {
    // Logika transformacji obiektów (przesunięcie, rotacja, skalowanie)
    // skopiowana z oryginalnego GameplayState.

    glm::vec3 moveDir(0.0f);
    if (m_objMoveZ_neg) moveDir.z -= 1.0f;
    if (m_objMoveZ_pos) moveDir.z += 1.0f;
    if (m_objMoveX_neg) moveDir.x -= 1.0f;
    if (m_objMoveX_pos) moveDir.x += 1.0f;
    if (m_objMoveY_pos) moveDir.y += 1.0f;
    if (m_objMoveY_neg) moveDir.y -= 1.0f;

    glm::vec3 rotAxisAccumulator(0.0f);
    if (m_objRotateX_pos) rotAxisAccumulator.x += 1.0f;
    if (m_objRotateX_neg) rotAxisAccumulator.x -= 1.0f;
    if (m_objRotateY_pos) rotAxisAccumulator.y += 1.0f;
    if (m_objRotateY_neg) rotAxisAccumulator.y -= 1.0f;
    if (m_objRotateZ_pos) rotAxisAccumulator.z += 1.0f;
    if (m_objRotateZ_neg) rotAxisAccumulator.z -= 1.0f;

    float actualRotAngleDeg = 0.0f;
    glm::vec3 finalRotAxis = glm::vec3(0.0f, 1.0f, 0.0f); // Domyślna oś Y

    if (glm::length(rotAxisAccumulator) > 0.001f) { // Jeśli jest jakiś wkład do rotacji
        finalRotAxis = glm::normalize(rotAxisAccumulator);
        actualRotAngleDeg = m_rotationSpeed * deltaTime;
    }

    float scaleChangeFactor = 1.0f;
    if (m_objScaleUp) scaleChangeFactor *= (1.0f + (m_scaleSpeed * deltaTime));
    if (m_objScaleDown) scaleChangeFactor /= (1.0f + (m_scaleSpeed * deltaTime));
    // glm::vec3 scaleChangeVec(scaleChangeFactor); // Dla jednolitego skalowania, jeśli prymitywy/modele tego wymagają

    glm::vec3 actualMove = glm::vec3(0.0f);
    if (glm::length(moveDir) > 0.001f) {
        actualMove = glm::normalize(moveDir) * m_moveSpeed * deltaTime;
    }

    // Zastosuj transformacje w zależności od wybranego typu obiektu
    switch (m_currentSelection) {
    case ActiveSelectionType::PRIMITIVE:
        if (m_activePrimitiveIndex != -1 && static_cast<size_t>(m_activePrimitiveIndex) < m_scenePrimitives.size()) {
            BasePrimitive* prim = m_scenePrimitives[m_activePrimitiveIndex].get();
            if (prim) { // Dodatkowe sprawdzenie wskaźnika
                if (glm::length(actualMove) > 0.001f) prim->translate(actualMove);
                if (actualRotAngleDeg != 0.0f) prim->rotate(actualRotAngleDeg, finalRotAxis);
                if (std::abs(scaleChangeFactor - 1.0f) > 0.0001f && scaleChangeFactor > 0.0f) {
                    prim->scale(glm::vec3(scaleChangeFactor)); // Skalowanie prymitywu
                }
            }
        }
        break;
    case ActiveSelectionType::MODEL:
        if (m_activeModelIndex != -1 && static_cast<size_t>(m_activeModelIndex) < m_sceneModels.size()) {
            Model* model = m_sceneModels[m_activeModelIndex].get();
            if (model) { // Dodatkowe sprawdzenie wskaźnika
                if (glm::length(actualMove) > 0.001f) model->setPosition(model->getPosition() + actualMove);
                if (actualRotAngleDeg != 0.0f) {
                    glm::quat currentRot = model->getRotation();
                    glm::quat rotChange = glm::angleAxis(glm::radians(actualRotAngleDeg), finalRotAxis);
                    model->setRotation(rotChange * currentRot); // Mnożenie kwaternionów
                }
                if (std::abs(scaleChangeFactor - 1.0f) > 0.0001f && scaleChangeFactor > 0.0f) {
                    model->setScale(model->getScale() * scaleChangeFactor); // Mnożenie skali modelu
                }
            }
        }
        break;
    case ActiveSelectionType::POINT_LIGHT:
        if (m_activePointLightIndex != -1 && m_lightingManager && static_cast<size_t>(m_activePointLightIndex) < m_lightingManager->getPointLights().size()) {
            PointLight* pLight = m_lightingManager->getPointLight(m_activePointLightIndex);
            if (pLight && glm::length(actualMove) > 0.001f) pLight->position += actualMove;
        }
        break;
    case ActiveSelectionType::SPOT_LIGHT:
        if (m_activeSpotLightIndex != -1 && m_lightingManager && static_cast<size_t>(m_activeSpotLightIndex) < m_lightingManager->getSpotLights().size()) {
            SpotLight* sLight = m_lightingManager->getSpotLight(m_activeSpotLightIndex);
            if (sLight) {
                if (glm::length(actualMove) > 0.001f) sLight->position += actualMove;
                if (actualRotAngleDeg != 0.0f) { // Rotacja kierunku reflektora
                    glm::mat4 rotationMat = glm::rotate(glm::mat4(1.0f), glm::radians(actualRotAngleDeg), finalRotAxis);
                    sLight->direction = glm::normalize(glm::vec3(rotationMat * glm::vec4(sLight->direction, 0.0f)));
                }
            }
        }
        break;
    case ActiveSelectionType::DIR_LIGHT:
        if (m_lightingManager) {
            DirectionalLight& dLight = m_lightingManager->getDirectionalLight();
            if (actualRotAngleDeg != 0.0f) { // Tylko rotacja dla światła kierunkowego
                glm::mat4 rotationMat = glm::rotate(glm::mat4(1.0f), glm::radians(actualRotAngleDeg), finalRotAxis);
                dLight.direction = glm::normalize(glm::vec3(rotationMat * glm::vec4(dLight.direction, 0.0f)));
            }
        }
        break;
    case ActiveSelectionType::NONE: // Brak wybranego obiektu
    default:
        break; // Nic nie rób
    }
}


void DemoState::renderSelectionInstructions() {
    if (!m_textRenderer || !m_textRenderer->isInitialized() || !IGameState::m_engine) return;

    float windowWidth = static_cast<float>(IGameState::m_engine->getWindowWidth());
    float windowHeight = static_cast<float>(IGameState::m_engine->getWindowHeight());
    float xPos = windowWidth - 420.0f; // Pozycja X dla tekstu (od prawej krawędzi)
    if (xPos < 10.0f) xPos = 10.0f;     // Zapewnij minimalny margines
    float yPosStart = windowHeight - 20.0f; // Pozycja Y startowa (od góry)
    float lineHeight = 18.0f;
    float scale = 0.6f;
    glm::vec3 textColor(0.9f, 0.9f, 0.9f); // Jasny kolor tekstu

    std::vector<std::string> instructions;
    instructions.push_back("--- Sterowanie Ogolne ---");
    instructions.push_back("Kamera: WASD, Mysz (M: tryb myszy)");
    instructions.push_back("Przelacz FPS: F1 | Wyjscie: Menu (ESC)");
    instructions.push_back("--- Wybor Elementu ---");
    instructions.push_back("Prymityw: 1-" + std::to_string(std::min(static_cast<size_t>(9), m_scenePrimitives.size())) + (m_scenePrimitives.size() >= 10 ? " (0 dla 10.)" : ""));
    if (!m_sceneModels.empty()) {
        instructions.push_back("Model: F11 (cyklicznie)");
    }
    instructions.push_back("Swiatlo Pkt: F2 | Reflektor: F3");
    instructions.push_back("Swiatlo Kier.: F4");
    instructions.push_back("Przelaczanie sw. kier.: P");

    std::string spotShadowStatus = "N/A";
    if (m_lightingManager && !m_lightingManager->getSpotLights().empty()) {
        int shadowIdx = (m_activeSpotLightIndex != -1) ? m_activeSpotLightIndex : 0;
        if (static_cast<size_t>(shadowIdx) < m_lightingManager->getSpotLights().size()) {
            spotShadowStatus = m_lightingManager->getSpotLights()[shadowIdx].castsShadow ? "ON" : "OFF";
        }
        instructions.push_back("Cien Reflektora (" + std::to_string(shadowIdx + 1) + "): F5 (" + spotShadowStatus + ")");
    }
    else {
        instructions.push_back("Cien Reflektora: F5 (Brak reflektorow)");
    }


    std::string pointShadowStatus = "N/A";
    if (m_lightingManager && !m_lightingManager->getPointLights().empty()) {
        int shadowIdx = (m_activePointLightIndex != -1) ? m_activePointLightIndex : 0;
        if (static_cast<size_t>(shadowIdx) < m_lightingManager->getPointLights().size()) {
            pointShadowStatus = m_lightingManager->getPointLights()[shadowIdx].castsShadow ? "ON" : "OFF";
        }
        instructions.push_back("Cien Sw. Pkt. (" + std::to_string(shadowIdx + 1) + "): F6 (" + pointShadowStatus + ")");
    }
    else {
        instructions.push_back("Cien Sw. Pkt.: F6 (Brak swiatel pkt.)");
    }


    instructions.push_back("--- Transformacje Wybranego ---");
    instructions.push_back("Ruch (XZ): Strzalki | Ruch (Y): PgUp/PgDn");
    instructions.push_back("Obrot (X,Y,Z): I/K, J/L, U/O");
    if (m_currentSelection == ActiveSelectionType::PRIMITIVE || m_currentSelection == ActiveSelectionType::MODEL) {
        instructions.push_back("Skala: +/-");
    }
    instructions.push_back("--- Aktywny Element ---");
    std::string activeElementStr = selectionTypeToString(m_currentSelection);
    if (m_currentSelection == ActiveSelectionType::PRIMITIVE && m_activePrimitiveIndex != -1 && static_cast<size_t>(m_activePrimitiveIndex) < m_scenePrimitives.size()) activeElementStr += " " + std::to_string(m_activePrimitiveIndex + 1);
    else if (m_currentSelection == ActiveSelectionType::MODEL && m_activeModelIndex != -1 && static_cast<size_t>(m_activeModelIndex) < m_sceneModels.size()) activeElementStr += " " + m_sceneModels[m_activeModelIndex]->getName();
    else if (m_currentSelection == ActiveSelectionType::POINT_LIGHT && m_activePointLightIndex != -1 && m_lightingManager && !m_lightingManager->getPointLights().empty()) activeElementStr += " " + std::to_string(m_activePointLightIndex + 1);
    else if (m_currentSelection == ActiveSelectionType::SPOT_LIGHT && m_activeSpotLightIndex != -1 && m_lightingManager && !m_lightingManager->getSpotLights().empty()) activeElementStr += " " + std::to_string(m_activeSpotLightIndex + 1);
    instructions.push_back(activeElementStr);

    for (size_t i = 0; i < instructions.size(); ++i) {
        m_textRenderer->renderText(instructions[i], xPos, yPosStart - (i * lineHeight), scale, textColor);
    }
}

void DemoState::render(Renderer* rendererFromEngine) {
    // Sprawdzenie podstawowych zależności
    if (!m_camera || !IGameState::m_engine) {
        Logger::getInstance().error("DemoState::render - Camera or Engine is null.");
        return;
    }

    // Pobranie macierzy widoku i projekcji z kamery
    glm::mat4 projectionMatrix = m_camera->getProjectionMatrix();
    glm::mat4 viewMatrix = m_camera->getViewMatrix();

    // Funkcja pomocnicza (lambda) do ustawiania uniformów oświetlenia na danym shaderze
    auto configureShaderForLighting = [&](std::shared_ptr<Shader> shader) {
        if (!shader || !m_lightingManager) return;
        shader->use();
        shader->setVec3("viewPos", m_camera->getPosition());

        // Światło kierunkowe
        const DirectionalLight& dirLight = m_lightingManager->getDirectionalLight();
        shader->setBool("dirLight.enabled", dirLight.enabled);
        if (dirLight.enabled) {
            shader->setVec3("dirLight.direction", dirLight.direction);
            shader->setVec3("dirLight.ambient", dirLight.ambient);
            shader->setVec3("dirLight.diffuse", dirLight.diffuse);
            shader->setVec3("dirLight.specular", dirLight.specular);
        }

        // Światła punktowe
        const auto& pointLights = m_lightingManager->getPointLights();
        int numPointLightsToSet = std::min((int)pointLights.size(), 4); // Ograniczenie do max 4 świateł w shaderze
        shader->setInt("numPointLights", numPointLightsToSet);
        for (int i = 0; i < numPointLightsToSet; ++i) {
            std::string prefix = "pointLights[" + std::to_string(i) + "].";
            shader->setBool(prefix + "enabled", pointLights[i].enabled);
            if (pointLights[i].enabled) {
                shader->setVec3(prefix + "position", pointLights[i].position);
                shader->setVec3(prefix + "ambient", pointLights[i].ambient);
                shader->setVec3(prefix + "diffuse", pointLights[i].diffuse);
                shader->setVec3(prefix + "specular", pointLights[i].specular);
                shader->setFloat(prefix + "constant", pointLights[i].constant);
                shader->setFloat(prefix + "linear", pointLights[i].linear);
                shader->setFloat(prefix + "quadratic", pointLights[i].quadratic);
                shader->setBool(prefix + "castsShadow", pointLights[i].castsShadow);
                shader->setInt(prefix + "shadowDataIndex", pointLights[i].shadowDataIndex);
            }
        }

        // Reflektory (Spot lights)
        const auto& spotLights = m_lightingManager->getSpotLights();
        int numSpotLightsToSet = std::min((int)spotLights.size(), 4);
        shader->setInt("numSpotLights", numSpotLightsToSet);
        for (int i = 0; i < numSpotLightsToSet; ++i) {
            std::string prefix = "spotLights[" + std::to_string(i) + "].";
            shader->setBool(prefix + "enabled", spotLights[i].enabled);
            if (spotLights[i].enabled) {
                shader->setVec3(prefix + "position", spotLights[i].position);
                shader->setVec3(prefix + "direction", spotLights[i].direction);
                shader->setVec3(prefix + "ambient", spotLights[i].ambient);
                shader->setVec3(prefix + "diffuse", spotLights[i].diffuse);
                shader->setVec3(prefix + "specular", spotLights[i].specular);
                shader->setFloat(prefix + "cutOff", spotLights[i].cutOff);
                shader->setFloat(prefix + "outerCutOff", spotLights[i].outerCutOff);
                shader->setBool(prefix + "castsShadow", spotLights[i].castsShadow);
                shader->setInt(prefix + "shadowDataIndex", spotLights[i].shadowDataIndex);
            }
        }

        // Przekazanie informacji o cieniach do shadera
        if (IGameState::m_engine->getShadowSystem()) {
            IGameState::m_engine->getShadowSystem()->uploadShadowUniforms(shader, *m_lightingManager);
        }
        };

    // --- Renderowanie prymitywów ---
    for (const auto& prim_ptr : m_scenePrimitives) {
        if (prim_ptr) {
            std::shared_ptr<Shader> shader = prim_ptr->getShader();
            if (shader) {
                configureShaderForLighting(shader);
                // Metoda render prymitywu powinna ustawić macierz modelu i inne uniformy materiału
                prim_ptr->render(viewMatrix, projectionMatrix, m_camera);
            }
        }
    }

    // --- Renderowanie modeli ---
    for (const auto& model_ptr : m_sceneModels) {
        if (model_ptr) {
            std::shared_ptr<Shader> shader = model_ptr->getShader();
            if (shader) {
                configureShaderForLighting(shader);
                // Metoda render modelu zajmuje się rysowaniem wszystkich jego siatek (mesh)
                model_ptr->render(viewMatrix, projectionMatrix);
            }
        }
    }

    // --- Renderowanie interfejsu (UI) ---
    renderSelectionInstructions();

    if (m_textRenderer && !m_mouseCaptured) {
        float yPos = static_cast<float>(IGameState::m_engine->getWindowHeight()) - 60.0f;
        m_textRenderer->renderText("Mysz uwolniona. Nacisnij M aby przechwycic.", 10.0f, yPos - 250.0f, 0.7f, glm::vec3(0.9f, 0.9f, 0.1f));
    }
}
