#include "Camera.h"
#include "Logger.h" // Dla logowania, jesli potrzebne
#include <glm/gtc/matrix_transform.hpp> // Dla glm::lookAt, glm::perspective, glm::ortho
#include <cmath>     // Dla std::cos, std::sin, std::abs
#include <string>    // Dla std::string, std::to_string (uzywane w zakomentowanych logach)
#include <algorithm> 


// Konstruktor kamery
Camera::Camera(glm::vec3 startPosition, glm::vec3 upDirection, float startYaw, float startPitch, float startFov, ProjectionType type)
    : m_position(startPosition),       // Inicjalizacja pozycji
    m_worldUp(glm::normalize(upDirection)), // Inicjalizacja globalnego wektora "w gore" (zawsze znormalizowany)
    m_yaw(startYaw),                 // Inicjalizacja katu odchylenia
    m_pitch(startPitch),             // Inicjalizacja katu pochylenia
    m_fov(startFov),                 // Inicjalizacja pola widzenia
    m_aspectRatio(16.0f / 9.0f),     // Domyslny stosunek szerokosci do wysokosci (16:9)
    m_nearPlane(0.1f),               // Domyslna bliska plaszczyzna przycinania
    m_farPlane(100.0f),              // Domyslna daleka plaszczyzna przycinania
    m_projectionType(type),          // Inicjalizacja typu projekcji
    m_movementSpeed(2.5f),           // Domyslna predkosc poruszania sie
    m_mouseSensitivity(0.1f)         // Domyslna czulosc myszy
{
    // Obliczenie poczatkowych wektorow kierunkowych kamery (front, right, up)
    // na podstawie poczatkowych katow yaw i pitch.
    updateCameraVectors();
    Logger::getInstance().info("Kamera utworzona i zainicjalizowana.");
}

// Aktualizacja wektorow kierunkowych kamery (front, right, up)
// na podstawie aktualnych katow yaw i pitch.
void Camera::updateCameraVectors() {
    // Obliczenie nowego wektora "do przodu" (front) na podstawie katow Eulera.
    // Konwersja katow ze stopni na radiany jest niezbedna dla funkcji trygonometrycznych.
    glm::vec3 newFront;
    newFront.x = std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    newFront.y = std::sin(glm::radians(m_pitch));
    newFront.z = std::sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    m_front = glm::normalize(newFront); // Normalizacja wektora front

    // Obliczenie wektora "w prawo" (right) jako iloczynu wektorowego
    // wektora front i globalnego wektora "w gore" (worldUp).
    // Normalizacja jest wazna, aby uniknac problemow ze skalowaniem.
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));

    // Obliczenie wektora "w gore" (up) w lokalnym ukladzie kamery
    // jako iloczynu wektorowego wektorow right i front.
    m_up = glm::normalize(glm::cross(m_right, m_front));
}

