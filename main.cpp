// Należy zdefiniować PRZED jakimkolwiek include'em GLM
#define GLM_ENABLE_EXPERIMENTAL

#include <glad/glad.h>
#include "Engine.h"
#include "InputManager.h"     // Potrzebny dla GameStateManager i Stanów Gry
#include "EventManager.h"     // Potrzebny dla GameStateManager i Stanów Gry
#include "Event.h"            // Potrzebny dla EventManager i IEventListener
#include "IEventListener.h"   // Potrzebny, jeśli stany implementują ten interfejs
#include "Renderer.h"         // Potrzebny dla GameStateManager i Stanów Gry
#include "Camera.h"           // Potrzebny dla Stanów Gry
#include "LightingManager.h"  // Potrzebny dla Stanów Gry
#include "TextRenderer.h"     // Potrzebny dla Stanów Gry
#include "GameStateManager.h" 
#include "Logger.h"         
#include "IGameState.h"       
#include "Shader.h" 
#include "ResourceManager.h" 
#include "Texture.h"         
#include "Model.h"           
#include "Primitives.h"       // Potrzebny dla DemoState
#include "BoundingVolume.h"   // Potrzebny dla DemoState/Primitives

// Nowe dołączenia dla stanów gry
#include "MenuState.h"      // Nowy MenuState z osobnego pliku
#include "DemoState.h"      // Nowy DemoState (wcześniej GameplayState) z osobnego pliku
// #include "TechDemoState.h" // Odkomentuj, jeśli TechDemoState jest używany i zdefiniowany

#include <vector>
#include <string>
#include <memory> 
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp> // Może być używane w stanach
#include <glm/gtx/transform.hpp>     // Może być używane w stanach
#include <algorithm> 
#include <cmath>     
#include <iomanip> 

// Usunięto definicje klas MenuState i GameplayState, ponieważ są teraz w osobnych plikach.
// Usunięto enum ActiveSelectionType i funkcję selectionTypeToString,
// ponieważ zostały przeniesione do DemoState.h/DemoState.cpp.

// --- Główna funkcja programu ---
int main() {
    // Pobranie instancji silnika (Singleton)
    Engine* engine = Engine::getInstance();

    // Inicjalizacja silnika z określonymi wymiarami okna i tytułem
    if (!engine->initialize(1280, 720, "Silnik PGK Demo")) {
        Logger::getInstance().fatal("Nie udalo sie zainicjalizowac silnika!");
        return -1; // Zakończ program, jeśli inicjalizacja silnika nie powiodła się
    }

    // Ustawienia silnika
    engine->setVSync(true); // Włącz synchronizację pionową
    engine->setBackgroundColor(0.1f, 0.12f, 0.15f, 1.0f); // Ustaw kolor tła
    engine->setAutoSwap(true); // Włącz automatyczną zamianę buforów
    engine->toggleFPSDisplay(); // Włącz wyświetlanie licznika FPS

    // Załadowanie podstawowych zasobów, jeśli są potrzebne globalnie lub przez wiele stanów.
    // W tym przypadku, shader "lightingShader" jest ładowany, ponieważ może być używany przez DemoState.
    ResourceManager::getInstance().loadShader("lightingShader", "assets/shaders/default_shader.vert", "assets/shaders/default_shader.frag");
    // Inne globalne zasoby można ładować tutaj

    // Sprawdzenie, czy GameStateManager został poprawnie zainicjalizowany w silniku
    if (engine->getGameStateManager()) {
        // Ustawienie początkowego stanu gry na MenuState
        // MenuState jest teraz ładowane z osobnego pliku MenuState.h/MenuState.cpp
        engine->getGameStateManager()->pushState(std::make_unique<MenuState>());
    }
    else {
        Logger::getInstance().fatal("GameStateManager jest null po inicjalizacji silnika!");
        engine->shutdown(); // Poprawne zamknięcie silnika
        return -1; // Zakończ program
    }

    Logger::getInstance().info("Rozpoczynanie petli glownej...");

    // Główna pętla silnika
    while (!engine->shouldClose()) {
        engine->update(); // Aktualizacja logiki silnika i aktywnego stanu gry
        engine->render(); // Renderowanie sceny
    }

    Logger::getInstance().info("Czyszczenie zasobow i wylaczanie silnika...");
    engine->shutdown(); // Zwolnienie zasobów i zamknięcie silnika

    Logger::getInstance().info("Aplikacja zakonczona.");
    return 0; // Zakończ program pomyślnie
}
