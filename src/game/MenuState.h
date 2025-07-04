// Plik: src\game\MenuState.h
#ifndef MENUSTATE_H
#define MENUSTATE_H

#include "IGameState.h"
#include "IEventListener.h"
#include <memory> // Dla std::unique_ptr, std::shared_ptr
#include <vector>
#include <string> // Dodano dla std::string
#include <glm/glm.hpp>

// Deklaracje wyprzedzające
class Engine;
class InputManager;
class EventManager;
class Renderer;
class Camera;
class Model;
class Shader;
class Event;
class Texture; // Dodano deklarację wyprzedzającą dla Texture

/**
 * @class MenuState
 * @brief Stan gry reprezentujący menu główne jako scenę 3D.
 * Scena zawiera biurko, na którym znajduje się clipboard z wymienialnymi kartkami (teksturami).
 * Umożliwia swobodne poruszanie kamerą w trybie debugowania.
 */
class MenuState : public IGameState, public IEventListener {
public:
    MenuState();
    ~MenuState() override;

    // Usunięte konstruktory kopiujące i operatory przypisania
    MenuState(const MenuState&) = delete;
    MenuState& operator=(const MenuState&) = delete;
    MenuState(MenuState&&) = delete;
    MenuState& operator=(MenuState&&) = delete;

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
    Engine* m_engine;                     ///< Wskaźnik do instancji silnika.
    std::unique_ptr<Camera> m_camera;     ///< Kamera dla sceny menu.
    std::shared_ptr<Model> m_deskModel;   ///< Model biurka.
    std::shared_ptr<Model> m_clipboardModel; ///< Model clipboardu.
    std::shared_ptr<Model> m_penModel;       ///< Model długopisu.               //Powinno to być zmienione na vectorka
    std::shared_ptr<Shader> m_modelShader; ///< Shader używany do renderowania modeli.

    // Stan sterowania kamerą i tryb debugowania
    bool m_debugMode;          ///< Czy aktywny jest tryb debugowania (swobodna kamera).
    bool m_camMoveForward;
    bool m_camMoveBackward;
    bool m_camMoveLeft;
    bool m_camMoveRight;
    bool m_camMoveUp;
    bool m_camMoveDown;

    bool m_mouseCaptured;      ///< Czy mysz jest przechwycona (tylko w trybie debugowania).
    glm::vec2 m_lastMousePos;  ///< Ostatnia pozycja myszy (dla obliczania delty).
    bool m_firstMouse;         ///< Flaga dla pierwszej aktualizacji pozycji myszy.

    // Zarządzanie teksturami papieru
    std::vector<std::string> m_paperTexturePaths;       ///< Ścieżki do tekstur papieru.
    std::vector<std::shared_ptr<Texture>> m_paperTextures; ///< Załadowane tekstury papieru.
    int m_currentPaperTextureIndex;                     ///< Indeks aktualnie wyświetlanej tekstury papieru.
    std::vector<size_t> m_clipboardPaperMeshIndices; // Indeksy siatek clipboardu używających tekstury papieru



    // Metody pomocnicze
    void setupCamera();
    void loadAssets();
    void setupLighting();
    void logCameraTransform();
    void applyCurrentPaperTexture(); /// Nowa metoda do zmiany tekstury clipboardu
    void cyclePaperTexture(bool cycleUp); ///< Nowa metoda do przełączania tekstur
};

#endif // MENUSTATE_H