// Zwraca macierz widoku (View Matrix)
glm::mat4 Camera::getViewMatrix() const {
    // Macierz widoku transformuje wspolrzedne ze swiata (world space)
    // do przestrzeni widoku (view space lub eye space).
    // Funkcja glm::lookAt tworzy macierz widoku na podstawie:
    // - pozycji kamery (m_position)
    // - punktu, na ktory kamera patrzy (m_position + m_front)
    // - wektora "w gore" (m_up) w lokalnym ukladzie kamery
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

// Zwraca macierz projekcji (Projection Matrix)
glm::mat4 Camera::getProjectionMatrix() const {
    // Macierz projekcji definiuje, jak scena 3D jest rzutowana na plaszczyzne 2D obrazu.
    if (m_projectionType == ProjectionType::Perspective) {
        // Projekcja perspektywiczna - obiekty dalej sa mniejsze.
        // Wymaga pola widzenia (FOV), stosunku szerokosci do wysokosci (aspect ratio)
        // oraz odleglosci do bliskiej i dalekiej plaszczyzny przycinania.
        return glm::perspective(glm::radians(m_fov), m_aspectRatio, m_nearPlane, m_farPlane);
    }
    else { // ProjectionType::Orthographic
        // Projekcja ortogonalna (rownolegla) - rozmiar obiektow nie zalezy od odleglosci.
        // Definiowana przez granice prostopadloscianu widzenia (lewa, prawa, dolna, gorna, bliska, daleka).
        // Rozmiar obszaru ortogonalnego mozna dostosowac; tutaj prosty przyklad.
        // TODO: Uczynic parametry projekcji ortogonalnej bardziej konfigurowalnymi.
        float orthoHeight = 10.0f; // Przykładowa wysokość obszaru widzenia
        float orthoWidth = orthoHeight * m_aspectRatio; // Szerokosc obliczona na podstawie proporcji
        return glm::ortho(-orthoWidth / 2.0f, orthoWidth / 2.0f,
            -orthoHeight / 2.0f, orthoHeight / 2.0f,
            m_nearPlane, m_farPlane);
    }
}

// Przetwarzanie wejscia z klawiatury do poruszania kamera
void Camera::processKeyboard(Direction direction, float deltaTime) {
    // Obliczenie predkosci ruchu w biezacej klatce.
    // deltaTime zapewnia, ze ruch jest niezalezny od liczby klatek na sekunde.
    float velocity = m_movementSpeed * deltaTime;

    // Aktualizacja pozycji kamery w zaleznosci od kierunku.
    if (direction == Direction::Forward)
        m_position += m_front * velocity; // Ruch do przodu wzdluz wektora front
    if (direction == Direction::Backward)
        m_position -= m_front * velocity; // Ruch do tylu
    if (direction == Direction::Left)
        m_position -= m_right * velocity; // Ruch w lewo wzdluz wektora right
    if (direction == Direction::Right)
        m_position += m_right * velocity; // Ruch w prawo
    if (direction == Direction::Up)
        // Ruch w gore. Mozna uzyc m_worldUp dla ruchu scisle pionowego w przestrzeni swiata,
        // lub m_up dla ruchu "w gore" wzgledem orientacji kamery.
        // Uzycie m_worldUp jest czesto bardziej intuicyjne dla trybu "no-clip" lub "fly-mode".
        m_position += m_worldUp * velocity;
    if (direction == Direction::Down)
        m_position -= m_worldUp * velocity; // Ruch w dol
}

// Przetwarzanie ruchu myszy do zmiany orientacji kamery
void Camera::processMouseMovement(float xOffset, float yOffset, bool constrainPitch) {
    // Skalowanie przesuniecia myszy przez czulosc myszy.
    xOffset *= m_mouseSensitivity;
    yOffset *= m_mouseSensitivity;

    // Dodanie przesuniec do katow yaw i pitch.
    m_yaw += xOffset;
    m_pitch += yOffset;

    // Ograniczenie katu pitch, aby uniknac "przewrocenia" kamery (gimbal lock)
    // i patrzenia idealnie w gore lub w dol, co moze powodowac problemy z wektorem 'up'.
    if (constrainPitch) {
        if (m_pitch > 89.0f)
            m_pitch = 89.0f;
        if (m_pitch < -89.0f)
            m_pitch = -89.0f;
    }

    // Aktualizacja wektorow kierunkowych kamery po zmianie katow.
    updateCameraVectors();
}

// Przetwarzanie przewijania rolka myszy do zmiany pola widzenia (FOV), symulujac zoom.
// Dziala tylko dla projekcji perspektywicznej.
void Camera::processMouseScroll(float yOffset) {
    // Zmiana FOV na podstawie wartosci przewiniecia rolki.
    // yOffset jest zwykle dodatni dla przewijania "w gore" (oddalanie)
    // i ujemny dla przewijania "w dol" (przyblizanie), wiec odejmujemy.
    m_fov -= yOffset;

    // Ograniczenie FOV do rozsadnego zakresu.
    if (m_fov < 1.0f)
        m_fov = 1.0f;   // Minimalne FOV
    if (m_fov > 90.0f) // Zmieniono z 45.0f na 90.0f dla wiekszego zakresu, zgodnie z oryginalnym kodem było 45.0f, ale 90.0f jest bardziej typowe jako max.
        m_fov = 90.0f;  // Maksymalne FOV (mozna dostosowac)
}

// --- Settery ---

void Camera::setProjectionType(ProjectionType type) {
    m_projectionType = type;
    // Logger::getInstance().info("Kamera: Typ projekcji ustawiony.");
}

void Camera::setAspectRatio(float ratio) {
    if (ratio > 0.0f) { // Stosunek szerokosci do wysokosci musi byc dodatni.
        m_aspectRatio = ratio;
        // Logger::getInstance().info("Kamera: Stosunek szerokosci do wysokosci ustawiony na: " + std::to_string(ratio));
    }
    else {
        Logger::getInstance().warning("Kamera: Proba ustawienia nieprawidlowego stosunku szerokosci do wysokosci: " + std::to_string(ratio));
    }
}

void Camera::setFOV(float fovValue) {
    // Ustawienie pola widzenia (FOV) z ograniczeniem do rozsadnego zakresu.
    if (fovValue >= 1.0f && fovValue <= 120.0f) { // Typowy zakres dla FOV.
        m_fov = fovValue;
        // Logger::getInstance().info("Kamera: FOV ustawione na: " + std::to_string(fovValue));
    }
    else {
        Logger::getInstance().warning("Kamera: Proba ustawienia FOV poza rozsadnym zakresem: " + std::to_string(fovValue));
    }
}

void Camera::setClippingPlanes(float nearClip, float farClip) {
    // Ustawienie plaszczyzn przycinania (bliskiej i dalekiej).
    // Bliska plaszczyzna musi byc dodatnia i mniejsza od dalekiej.
    if (nearClip > 0.0f && farClip > nearClip) {
        m_nearPlane = nearClip;
        m_farPlane = farClip;
        // Logger::getInstance().info("Kamera: Plaszczyzny przycinania ustawione: Bliska=" + std::to_string(nearClip) + ", Daleka=" + std::to_string(farClip));
    }
    else {
        Logger::getInstance().warning("Kamera: Nieprawidlowe wartosci plaszczyzn przycinania.");
    }
}

void Camera::setPosition(const glm::vec3& pos) {
    m_position = pos;
    // Logger::getInstance().info("Kamera: Pozycja ustawiona.");
    // Zmiana pozycji nie wplywa bezposrednio na orientacje, wiec
    // updateCameraVectors() nie jest tutaj generalnie potrzebne, chyba ze
    // logika gry tego wymaga (np. kamera zawsze patrzy na cel po zmianie pozycji).
}

void Camera::setMovementSpeed(float speed) {
    if (speed > 0.0f) { // Predkosc powinna byc dodatnia.
        m_movementSpeed = speed;
        // Logger::getInstance().info("Kamera: Predkosc poruszania ustawiona na: " + std::to_string(speed));
    }
    else {
        Logger::getInstance().warning("Kamera: Proba ustawienia niedodatniej predkosci poruszania.");
    }
}

void Camera::setMouseSensitivity(float sensitivity) {
    if (sensitivity > 0.0f) { // Czulosc powinna byc dodatnia.
        m_mouseSensitivity = sensitivity;
        // Logger::getInstance().info("Kamera: Czulosc myszy ustawiona na: " + std::to_string(sensitivity));
    }
    else {
        Logger::getInstance().warning("Kamera: Proba ustawienia niedodatniej czulosci myszy.");
    }
}

void Camera::setYaw(float yawValue) {
    m_yaw = yawValue;
    updateCameraVectors(); // Zawsze aktualizuj wektory kierunkowe po zmianie katu orientacji.
    // Logger::getInstance().info("Kamera: Kat Yaw ustawiony na: " + std::to_string(yawValue));
}

void Camera::setPitch(float pitchValue) {
    m_pitch = pitchValue;
    // Ograniczenie katu pitch, aby uniknac problemow.
    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;
    updateCameraVectors(); // Zawsze aktualizuj wektory kierunkowe po zmianie katu orientacji.
    // Logger::getInstance().info("Kamera: Kat Pitch ustawiony na: " + std::to_string(pitchValue));
}


// --- Gettery ---
// Proste metody zwracajace wartosci prywatnych skladowych.

glm::vec3 Camera::getPosition() const { return m_position; }
glm::vec3 Camera::getFront() const { return m_front; }
glm::vec3 Camera::getUp() const { return m_up; }
glm::vec3 Camera::getRight() const { return m_right; }
float Camera::getYaw() const { return m_yaw; }
float Camera::getPitch() const { return m_pitch; }
float Camera::getFOV() const { return m_fov; }
float Camera::getMovementSpeed() const { return m_movementSpeed; }
float Camera::getMouseSensitivity() const { return m_mouseSensitivity; }
Camera::ProjectionType Camera::getProjectionType() const { return m_projectionType; }

