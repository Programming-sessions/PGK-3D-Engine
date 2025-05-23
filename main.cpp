// Należy zdefiniować PRZED jakimkolwiek include'em GLM
#define GLM_ENABLE_EXPERIMENTAL

#include <glad/glad.h>
#include "Engine.h"
#include "InputManager.h"
#include "EventManager.h" 
#include "Event.h"        
#include "IEventListener.h" 
#include "Primitives.h"
#include "BoundingVolume.h"
#include "Lighting.h"
#include "TextRenderer.h"
#include "Camera.h"
#include "GameStateManager.h" 
#include "Logger.h"         
#include "IGameState.h"       
#include "Shader.h" 
#include "ResourceManager.h" // Dodano dla ResourceManager
#include "Texture.h"         // Dodano dla Texture
#include "Model.h"           // Dodano dla Model

#include <vector>
#include <string>
#include <memory> 
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/transform.hpp>
#include <algorithm> 
#include <cmath>     
#include <iomanip> // Dla std::fixed, std::setprecision w renderSelectionInstructions

// Enum do śledzenia, co jest aktualnie wybrane (z oryginalnego main.cpp)
enum class ActiveSelectionType {
    NONE,
    PRIMITIVE,
    MODEL, // Dodano dla modelu
    POINT_LIGHT,
    SPOT_LIGHT,
    DIR_LIGHT
};

// Funkcja pomocnicza do konwersji typu wyboru na string (z oryginalnego main.cpp)
std::string selectionTypeToString(ActiveSelectionType type) {
    switch (type) {
    case ActiveSelectionType::NONE: return "None";
    case ActiveSelectionType::PRIMITIVE: return "Prymityw";
    case ActiveSelectionType::MODEL: return "Model"; // Dodano dla modelu
    case ActiveSelectionType::POINT_LIGHT: return "Swiatlo Pkt.";
    case ActiveSelectionType::SPOT_LIGHT: return "Reflektor";
    case ActiveSelectionType::DIR_LIGHT: return "Swiatlo Kier.";
    default: return "Unknown";
    }
}


// Deklaracje wyprzedzające dla stanów
class GameplayState;
class MenuState;

// --- Implementacja MenuState ---
class MenuState : public IGameState {
public:
    MenuState()
        : m_textRenderer(nullptr),
        m_selectedOption(0), m_triggerStartGame(false), m_triggerExitGame(false) {
        Logger::getInstance().info("MenuState constructor");
    }

    ~MenuState() override {
        Logger::getInstance().info("MenuState destructor");
    }

    void init(Engine* engine) override {
        IGameState::m_engine = engine;

        Logger::getInstance().info("MenuState init");
        m_textRenderer = TextRenderer::getInstance();
        if (!m_textRenderer || !m_textRenderer->isInitialized()) {
            Logger::getInstance().error("MenuState: TextRenderer not available or not initialized!");
        }

        m_menuOptions.push_back("Start Game");
        m_menuOptions.push_back("Exit");

        m_selectedOption = 0;
        m_triggerStartGame = false;
        m_triggerExitGame = false;
    }

    void cleanup() override {
        Logger::getInstance().info("MenuState cleanup");
        m_menuOptions.clear();
    }

    void pause() override {
        Logger::getInstance().info("MenuState paused");
    }

    void resume() override {
        Logger::getInstance().info("MenuState resumed");
        m_triggerStartGame = false;
        m_triggerExitGame = false;
    }

    void handleEvents(InputManager* inputManager, EventManager* eventManager) override {
        if (!inputManager) return;

        if (inputManager->isKeyPressed(GLFW_KEY_UP)) {
            m_selectedOption--;
            if (m_selectedOption < 0) {
                m_selectedOption = static_cast<int>(m_menuOptions.size()) - 1;
            }
        }
        if (inputManager->isKeyPressed(GLFW_KEY_DOWN)) {
            m_selectedOption++;
            if (m_selectedOption >= static_cast<int>(m_menuOptions.size())) {
                m_selectedOption = 0;
            }
        }
        if (inputManager->isKeyPressed(GLFW_KEY_ENTER)) {
            if (m_selectedOption == 0) { // Start Game
                m_triggerStartGame = true;
            }
            else if (m_selectedOption == 1) { // Exit
                m_triggerExitGame = true;
            }
        }
        if (inputManager->isKeyPressed(GLFW_KEY_ESCAPE)) {
            m_triggerExitGame = true;
        }
    }

    void update(float deltaTime) override;

    void render(Renderer* renderer) override {
        if (m_textRenderer && m_textRenderer->isInitialized() && IGameState::m_engine) {
            float windowHeight = static_cast<float>(IGameState::m_engine->getWindowHeight());
            float x = 100.0f;
            float y = windowHeight / 1.5f;
            float scale = 1.0f;
            glm::vec3 titleColor(0.9f, 0.9f, 0.1f);
            glm::vec3 normalColor(0.7f, 0.7f, 0.7f);
            glm::vec3 selectedColor(1.0f, 1.0f, 0.0f);

            m_textRenderer->renderText("Super Gra Engine Test", x - 20.0f, y + 60.0f, 1.2f, titleColor);

            for (size_t i = 0; i < m_menuOptions.size(); ++i) {
                glm::vec3 color = (static_cast<int>(i) == m_selectedOption) ? selectedColor : normalColor;
                m_textRenderer->renderText(m_menuOptions[i], x, y - (i * 40.0f * scale), scale, color);
            }
            m_textRenderer->renderText("Uzyj strzalek GORA/DOL i ENTER", x, y - (m_menuOptions.size() * 40.0f * scale) - 40.0f, 0.7f, normalColor);
        }
    }

private:
    TextRenderer* m_textRenderer;
    std::vector<std::string> m_menuOptions;
    int m_selectedOption;
    bool m_triggerStartGame;
    bool m_triggerExitGame;
};

