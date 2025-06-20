#ifndef DEMOSTATE_H
#define DEMOSTATE_H

#include "IGameState.h"
#include "IEventListener.h"
#include "Primitives.h" // Dla std::unique_ptr<BasePrimitive>
#include "Model.h"      // Dla std::unique_ptr<Model>
#include <vector>
#include <string>
#include <memory> // Dla std::unique_ptr
#include <glm/glm.hpp>

// Deklaracje wyprzedzające
class Engine;
class InputManager;
class EventManager;
class Renderer;
class Camera;
class LightingManager;
class TextRenderer;
class Shader; // Potrzebne dla std::shared_ptr<Shader> w Model
class MenuState; // Jeśli DemoState ma wracać do MenuState

// Enum do śledzenia, co jest aktualnie wybrane
// Przeniesiony tutaj, ponieważ jest częścią stanu wewnętrznego DemoState
enum class ActiveSelectionType {
    NONE,
    PRIMITIVE,
    MODEL,
    POINT_LIGHT,
    SPOT_LIGHT,
    DIR_LIGHT
};

class DemoState : public IGameState, public IEventListener {
public:
    DemoState();
    ~DemoState() override;

    void init() override;
    void cleanup() override;

    void pause() override;
    void resume() override;

    void handleEvents(InputManager* inputManager, EventManager* eventManager) override;
    void update(float deltaTime) override;
    void render(Renderer* renderer) override;

    // Metoda z IEventListener
    void onEvent(Event& event) override;

private:
    // Główne komponenty silnika
    Camera* m_camera;
    LightingManager* m_lightingManager;
    TextRenderer* m_textRenderer;

    // Obiekty sceny
    std::vector<std::unique_ptr<BasePrimitive>> m_scenePrimitives;
    std::vector<std::unique_ptr<Model>> m_sceneModels;

    // Stan wyboru i aktywne indeksy
    ActiveSelectionType m_currentSelection;
    int m_activePrimitiveIndex;
    int m_activeModelIndex;
    int m_activePointLightIndex;
    int m_activeSpotLightIndex;

    // Flagi stanu (np. cienie, sterowanie)
    bool m_spotLight0_shadow_enabled;
    bool m_pointLight0_shadow_enabled;

    bool m_camMoveForward;
    bool m_camMoveBackward;
    bool m_camMoveLeft;
    bool m_camMoveRight;

    bool m_objMoveZ_neg, m_objMoveZ_pos, m_objMoveX_neg, m_objMoveX_pos, m_objMoveY_pos, m_objMoveY_neg;
    bool m_objRotateX_pos, m_objRotateX_neg, m_objRotateY_pos, m_objRotateY_neg, m_objRotateZ_pos, m_objRotateZ_neg;
    bool m_objScaleUp, m_objScaleDown;

    // Parametry ruchu/transformacji
    float m_moveSpeed;
    float m_rotationSpeed;
    float m_scaleSpeed;

    // Stan myszy i nawigacji
    bool m_mouseCaptured;
    bool m_triggerExitToMenu; // Flaga sygnalizująca chęć powrotu do menu

    glm::vec2 m_lastMousePos;
    bool m_firstMouse;

    // Prywatne metody pomocnicze
    void applyTransformationsToSelected(float deltaTime);
    void renderSelectionInstructions();
};

#endif // DEMOSTATE_H
