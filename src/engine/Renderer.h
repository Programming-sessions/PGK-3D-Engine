/**
* @file Renderer.h
* @brief Definicja klasy Renderer.
*
* Plik ten zawiera deklarację klasy Renderer, która jest
* odpowiedzialna za renderowanie wszystkich obiektów na scenie.
*/
    #ifndef RENDERER_H
#define RENDERER_H

#include <vector>
#include <memory> // Dla std::vector, choc bezposrednio smart pointers nie sa tu uzywane
#include "IRenderable.h" // Interfejs dla obiektow renderowalnych
#include "Camera.h"     // Pelna definicja klasy Camera jest potrzebna

/**
 * @brief Odpowiada za renderowanie sceny 3D.
 *
 * Zarzadza kolekcja obiektow renderowalnych (IRenderable) oraz wskaznikiem
 * do aktywnej kamery. Iteruje po obiektach i wywoluje ich metody renderujace,
 * przekazujac niezbedne macierze (widoku, projekcji).
 */
class Renderer {
public:
    /**
     * @brief Konstruktor domyslny.
     */
    Renderer();

    /**
     * @brief Destruktor.
     * * Niszczenie Renderera nie powoduje niszczenia obiektow IRenderable
     * * ani kamery, poniewaz Renderer nie jest ich wlascicielem.
     */
    ~Renderer();

    // Zapobieganie kopiowaniu, poniewaz Renderer nie jest wlascicielem
    // swoich skladowych (m_camera, elementy w m_renderables) w sensie glebokim.
    // Kopiowanie mogloby prowadzic do niejasnosci w zarzadzaniu pamiecia.
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Operacje przenoszenia moga byc uzyteczne.
    /**
     * @brief Konstruktor przenoszacy.
     * @param other Obiekt Renderer do przeniesienia.
     */
    Renderer(Renderer&& other) noexcept;
    /**
     * @brief Operator przypisania przenoszacego.
     * @param other Obiekt Renderer do przeniesienia.
     * @return Referencja do tego obiektu.
     */
    Renderer& operator=(Renderer&& other) noexcept;


    /**
     * @brief Inicjalizuje renderer.
     * * W obecnej formie glownie loguje komunikat. Kamera jest ustawiana osobno.
     * @return true Zawsze zwraca true w tej implementacji.
     */
    bool initialize();

    /**
     * @brief Ustawia aktywna kamere dla renderera.
     * * Renderer nie przejmuje wlasnosci nad obiektem kamery.
     * @param cameraInstance Wskaznik do obiektu kamery.
     */
    void setCamera(Camera* cameraInstance);

    /**
     * @brief Zwraca wskaznik do aktualnie ustawionej kamery.
     * @return Wskaznik do obiektu Camera lub nullptr, jesli kamera nie jest ustawiona.
     */
    Camera* getCamera() const;

    /**
     * @brief Dodaje obiekt renderowalny do kolejki renderowania.
     * * Renderer nie przejmuje wlasnosci nad obiektem renderowalnym.
     * @param renderable Wskaznik do obiektu implementujacego IRenderable.
     */
    void addRenderable(IRenderable* renderable);

    /**
     * @brief Zwraca stala referencje do wektora wskaznikow na obiekty renderowalne.
     * @return Const referencja do wektora IRenderable*.
     */
    const std::vector<IRenderable*>& getRenderables() const;

    /**
     * @brief Usuwa obiekt renderowalny z kolejki renderowania.
     * * Nie niszczy samego obiektu renderowalnego.
     * @param renderable Wskaznik do obiektu IRenderable do usuniecia.
     */
    void removeRenderable(IRenderable* renderable);

    /**
     * @brief Usuwa wszystkie obiekty renderowalne z kolejki renderowania.
     * * Nie niszczy samych obiektow renderowalnych.
     */
    void clearRenderables();

    /**
     * @brief Renderuje scene na podstawie aktualnej kamery i listy obiektow renderowalnych.
     * * Zaklada, ze odpowiednie shadery i globalne uniformy (np. dane oswietlenia)
     * * zostaly juz ustawione przez wyzsza warstwe (np. Engine).
     */
    void renderScene();

private:
    std::vector<IRenderable*> m_renderables; ///< Kontener na wskazniki do obiektow renderowalnych.
    Camera* m_camera;                        ///< Wskaznik do aktywnej kamery.
};

#endif // RENDERER_H