// --- Implementacja GameplayState ---
class GameplayState : public IGameState, public IEventListener {
public:
    GameplayState()
        : m_camera(nullptr), m_lightingManager(nullptr), m_textRenderer(nullptr),
        m_currentSelection(ActiveSelectionType::PRIMITIVE),
        m_activePrimitiveIndex(0),
        m_activeModelIndex(-1), // Dodano dla modelu
        m_activePointLightIndex(-1),
        m_activeSpotLightIndex(-1),
        m_spotLight0_shadow_enabled(true),
        m_pointLight0_shadow_enabled(true),
        m_camMoveForward(false), m_camMoveBackward(false), m_camMoveLeft(false), m_camMoveRight(false),
        m_objMoveZ_neg(false), m_objMoveZ_pos(false), m_objMoveX_neg(false), m_objMoveX_pos(false), m_objMoveY_pos(false), m_objMoveY_neg(false),
        m_objRotateX_pos(false), m_objRotateX_neg(false), m_objRotateY_pos(false), m_objRotateY_neg(false), m_objRotateZ_pos(false), m_objRotateZ_neg(false),
        m_objScaleUp(false), m_objScaleDown(false),
        m_mouseCaptured(false), m_triggerExitToMenu(false),
        m_firstMouse(true),
        m_moveSpeed(5.0f), m_rotationSpeed(70.0f), m_scaleSpeed(0.8f)
    {
        Logger::getInstance().info("GameplayState constructor");
    }

    ~GameplayState() override {
        Logger::getInstance().info("GameplayState destructor");
        if (IGameState::m_engine && IGameState::m_engine->getEventManager()) {
            IGameState::m_engine->getEventManager()->unsubscribe(EventType::KeyPressed, this);
            IGameState::m_engine->getEventManager()->unsubscribe(EventType::KeyReleased, this);
            IGameState::m_engine->getEventManager()->unsubscribe(EventType::MouseMoved, this);
            IGameState::m_engine->getEventManager()->unsubscribe(EventType::MouseScrolled, this);
        }
    }

    void init(Engine* engine) override {
        IGameState::m_engine = engine;
        Logger::getInstance().info("GameplayState init");

        m_camera = IGameState::m_engine->getCamera();
        m_lightingManager = IGameState::m_engine->getLightingManager();
        m_textRenderer = TextRenderer::getInstance();
        ResourceManager& resManager = ResourceManager::getInstance();

        if (!m_camera) Logger::getInstance().error("GameplayState: Camera is null!");
        if (!m_lightingManager) Logger::getInstance().error("GameplayState: LightingManager is null! Oświetlenie nie będzie działać.");
        if (!m_textRenderer || !m_textRenderer->isInitialized()) Logger::getInstance().warning("GameplayState: TextRenderer not usable.");

        if (IGameState::m_engine && IGameState::m_engine->getEventManager()) {
            Logger::getInstance().info("GameplayState subscribing to events: KeyPressed, KeyReleased, MouseMoved, MouseScrolled");
            IGameState::m_engine->getEventManager()->subscribe(EventType::KeyPressed, this);
            IGameState::m_engine->getEventManager()->subscribe(EventType::KeyReleased, this);
            IGameState::m_engine->getEventManager()->subscribe(EventType::MouseMoved, this);
            IGameState::m_engine->getEventManager()->subscribe(EventType::MouseScrolled, this);
        }
        else {
            Logger::getInstance().error("GameplayState: Cannot subscribe to events, EventManager is null.");
        }

        // --- Load Primitives ---
        auto ground = std::make_unique<Plane>(glm::vec3(0.0f, -1.0f, 0.0f), 20.0f, 20.0f, glm::vec4(0.4f, 0.6f, 0.4f, 1.0f));
        ground->setMaterialShininess(16.0f);
		ground->setCastsShadow(false);
		ground->setCollisionsEnabled(false);
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

        // --- Load Models ---
        std::shared_ptr<Shader> modelShader = resManager.getShader("lightingShader");
        if (!modelShader) {
            modelShader = resManager.loadShader("standardLighting", "assets/shaders/default_shader.vert", "assets/shaders/default_shader.frag");
        }

        if (!modelShader) {
            Logger::getInstance().error("GameplayState: Failed to load or get shader for models!");
        }
        else {
            Logger::getInstance().info("GameplayState: Using shader '" + modelShader->getName() + "' for models.");
        }

        std::shared_ptr<ModelAsset> cheeseAsset = resManager.loadModel("cheeseModel", "assets/models/Cheese.obj");
        if (cheeseAsset && modelShader) {
            auto cheeseModel = std::make_unique<Model>("Cheese", cheeseAsset, modelShader);
            cheeseModel->setPosition(glm::vec3(0.0f, -0.5f, -1.0f));
            cheeseModel->setScale(glm::vec3(0.3f));
            if (m_camera) cheeseModel->setCameraForLighting(m_camera);
            m_sceneModels.push_back(std::move(cheeseModel));
            Logger::getInstance().info("Cheese model loaded and added to scene.");
        }
        else {
            if (!cheeseAsset) Logger::getInstance().error("GameplayState: Failed to load cheese model asset!");
            if (!modelShader && cheeseAsset) Logger::getInstance().error("GameplayState: Cheese model asset loaded, but no shader available for it!");
        }


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

        if (m_engine) {
            for (const auto& prim : m_scenePrimitives) {
				m_engine->addRenderable(prim.get());          //???
            }
            for (const auto& model : m_sceneModels) {
                m_engine->addRenderable(model.get());
            }
        }

        if (m_lightingManager) {
            m_lightingManager->clearPointLights();
            m_lightingManager->clearSpotLights();

            PointLight pLight1;
            pLight1.position = glm::vec3(2.0f, 2.0f, 1.0f);
            pLight1.diffuse = glm::vec3(1.0f, 0.7f, 0.3f);
            pLight1.ambient = pLight1.diffuse * 0.1f;
            pLight1.specular = pLight1.diffuse;
            pLight1.linear = 0.07f;
            pLight1.quadratic = 0.017f;
            pLight1.castsShadow = m_pointLight0_shadow_enabled;
            pLight1.shadowNearPlane = 0.1f;
            pLight1.shadowFarPlane = 25.0f;
            m_lightingManager->addPointLight(pLight1);
			engine->enablePointLightShadow(0, true);

            SpotLight sLight1;
            sLight1.position = glm::vec3(-2.0f, 3.0f, 1.0f);
            sLight1.direction = glm::normalize(glm::vec3(0.5f, -1.0f, -0.3f));
            sLight1.diffuse = glm::vec3(0.5f, 0.8f, 1.0f);
            sLight1.ambient = sLight1.diffuse * 0.05f;
            sLight1.specular = sLight1.diffuse;
            sLight1.cutOff = glm::cos(glm::radians(15.5f));
            sLight1.outerCutOff = glm::cos(glm::radians(22.5f));
            sLight1.enabled = true;
            sLight1.castsShadow = m_spotLight0_shadow_enabled;
            m_lightingManager->addSpotLight(sLight1);
			engine->enableSpotLightShadow(0, true);

            if (IGameState::m_engine->getShadowSystem()) {
                if (!m_lightingManager->getPointLights().empty()) {
                    IGameState::m_engine->enablePointLightShadow(0, m_pointLight0_shadow_enabled);
                }
                if (!m_lightingManager->getSpotLights().empty()) {
                    IGameState::m_engine->enableSpotLightShadow(0, m_spotLight0_shadow_enabled);
                }
            }

            DirectionalLight& dirLight = m_lightingManager->getDirectionalLight();
            dirLight.enabled = true;
            dirLight.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
        }

        if (m_camera) {
            m_camera->setPosition(glm::vec3(0.0f, 1.5f, 6.0f));
            m_camera->setYaw(-90.0f);
            m_camera->setPitch(-10.0f);
        }

        m_mouseCaptured = IGameState::m_engine->getInputManager() ? IGameState::m_engine->getInputManager()->isMouseCaptured() : false;
        if (!m_mouseCaptured && IGameState::m_engine->getInputManager()) {
            IGameState::m_engine->getInputManager()->setMouseCapture(true);
            m_mouseCaptured = true;
        }
        m_firstMouse = true;
        if (m_mouseCaptured && IGameState::m_engine->getInputManager()) {
            m_lastMousePos = IGameState::m_engine->getInputManager()->getMousePosition();
        }
        m_triggerExitToMenu = false;
    }

