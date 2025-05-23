#include "Renderer.h"
#include "Logger.h"
#include "Camera.h"      // Dla getViewMatrix, getProjectionMatrix
#include "IRenderable.h" // Dla render()

#include <algorithm> // Dla std::remove
#include <utility>   // Dla std::move

Renderer::Renderer() : m_camera(nullptr) {
    // Logger::getInstance().info("Renderer utworzony.");
}

Renderer::~Renderer() {
    // clearRenderables() jest wywolywane automatycznie przez std::vector,
    // jesli nie ma specjalnej logiki (np. zwalniania pamieci obiektow).
    // Poniewaz Renderer nie jest wlascicielem, m_renderables.clear() w destruktorze
    // nie jest scisle konieczne, chyba ze dla jawnego wskazania intencji.
    // Logger::getInstance().info("Renderer zniszczony.");
}

Renderer::Renderer(Renderer&& other) noexcept
    : m_renderables(std::move(other.m_renderables)), // Przenies wektor
    m_camera(other.m_camera) {                    // Skopiuj wskaznik
    other.m_camera = nullptr; // Wyzeruj wskaznik w przeniesionym obiekcie
    // other.m_renderables jest juz w stanie "valid but unspecified" po std::move
}

Renderer& Renderer::operator=(Renderer&& other) noexcept {
    if (this != &other) {
        m_renderables = std::move(other.m_renderables); // Przenies wektor
        m_camera = other.m_camera;                    // Skopiuj wskaznik

        other.m_camera = nullptr; // Wyzeruj wskaznik w przeniesionym obiekcie
    }
    return *this;
}

bool Renderer::initialize() {
    if (!m_camera) {
        // Logger::getInstance().warning("Renderer zainicjalizowany bez ustawionej kamery. Zazwyczaj ustawiana przez Engine.");
    }
    // Logger::getInstance().info("Renderer zainicjalizowany.");
    return true;
}

void Renderer::setCamera(Camera* cameraInstance) {
    m_camera = cameraInstance;
    if (m_camera) {
        // Logger::getInstance().info("Kamera ustawiona dla Renderer.");
    }
    else {
        // Logger::getInstance().warning("Ustawiono pusta (null) kamere dla Renderer.");
    }
}

Camera* Renderer::getCamera() const {
    return m_camera;
}

void Renderer::addRenderable(IRenderable* renderable) {
    if (renderable) {
        m_renderables.push_back(renderable);
    }
    else {
        Logger::getInstance().warning("Renderer: Proba dodania obiektu IRenderable wskazujacego na null.");
    }
}

const std::vector<IRenderable*>& Renderer::getRenderables() const {
    return m_renderables;
}

void Renderer::removeRenderable(IRenderable* renderable) {
    if (renderable) {
        // Uzycie idiomu erase-remove do usuniecia elementu z wektora
        auto it = std::remove(m_renderables.begin(), m_renderables.end(), renderable);
        if (it != m_renderables.end()) {
            m_renderables.erase(it, m_renderables.end());
        }
    }
    else {
        // Logger::getInstance().warning("Renderer: Proba usuniecia obiektu IRenderable wskazujacego na null.");
    }
}

void Renderer::clearRenderables() {
    m_renderables.clear(); // Usuwa wszystkie wskazniki z wektora, nie niszczy obiektow IRenderable
}

void Renderer::renderScene() {
    if (!m_camera) {
        Logger::getInstance().error("Renderer::renderScene - Brak ustawionej kamery! Nie mozna renderowac.");
        return;
    }

    // Pobranie macierzy widoku i projekcji z kamery
    // Zakladamy, ze metody te sa const i nie modyfikuja stanu kamery w sposob niepozadany
    const glm::mat4 viewMatrix = m_camera->getViewMatrix();
    const glm::mat4 projectionMatrix = m_camera->getProjectionMatrix();

    // Petla renderowania po wszystkich obiektach renderowalnych
    // W tej petli, kazdy obiekt IRenderable jest odpowiedzialny za wlasne bindowanie shadera,
    // ustawianie uniformow specyficznych dla obiektu (np. macierz modelu)
    // oraz wywolanie odpowiedniej komendy rysujacej (np. glDrawElements).
    // Renderer dostarcza jedynie macierze widoku i projekcji.
    for (IRenderable* renderable : m_renderables) {
        if (renderable) {
            // Wywolanie metody render na obiekcie implementujacym IRenderable.
            // Obiekt ten powinien wewnetrznie obsluzyc proces renderowania
            // (np. poprzez wywolanie BasePrimitive::render, jesli z niego dziedziczy).
            renderable->render(viewMatrix, projectionMatrix);
        }
    }
}

// Usunieto zbedna klamre zamykajaca, jesli byla na koncu pliku.