/**
* @file Camera.h
* @brief Definicja klasy Camera.
*
* Plik ten zawiera deklarację klasy Camera, która zarządza
* pozycją i orientacją kamery w świecie gry oraz macierzami
* widoku i projekcji.
*/
#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp> // Podstawowe typy GLM (wektory, macierze)
#include <glm/gtc/matrix_transform.hpp> // Dla glm::lookAt, glm::perspective, glm::ortho

/**
 * @class Camera
 * @brief Reprezentuje kamere wirtualna w scenie 3D.
 *
 * Odpowiada za obliczanie macierzy widoku i projekcji, ktore sa nastepnie
 * uzywane w procesie renderowania. Umozliwia poruszanie sie i zmiane orientacji
 * w przestrzeni.
 */
class Camera {
public:
    /**
     * @enum ProjectionType
     * @brief Okresla typ projekcji uzywanej przez kamere.
     */
    enum class ProjectionType {
        Perspective,  ///< Projekcja perspektywiczna (zbiezna).
        Orthographic  ///< Projekcja ortogonalna (rownolegla).
    };

    /**
     * @enum Direction
     * @brief Okresla kierunki ruchu kamery, uzywane w metodzie processKeyboard.
     * Te kierunki sa wzgledem lokalnego ukladu wspolrzednych kamery.
     */
    enum class Direction {
        Forward,    ///< Do przodu.
        Backward,   ///< Do tylu.
        Left,       ///< W lewo.
        Right,      ///< W prawo.
        Up,         ///< Do gory (wzgledem lokalnej osi 'up' kamery lub globalnej 'worldUp').
        Down        ///< W dol (wzgledem lokalnej osi 'up' kamery lub globalnej 'worldUp').
    };

private:
    // --- Skladowe pozycji i orientacji kamery ---
    glm::vec3 m_position; ///< Pozycja kamery w przestrzeni swiata.
    glm::vec3 m_front;    ///< Wektor skierowany "do przodu" od kamery (kierunek patrzenia).
    glm::vec3 m_up;       ///< Wektor skierowany "w gore" w lokalnym ukladzie kamery.
    glm::vec3 m_right;    ///< Wektor skierowany "w prawo" w lokalnym ukladzie kamery.
    glm::vec3 m_worldUp;  ///< Wektor "w gore" w przestrzeni swiata (np. glm::vec3(0.0f, 1.0f, 0.0f)).

    // --- Katy Eulera (orientacja kamery) ---
    float m_yaw;        ///< Kat odchylenia (obrot wokol osi Y).
    float m_pitch;      ///< Kat pochylenia (obrot wokol osi X).
    // Roll (obrot wokol osi Z) nie jest tutaj implementowany dla uproszczenia.

    // --- Parametry projekcji ---
    float m_fov;            ///< Pole widzenia (Field of View) w stopniach, dla projekcji perspektywicznej.
    float m_aspectRatio;    ///< Stosunek szerokosci do wysokosci okna (aspect ratio).
    float m_nearPlane;      ///< Odleglosc do bliskiej plaszczyzny przycinania.
    float m_farPlane;       ///< Odleglosc do dalekiej plaszczyzny przycinania.

    ProjectionType m_projectionType; ///< Aktualnie uzywany typ projekcji.

    // --- Ustawienia czulosci i predkosci kamery ---
    float m_movementSpeed;    ///< Predkosc poruszania sie kamery.
    float m_mouseSensitivity; ///< Czulosc myszy przy obracaniu kamera.

    /**
     * @brief Prywatna metoda aktualizujaca wektory kierunkowe kamery (front, right, up).
     * Wywolywana po kazdej zmianie katow yaw lub pitch.
     */
    void updateCameraVectors();

public:
    /**
     * @brief Konstruktor kamery.
     * @param startPosition Poczatkowa pozycja kamery. Domyslnie (0,0,3).
     * @param upDirection Wektor "w gore" przestrzeni swiata. Domyslnie (0,1,0).
     * @param startYaw Poczatkowy kat odchylenia (yaw) w stopniach. Domyslnie -90 (patrzenie wzdluz -Z).
     * @param startPitch Poczatkowy kat pochylenia (pitch) w stopniach. Domyslnie 0.
     * @param startFov Poczatkowe pole widzenia (FOV) w stopniach. Domyslnie 45.
     * @param type Poczatkowy typ projekcji. Domyslnie Perspective.
     */
    Camera(glm::vec3 startPosition = glm::vec3(0.0f, 0.0f, 3.0f),
        glm::vec3 upDirection = glm::vec3(0.0f, 1.0f, 0.0f),
        float startYaw = -90.0f,
        float startPitch = 0.0f,
        float startFov = 45.0f,
        ProjectionType type = ProjectionType::Perspective);