    void cleanup() override {
        Logger::getInstance().info("GameplayState cleanup");
        if (IGameState::m_engine && IGameState::m_engine->getEventManager()) {
            Logger::getInstance().info("GameplayState unsubscribing from events.");
            IGameState::m_engine->getEventManager()->unsubscribe(EventType::KeyPressed, this);
            IGameState::m_engine->getEventManager()->unsubscribe(EventType::KeyReleased, this);
            IGameState::m_engine->getEventManager()->unsubscribe(EventType::MouseMoved, this);
            IGameState::m_engine->getEventManager()->unsubscribe(EventType::MouseScrolled, this);
        }

        if (m_engine) {
            for (const auto& prim : m_scenePrimitives) {
				m_engine->removeRenderable(prim.get());
            }
            for (const auto& model : m_sceneModels) {
                m_engine->removeRenderable(model.get());
            }
        }
        m_scenePrimitives.clear();
        m_sceneModels.clear();

        if (IGameState::m_engine->getInputManager() && IGameState::m_engine->getInputManager()->isMouseCaptured()) {
            IGameState::m_engine->getInputManager()->setMouseCapture(false);
        }
    }

    void pause() override {
        Logger::getInstance().info("GameplayState paused");
        if (m_mouseCaptured && IGameState::m_engine && IGameState::m_engine->getInputManager()) {
            IGameState::m_engine->getInputManager()->setMouseCapture(false);
        }
    }

    void resume() override {
        Logger::getInstance().info("GameplayState resumed");
        if (IGameState::m_engine && IGameState::m_engine->getInputManager()) {
            if (!m_mouseCaptured) {
                IGameState::m_engine->getInputManager()->setMouseCapture(true);
                m_mouseCaptured = true;
            }
            else if (m_mouseCaptured) {
                IGameState::m_engine->getInputManager()->setMouseCapture(true);
            }
        }
        m_firstMouse = true;
        if (m_mouseCaptured && IGameState::m_engine->getInputManager()) {
            m_lastMousePos = IGameState::m_engine->getInputManager()->getMousePosition();
        }
        m_triggerExitToMenu = false;
    }

    void onEvent(Event& event) override {
        if (event.getEventType() == EventType::KeyPressed) {
            KeyPressedEvent& e = static_cast<KeyPressedEvent&>(event);
            int key = e.getKeyCode();

            if (key == GLFW_KEY_W) m_camMoveForward = true;
            else if (key == GLFW_KEY_S) m_camMoveBackward = true;
            else if (key == GLFW_KEY_A) m_camMoveLeft = true;
            else if (key == GLFW_KEY_D) m_camMoveRight = true;
            else if (key == GLFW_KEY_UP) m_objMoveZ_neg = true;
            else if (key == GLFW_KEY_DOWN) m_objMoveZ_pos = true;
            else if (key == GLFW_KEY_LEFT) m_objMoveX_neg = true;
            else if (key == GLFW_KEY_RIGHT) m_objMoveX_pos = true;
            else if (key == GLFW_KEY_PAGE_UP) m_objMoveY_pos = true;
            else if (key == GLFW_KEY_PAGE_DOWN) m_objMoveY_neg = true;
            else if (key == GLFW_KEY_KP_8) m_objRotateX_pos = true;
            else if (key == GLFW_KEY_KP_2) m_objRotateX_neg = true;
            else if (key == GLFW_KEY_KP_4) m_objRotateY_pos = true;
            else if (key == GLFW_KEY_KP_6) m_objRotateY_neg = true;
            else if (key == GLFW_KEY_KP_7) m_objRotateZ_pos = true;
            else if (key == GLFW_KEY_KP_9) m_objRotateZ_neg = true;
            else if (key == GLFW_KEY_KP_ADD || key == GLFW_KEY_EQUAL) m_objScaleUp = true;
            else if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS) m_objScaleDown = true;
            else {
                bool handled = false;
                for (int i = 0; i < 9 && static_cast<size_t>(i) < m_scenePrimitives.size(); ++i) {
                    if (key == (GLFW_KEY_1 + i)) {
                        m_currentSelection = ActiveSelectionType::PRIMITIVE;
                        m_activePrimitiveIndex = i;
                        m_activeModelIndex = -1; m_activePointLightIndex = -1; m_activeSpotLightIndex = -1;
                        Logger::getInstance().info("Wybrano prymityw: " + std::to_string(i + 1));
                        handled = true; break;
                    }
                }
                if (!handled && m_scenePrimitives.size() >= 10 && key == GLFW_KEY_0) {
                    if (static_cast<size_t>(9) < m_scenePrimitives.size()) {
                        m_currentSelection = ActiveSelectionType::PRIMITIVE;
                        m_activePrimitiveIndex = 9;
                        m_activeModelIndex = -1; m_activePointLightIndex = -1; m_activeSpotLightIndex = -1;
                        Logger::getInstance().info("Wybrano prymityw: 10");
                        handled = true;
                    }
                }
                if (!handled && key == GLFW_KEY_F11 && !m_sceneModels.empty()) {
                    m_currentSelection = ActiveSelectionType::MODEL;
                    m_activeModelIndex = (m_activeModelIndex + 1) % m_sceneModels.size();
                    m_activePrimitiveIndex = -1; m_activePointLightIndex = -1; m_activeSpotLightIndex = -1;
                    Logger::getInstance().info("Wybrano model: " + m_sceneModels[m_activeModelIndex]->getName() + " (Index: " + std::to_string(m_activeModelIndex) + ")");
                    handled = true;
                }

                if (!handled && key == GLFW_KEY_F2 && m_lightingManager && !m_lightingManager->getPointLights().empty()) {
                    m_currentSelection = ActiveSelectionType::POINT_LIGHT;
                    m_activePointLightIndex = (m_activePointLightIndex + 1) % m_lightingManager->getPointLights().size();
                    m_activePrimitiveIndex = -1; m_activeModelIndex = -1; m_activeSpotLightIndex = -1;
                    Logger::getInstance().info("Wybrano swiatlo punktowe: " + std::to_string(m_activePointLightIndex + 1));
                    handled = true;
                }
                if (!handled && key == GLFW_KEY_F3 && m_lightingManager && !m_lightingManager->getSpotLights().empty()) {
                    m_currentSelection = ActiveSelectionType::SPOT_LIGHT;
                    m_activeSpotLightIndex = (m_activeSpotLightIndex + 1) % m_lightingManager->getSpotLights().size();
                    m_activePrimitiveIndex = -1; m_activeModelIndex = -1; m_activePointLightIndex = -1;
                    Logger::getInstance().info("Wybrano reflektor: " + std::to_string(m_activeSpotLightIndex + 1));
                    handled = true;
                }
                if (!handled && key == GLFW_KEY_F4) {
                    m_currentSelection = ActiveSelectionType::DIR_LIGHT;
                    m_activePrimitiveIndex = -1; m_activeModelIndex = -1; m_activePointLightIndex = -1; m_activeSpotLightIndex = -1;
                    Logger::getInstance().info("Wybrano swiatlo kierunkowe.");
                    handled = true;
                }
                if (!handled && key == GLFW_KEY_L && m_lightingManager) {
                    DirectionalLight& dirLight = m_lightingManager->getDirectionalLight();
                    dirLight.enabled = !dirLight.enabled;
                    Logger::getInstance().info("Swiatlo kierunkowe: " + std::string(dirLight.enabled ? "ON" : "OFF"));
                    handled = true;
                }
                if (!handled && key == GLFW_KEY_F5 && m_lightingManager && !m_lightingManager->getSpotLights().empty() && IGameState::m_engine->getShadowSystem()) {
                    if (m_activeSpotLightIndex != -1) { // If a specific spotlight is selected
                        SpotLight* sl = m_lightingManager->getSpotLight(m_activeSpotLightIndex);
                        if (sl) {
                            sl->castsShadow = !sl->castsShadow;
                            IGameState::m_engine->enableSpotLightShadow(m_activeSpotLightIndex, sl->castsShadow);
                            Logger::getInstance().info("Cien reflektora " + std::to_string(m_activeSpotLightIndex + 1) + ": " + std::string(sl->castsShadow ? "ON" : "OFF"));
                        }
                    }
                    else if (!m_lightingManager->getSpotLights().empty()) { // Default to first spotlight if none selected
                        SpotLight* sl = m_lightingManager->getSpotLight(0);
                        sl->castsShadow = !sl->castsShadow;
                        IGameState::m_engine->enableSpotLightShadow(0, sl->castsShadow);
                        Logger::getInstance().info("Cien reflektora 1: " + std::string(sl->castsShadow ? "ON" : "OFF"));
                    }
                    if (m_activeSpotLightIndex == 0 || (m_activeSpotLightIndex == -1 && !m_lightingManager->getSpotLights().empty())) {
                        m_spotLight0_shadow_enabled = m_lightingManager->getSpotLights()[0].castsShadow;
                    }
                    handled = true;
                }
                if (!handled && key == GLFW_KEY_F6 && m_lightingManager && !m_lightingManager->getPointLights().empty() && IGameState::m_engine->getShadowSystem()) {
                    if (m_activePointLightIndex != -1) { // If a specific pointlight is selected
                        PointLight* pl = m_lightingManager->getPointLight(m_activePointLightIndex);
                        if (pl) {
                            pl->castsShadow = !pl->castsShadow;
                            IGameState::m_engine->enablePointLightShadow(m_activePointLightIndex, pl->castsShadow);
                            Logger::getInstance().info("Cien sw. punktowego " + std::to_string(m_activePointLightIndex + 1) + ": " + std::string(pl->castsShadow ? "ON" : "OFF"));
                        }
                    }
                    else if (!m_lightingManager->getPointLights().empty()) { // Default to first pointlight if none selected
                        PointLight* pl = m_lightingManager->getPointLight(0);
                        pl->castsShadow = !pl->castsShadow;
                        IGameState::m_engine->enablePointLightShadow(0, pl->castsShadow);
                        Logger::getInstance().info("Cien sw. punktowego 1: " + std::string(pl->castsShadow ? "ON" : "OFF"));
                    }
                    if (m_activePointLightIndex == 0 || (m_activePointLightIndex == -1 && !m_lightingManager->getPointLights().empty())) {
                        m_pointLight0_shadow_enabled = m_lightingManager->getPointLights()[0].castsShadow;
                    }
                    handled = true;
                }
                if (handled) e.handled = true;
            }

        }
        else if (event.getEventType() == EventType::KeyReleased) {
            KeyReleasedEvent& e = static_cast<KeyReleasedEvent&>(event);
            int key = e.getKeyCode();
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
            else if (key == GLFW_KEY_KP_8) m_objRotateX_pos = false;
            else if (key == GLFW_KEY_KP_2) m_objRotateX_neg = false;
            else if (key == GLFW_KEY_KP_4) m_objRotateY_pos = false;
            else if (key == GLFW_KEY_KP_6) m_objRotateY_neg = false;
            else if (key == GLFW_KEY_KP_7) m_objRotateZ_pos = false;
            else if (key == GLFW_KEY_KP_9) m_objRotateZ_neg = false;
            else if (key == GLFW_KEY_KP_ADD || key == GLFW_KEY_EQUAL) m_objScaleUp = false;
            else if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS) m_objScaleDown = false;

        }
        else if (event.getEventType() == EventType::MouseMoved) {
            MouseMovedEvent& e = static_cast<MouseMovedEvent&>(event);
            if (m_mouseCaptured && m_camera && IGameState::m_engine->getInputManager()) {
                glm::vec2 currentMousePos = e.getPosition();
                if (m_firstMouse) {
                    m_lastMousePos = currentMousePos;
                    m_firstMouse = false;
                }
                glm::vec2 offset = currentMousePos - m_lastMousePos;
                m_lastMousePos = currentMousePos;
                if (offset.x != 0 || offset.y != 0) {
                    m_camera->processMouseMovement(offset.x, -offset.y);
                }
            }
        }
        else if (event.getEventType() == EventType::MouseScrolled) {
            MouseScrolledEvent& e = static_cast<MouseScrolledEvent&>(event);
            if (m_camera) {
                m_camera->processMouseScroll(e.getYOffset());
            }
        }
    }

    void handleEvents(InputManager* inputManager, EventManager* eventManager) override {
        if (!inputManager) return;
        if (inputManager->isKeyPressed(GLFW_KEY_ESCAPE)) {
            m_triggerExitToMenu = true;
        }
        if (inputManager->isKeyPressed(GLFW_KEY_M)) {
            bool currentCaptureState = inputManager->isMouseCaptured();
            m_mouseCaptured = !currentCaptureState;
            inputManager->setMouseCapture(m_mouseCaptured);
            Logger::getInstance().info("Mouse capture toggled via M key to: " + std::string(m_mouseCaptured ? "ON" : "OFF"));
            m_firstMouse = true;
            if (m_mouseCaptured) {
                m_lastMousePos = inputManager->getMousePosition();
            }
        }
        if (inputManager->isKeyPressed(GLFW_KEY_F1)) {
            IGameState::m_engine->toggleFPSDisplay();
        }
    }

    void update(float deltaTime) override;

    void applyTransformationsToSelected(float deltaTime) {
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
        glm::vec3 finalRotAxis = glm::vec3(0.0f, 1.0f, 0.0f);

        if (glm::length(rotAxisAccumulator) > 0.001f) {
            finalRotAxis = glm::normalize(rotAxisAccumulator);
            actualRotAngleDeg = m_rotationSpeed * deltaTime;
        }

        float scaleChangeFactor = 1.0f;
        if (m_objScaleUp) scaleChangeFactor *= (1.0f + (m_scaleSpeed * deltaTime));
        if (m_objScaleDown) scaleChangeFactor /= (1.0f + (m_scaleSpeed * deltaTime));
        glm::vec3 scaleChangeVec(scaleChangeFactor);


        glm::vec3 actualMove = glm::vec3(0.0f);
        if (glm::length(moveDir) > 0.001f) {
            actualMove = glm::normalize(moveDir) * m_moveSpeed * deltaTime;
        }

        switch (m_currentSelection) {
        case ActiveSelectionType::PRIMITIVE:
            if (m_activePrimitiveIndex != -1 && static_cast<size_t>(m_activePrimitiveIndex) < m_scenePrimitives.size()) {
                BasePrimitive* prim = m_scenePrimitives[m_activePrimitiveIndex].get();
                if (glm::length(actualMove) > 0.001f) prim->translate(actualMove);
                if (actualRotAngleDeg != 0.0f) prim->rotate(actualRotAngleDeg, finalRotAxis);
                if (std::abs(scaleChangeFactor - 1.0f) > 0.0001f && scaleChangeFactor > 0.0f) {
                    prim->scale(scaleChangeVec);
                }
            }
            break;
        case ActiveSelectionType::MODEL:
            if (m_activeModelIndex != -1 && static_cast<size_t>(m_activeModelIndex) < m_sceneModels.size()) {
                Model* model = m_sceneModels[m_activeModelIndex].get();
                if (glm::length(actualMove) > 0.001f) model->setPosition(model->getPosition() + actualMove);
                if (actualRotAngleDeg != 0.0f) {
                    glm::quat currentRot = model->getRotation();
                    glm::quat rotChange = glm::angleAxis(glm::radians(actualRotAngleDeg), finalRotAxis);
                    model->setRotation(rotChange * currentRot);
                }
                if (std::abs(scaleChangeFactor - 1.0f) > 0.0001f && scaleChangeFactor > 0.0f) {
                    model->setScale(model->getScale() * scaleChangeFactor);
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
                    if (actualRotAngleDeg != 0.0f) {
                        glm::mat4 rotationMat = glm::rotate(glm::mat4(1.0f), glm::radians(actualRotAngleDeg), finalRotAxis);
                        sLight->direction = glm::normalize(glm::vec3(rotationMat * glm::vec4(sLight->direction, 0.0f)));
                    }
                }
            }
            break;
        case ActiveSelectionType::DIR_LIGHT:
            if (m_lightingManager) {
                DirectionalLight& dLight = m_lightingManager->getDirectionalLight();
                if (actualRotAngleDeg != 0.0f) {
                    glm::mat4 rotationMat = glm::rotate(glm::mat4(1.0f), glm::radians(actualRotAngleDeg), finalRotAxis);
                    dLight.direction = glm::normalize(glm::vec3(rotationMat * glm::vec4(dLight.direction, 0.0f)));
                }
            }
            break;
        case ActiveSelectionType::NONE:
            break;
        }
    }

    void renderSelectionInstructions() {
        if (!m_textRenderer || !m_textRenderer->isInitialized() || !IGameState::m_engine) return;

        float windowWidth = static_cast<float>(IGameState::m_engine->getWindowWidth());
        float windowHeight = static_cast<float>(IGameState::m_engine->getWindowHeight());
        float xPos = windowWidth - 420.0f;
        float yPosStart = windowHeight - 20.0f;
        float lineHeight = 18.0f;
        float scale = 0.6f;
        glm::vec3 textColor(0.9f, 0.9f, 0.9f);

        std::vector<std::string> instructions;
        instructions.push_back("--- Sterowanie Ogolne ---");
        instructions.push_back("Kamera: WASD, Mysz (M: tryb myszy)");
        instructions.push_back("Przelacz FPS: F1 | Wyjscie: Menu (ESC)");
        instructions.push_back("--- Wybor Elementu ---");
        instructions.push_back("Prymityw: 1-" + std::to_string(std::min((size_t)9, m_scenePrimitives.size())) + (m_scenePrimitives.size() >= 10 ? " (0 dla 10.)" : ""));
        if (!m_sceneModels.empty()) {
            instructions.push_back("Model: F11 (cyklicznie)");
        }
        instructions.push_back("Swiatlo Pkt: F2 | Reflektor: F3");
        instructions.push_back("Swiatlo Kier.: F4");
        instructions.push_back("--- Zarzadzanie Swiatlami ---");
        instructions.push_back(std::string("Prymityw: 1-") + std::to_string(std::min((size_t)9, m_scenePrimitives.size())) + (m_scenePrimitives.size() >= 10 ? " (0 dla 10.)" : ""));


        std::string spotShadowStatus = "OFF";
        if (m_lightingManager && !m_lightingManager->getSpotLights().empty()) {
            int shadowIdx = (m_activeSpotLightIndex != -1) ? m_activeSpotLightIndex : 0; // Default to 0 if none selected but list not empty
            if (static_cast<size_t>(shadowIdx) < m_lightingManager->getSpotLights().size()) { // Ensure index is valid
                spotShadowStatus = m_lightingManager->getSpotLights()[shadowIdx].castsShadow ? "ON" : "OFF";
            }
        }
        instructions.push_back("Cien Reflektora (" + std::to_string((m_activeSpotLightIndex != -1 ? m_activeSpotLightIndex : 0) + 1) + "): F5 (" + spotShadowStatus + ")");

        std::string pointShadowStatus = "OFF";
        if (m_lightingManager && !m_lightingManager->getPointLights().empty()) {
            int shadowIdx = (m_activePointLightIndex != -1) ? m_activePointLightIndex : 0; // Default to 0
            if (static_cast<size_t>(shadowIdx) < m_lightingManager->getPointLights().size()) { // Ensure index is valid
                pointShadowStatus = m_lightingManager->getPointLights()[shadowIdx].castsShadow ? "ON" : "OFF";
            }
        }
        instructions.push_back("Cien Sw. Pkt. (" + std::to_string((m_activePointLightIndex != -1 ? m_activePointLightIndex : 0) + 1) + "): F6 (" + pointShadowStatus + ")");

        instructions.push_back("--- Transformacje Wybranego ---");
        instructions.push_back("Ruch (XZ): Strzalki | Ruch (Y): PgUp/PgDn");
        instructions.push_back("Obrot X: NP8/NP2 | Obrot Y: NP4/NP6");
        instructions.push_back("Obrot Z: NP7/NP9");
        if (m_currentSelection == ActiveSelectionType::PRIMITIVE || m_currentSelection == ActiveSelectionType::MODEL) {
            instructions.push_back("Skala: NP+ / NP- (lub +/-)");
        }
        instructions.push_back("--- Aktywny Element ---");
        std::string activeElementStr = selectionTypeToString(m_currentSelection);
        if (m_currentSelection == ActiveSelectionType::PRIMITIVE && m_activePrimitiveIndex != -1) activeElementStr += " " + std::to_string(m_activePrimitiveIndex + 1);
        else if (m_currentSelection == ActiveSelectionType::MODEL && m_activeModelIndex != -1 && static_cast<size_t>(m_activeModelIndex) < m_sceneModels.size()) activeElementStr += " " + m_sceneModels[m_activeModelIndex]->getName();
        else if (m_currentSelection == ActiveSelectionType::POINT_LIGHT && m_activePointLightIndex != -1 && m_lightingManager && !m_lightingManager->getPointLights().empty()) activeElementStr += " " + std::to_string(m_activePointLightIndex + 1);
        else if (m_currentSelection == ActiveSelectionType::SPOT_LIGHT && m_activeSpotLightIndex != -1 && m_lightingManager && !m_lightingManager->getSpotLights().empty()) activeElementStr += " " + std::to_string(m_activeSpotLightIndex + 1);
        instructions.push_back(activeElementStr);

        for (size_t i = 0; i < instructions.size(); ++i) {
            m_textRenderer->renderText(instructions[i], xPos, yPosStart - (i * lineHeight), scale, textColor);
        }
    }

    void render(Renderer* rendererFromEngine) override {
        if (!m_camera || !IGameState::m_engine) return;

        glm::mat4 projectionMatrix = m_camera->getProjectionMatrix();
        glm::mat4 viewMatrix = m_camera->getViewMatrix();

        // Helper lambda to set lighting uniforms (excluding shadow maps, viewPos)
        auto setBasicLightUniforms = [&](std::shared_ptr<Shader> shaderToConfigure) {
            if (!shaderToConfigure) return;
            shaderToConfigure->use(); // Ensure shader is active before setting uniforms

            if (m_lightingManager) {
                // Directional Light
                const DirectionalLight& dirLight = m_lightingManager->getDirectionalLight();
                shaderToConfigure->setBool("dirLight.enabled", dirLight.enabled);
                if (dirLight.enabled) {
                    shaderToConfigure->setVec3("dirLight.direction", dirLight.direction);
                    shaderToConfigure->setVec3("dirLight.ambient", dirLight.ambient);
                    shaderToConfigure->setVec3("dirLight.diffuse", dirLight.diffuse);
                    shaderToConfigure->setVec3("dirLight.specular", dirLight.specular);
                }

                // Point Lights
                const auto& pointLights = m_lightingManager->getPointLights();
                int numPointLightsToSet = std::min((int)pointLights.size(), 4); // Max 4 point lights in shader
                shaderToConfigure->setInt("numPointLights", numPointLightsToSet);
                for (int i = 0; i < numPointLightsToSet; ++i) {
                    std::string prefix = "pointLights[" + std::to_string(i) + "].";
                    shaderToConfigure->setBool(prefix + "enabled", pointLights[i].enabled);
                    if (pointLights[i].enabled) {
                        shaderToConfigure->setVec3(prefix + "position", pointLights[i].position);
                        shaderToConfigure->setVec3(prefix + "ambient", pointLights[i].ambient);
                        shaderToConfigure->setVec3(prefix + "diffuse", pointLights[i].diffuse);
                        shaderToConfigure->setVec3(prefix + "specular", pointLights[i].specular);
                        shaderToConfigure->setFloat(prefix + "constant", pointLights[i].constant);
                        shaderToConfigure->setFloat(prefix + "linear", pointLights[i].linear);
                        shaderToConfigure->setFloat(prefix + "quadratic", pointLights[i].quadratic);
                        shaderToConfigure->setBool(prefix + "castsShadow", pointLights[i].castsShadow);
                        shaderToConfigure->setInt(prefix + "shadowDataIndex", pointLights[i].shadowDataIndex);
                    }
                }
                for (int i = numPointLightsToSet; i < 4; ++i) { // Disable unused slots
                    shaderToConfigure->setBool("pointLights[" + std::to_string(i) + "].enabled", false);
                }


                // Spot Lights
                const auto& spotLights = m_lightingManager->getSpotLights();
                int numSpotLightsToSet = std::min((int)spotLights.size(), 4); // Max 4 spot lights
                shaderToConfigure->setInt("numSpotLights", numSpotLightsToSet);
                for (int i = 0; i < numSpotLightsToSet; ++i) {
                    std::string prefix = "spotLights[" + std::to_string(i) + "].";
                    shaderToConfigure->setBool(prefix + "enabled", spotLights[i].enabled);
                    if (spotLights[i].enabled) {
                        shaderToConfigure->setVec3(prefix + "position", spotLights[i].position);
                        shaderToConfigure->setVec3(prefix + "direction", spotLights[i].direction);
                        shaderToConfigure->setVec3(prefix + "ambient", spotLights[i].ambient);
                        shaderToConfigure->setVec3(prefix + "diffuse", spotLights[i].diffuse);
                        shaderToConfigure->setVec3(prefix + "specular", spotLights[i].specular);
                        shaderToConfigure->setFloat(prefix + "constant", spotLights[i].constant);
                        shaderToConfigure->setFloat(prefix + "linear", spotLights[i].linear);
                        shaderToConfigure->setFloat(prefix + "quadratic", spotLights[i].quadratic);
                        shaderToConfigure->setFloat(prefix + "cutOff", spotLights[i].cutOff);
                        shaderToConfigure->setFloat(prefix + "outerCutOff", spotLights[i].outerCutOff);
                        shaderToConfigure->setBool(prefix + "castsShadow", spotLights[i].castsShadow);
                        shaderToConfigure->setInt(prefix + "shadowDataIndex", spotLights[i].shadowDataIndex);
                    }
                }
                for (int i = numSpotLightsToSet; i < 4; ++i) { // Disable unused slots
                    shaderToConfigure->setBool("spotLights[" + std::to_string(i) + "].enabled", false);
                }
            }

            if (IGameState::m_engine) {
                shaderToConfigure->setInt("pcfRadius", IGameState::m_engine->getPCFQuality());
            }
            // viewPos_World is set by the individual render() calls of primitives/models
            // Shadow map samplers and matrices are assumed to be globally set by Engine/ShadowSystem
            };

        // Render Primitives
        for (const auto& prim_ptr : m_scenePrimitives) {
            if (prim_ptr) {
                BasePrimitive* prim = prim_ptr.get();
                std::shared_ptr<Shader> shader = prim->getShader();
                if (shader) { // Only proceed if shader is valid
                    setBasicLightUniforms(shader);
                    // prim->render will set model matrix, material, textures, viewPos and draw
                    prim->render(viewMatrix, projectionMatrix, m_camera);
                }
                else {
                    Logger::getInstance().warning("GameplayState: Primitive missing shader, skipping its rendering.");
                }
            }
        }

        // Render Models
        for (const auto& model_ptr : m_sceneModels) {
            if (model_ptr) {
                Model* model = model_ptr.get();
                std::shared_ptr<Shader> shader = model->getShader();
                if (shader) { // Only proceed if shader is valid
                    setBasicLightUniforms(shader);
                    // model->render will set model matrix, material, textures, viewPos (if camera set) and draw
                    model->render(viewMatrix, projectionMatrix);
                }
                else {
                    Logger::getInstance().warning("GameplayState: Model '" + model->getName() + "' missing shader, skipping its rendering.");
                }
            }
        }

        renderSelectionInstructions();
        if (m_textRenderer && m_textRenderer->isInitialized()) {
            float yPos = static_cast<float>(IGameState::m_engine->getWindowHeight()) - 60.0f;
            if (!m_mouseCaptured) {
                m_textRenderer->renderText("Mysz uwolniona. Nacisnij M aby przechwycic.", 10.0f, yPos - 250.0f, 0.7f, glm::vec3(0.9f, 0.9f, 0.1f));
            }
        }
    }