    /**
     * @brief Zwraca macierz widoku (view matrix) obliczona na podstawie aktualnej pozycji i orientacji kamery.
     * Uzywa funkcji glm::lookAt.
     * @return Macierz widoku (4x4).
     */
    glm::mat4 getViewMatrix() const;

    /**
     * @brief Zwraca macierz projekcji obliczona na podstawie aktualnych parametrow kamery (FOV, aspect ratio, plaszczyzny przycinania, typ projekcji).
     * @return Macierz projekcji (4x4).
     */
    glm::mat4 getProjectionMatrix() const;

    // --- Metody przetwarzania wejscia ---

    /**
     * @brief Przetwarza wejscie z klawiatury w celu poruszenia kamera.
     * @param direction Kierunek ruchu (z enum Camera::Direction).
     * @param deltaTime Czas, ktory uplynal od ostatniej klatki (do skalowania predkosci).
     */
    void processKeyboard(Direction direction, float deltaTime);

    /**
     * @brief Przetwarza ruch myszy w celu zmiany orientacji kamery (zmiana katow yaw i pitch).
     * @param xOffset Przesuniecie myszy w osi X.
     * @param yOffset Przesuniecie myszy w osi Y.
     * @param constrainPitch Czy ograniczyc kat pitch, aby uniknac "przewrocenia" kamery. Domyslnie true.
     */
    void processMouseMovement(float xOffset, float yOffset, bool constrainPitch = true);

    /**
     * @brief Przetwarza przewijanie rolka myszy w celu zmiany pola widzenia (FOV), symulujac zoom.
     * Dziala tylko dla projekcji perspektywicznej.
     * @param yOffset Wartosc przewiniecia rolki (zwykle dodatnia dla przewijania w gore, ujemna w dol).
     */
    void processMouseScroll(float yOffset);

    // --- Settery dla parametrow kamery ---

    /** @brief Ustawia typ projekcji kamery. */
    void setProjectionType(ProjectionType type);
    /** @brief Ustawia stosunek szerokosci do wysokosci okna (aspect ratio). */
    void setAspectRatio(float ratio);
    /** @brief Ustawia pole widzenia (FOV) w stopniach (dla projekcji perspektywicznej). */
    void setFOV(float fovValue);
    /** @brief Ustawia odleglosci do bliskiej i dalekiej plaszczyzny przycinania. */
    void setClippingPlanes(float nearClip, float farClip);
    /** @brief Ustawia pozycje kamery w przestrzeni swiata. */
    void setPosition(const glm::vec3& pos);
    /** @brief Ustawia predkosc poruszania sie kamery. */
    void setMovementSpeed(float speed);
    /** @brief Ustawia czulosc myszy. */
    void setMouseSensitivity(float sensitivity);
    /** @brief Ustawia kat odchylenia (yaw) w stopniach i aktualizuje wektory kamery. */
    void setYaw(float yawValue);
    /** @brief Ustawia kat pochylenia (pitch) w stopniach (z ograniczeniem) i aktualizuje wektory kamery. */
    void setPitch(float pitchValue);

    // --- Gettery dla parametrow i wektorow kamery ---

    /** @brief Zwraca aktualna pozycje kamery. */
    glm::vec3 getPosition() const;
    /** @brief Zwraca aktualny wektor "do przodu" kamery. */
    glm::vec3 getFront() const;
    /** @brief Zwraca aktualny wektor "w gore" w lokalnym ukladzie kamery. */
    glm::vec3 getUp() const;
    /** @brief Zwraca aktualny wektor "w prawo" w lokalnym ukladzie kamery. */
    glm::vec3 getRight() const;
    /** @brief Zwraca aktualny kat odchylenia (yaw) w stopniach. */
    float getYaw() const;
    /** @brief Zwraca aktualny kat pochylenia (pitch) w stopniach. */
    float getPitch() const;
    /** @brief Zwraca aktualne pole widzenia (FOV) w stopniach. */
    float getFOV() const;
    /** @brief Zwraca aktualna predkosc poruszania sie kamery. */
    float getMovementSpeed() const;
    /** @brief Zwraca aktualna czulosc myszy. */
    float getMouseSensitivity() const;
    /** @brief Zwraca aktualny typ projekcji kamery. */
    ProjectionType getProjectionType() const;
    /** @brief Zwraca aktualny stosunek szerokosci do wysokosci (aspect ratio). */
    float getAspectRatio() const { return m_aspectRatio; }
    /** @brief Zwraca odleglosc do bliskiej plaszczyzny przycinania. */
    float getNearPlane() const { return m_nearPlane; }
    /** @brief Zwraca odleglosc do dalekiej plaszczyzny przycinania. */
    float getFarPlane() const { return m_farPlane; }

};

#endif // CAMERA_H