private:
    Camera* m_camera;
    LightingManager* m_lightingManager;
    TextRenderer* m_textRenderer;
    std::vector<std::unique_ptr<BasePrimitive>> m_scenePrimitives;
    std::vector<std::unique_ptr<Model>> m_sceneModels;

    ActiveSelectionType m_currentSelection;
    int m_activePrimitiveIndex;
    int m_activeModelIndex;
    int m_activePointLightIndex;
    int m_activeSpotLightIndex;

    bool m_spotLight0_shadow_enabled;
    bool m_pointLight0_shadow_enabled;

    bool m_camMoveForward;
    bool m_camMoveBackward;
    bool m_camMoveLeft;
    bool m_camMoveRight;

    bool m_objMoveZ_neg, m_objMoveZ_pos, m_objMoveX_neg, m_objMoveX_pos, m_objMoveY_pos, m_objMoveY_neg;
    bool m_objRotateX_pos, m_objRotateX_neg, m_objRotateY_pos, m_objRotateY_neg, m_objRotateZ_pos, m_objRotateZ_neg;
    bool m_objScaleUp, m_objScaleDown;

    float m_moveSpeed;
    float m_rotationSpeed;
    float m_scaleSpeed;

    bool m_mouseCaptured;
    bool m_triggerExitToMenu;

    glm::vec2 m_lastMousePos;
    bool m_firstMouse;
};

// Definicje metod update
void MenuState::update(float deltaTime) {
    if (m_triggerStartGame) {
        Logger::getInstance().info("MenuState: Changing to GameplayState.");
        IGameState::m_engine->getGameStateManager()->changeState(std::make_unique<GameplayState>());
        m_triggerStartGame = false;
        return;
    }
    if (m_triggerExitGame) {
        Logger::getInstance().info("MenuState: Exiting game.");
        if (IGameState::m_engine->getWindow()) {
            glfwSetWindowShouldClose(IGameState::m_engine->getWindow(), 1);
        }
        m_triggerExitGame = false;
    }
}

void GameplayState::update(float deltaTime) {
    if (m_triggerExitToMenu) {
        Logger::getInstance().info("GameplayState: Changing to MenuState.");
        IGameState::m_engine->getGameStateManager()->changeState(std::make_unique<MenuState>());
        m_triggerExitToMenu = false;
        return;
    }

    if (!m_camera) return;

    float camSpeed = m_camera->getMovementSpeed() * deltaTime;
    if (m_camMoveForward) m_camera->processKeyboard(Camera::Direction::Forward, camSpeed);
    if (m_camMoveBackward) m_camera->processKeyboard(Camera::Direction::Backward, camSpeed);
    if (m_camMoveLeft) m_camera->processKeyboard(Camera::Direction::Left, camSpeed);
    if (m_camMoveRight) m_camera->processKeyboard(Camera::Direction::Right, camSpeed);

    applyTransformationsToSelected(deltaTime);
}


// --- Główna funkcja programu ---
int main() {
    Engine* engine = Engine::getInstance();

    if (!engine->initialize(1280, 720, "Test Silnika z GameState i Modelem")) {
        Logger::getInstance().fatal("Nie udalo sie zainicjalizowac silnika!");
        return -1;
    }

    engine->setVSync(true);
    engine->setBackgroundColor(0.1f, 0.12f, 0.15f, 1.0f);
    engine->setAutoSwap(true);
    engine->toggleFPSDisplay();

    ResourceManager::getInstance().loadShader("lightingShader", "assets/shaders/default_shader.vert", "assets/shaders/default_shader.frag");

    if (engine->getGameStateManager()) {
        engine->getGameStateManager()->pushState(std::make_unique<MenuState>());
    }
    else {
        Logger::getInstance().fatal("GameStateManager is null after engine initialization!");
        engine->shutdown();
        return -1;
    }

    Logger::getInstance().info("Rozpoczynanie petli glownej...");

    while (!engine->shouldClose()) {
        engine->update();
        engine->render();
    }

    Logger::getInstance().info("Czyszczenie zasobow...");
    engine->shutdown();

    Logger::getInstance().info("Aplikacja zakonczona.");
    return 0;
}
