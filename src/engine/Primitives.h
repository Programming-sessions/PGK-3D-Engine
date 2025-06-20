/**
* @file Primitives.h
* @brief Definicja klasy Primitives.
*
* Plik ten zawiera deklarację klasy Primitives, która dostarcza
* metody do tworzenia podstawowych obiektów geometrycznych,
* takich jak sześciany czy płaszczyzny.
*/
#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <glad/glad.h> // Do operacji OpenGL
#include <vector>
#include <memory> // Dla std::shared_ptr, std::unique_ptr
#include <algorithm> // Dla std::min, std::max (potencjalnie)
#include <limits>    // Dla std::numeric_limits (potencjalnie)

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include "IRenderable.h"
#include "ICollidable.h"
#include "BoundingVolume.h" // Zawiera BoundingShapeType
#include "Texture.h"        // Definicja struktury Texture
#include "Lighting.h"       // Zawiera definicje Material

// Deklaracje wyprzedzajace
class Shader;
class Camera;

/**
 * @struct Vertex
 * @brief Struktura reprezentujaca wierzcholek w meshu.
 * * Przechowuje pozycje, normale, wspolrzedne tekstury i kolor wierzcholka.
 */
struct Vertex {
    glm::vec3 position;  ///< Pozycja wierzcholka w przestrzeni modelu.
    glm::vec3 normal;    ///< Wektor normalny wierzcholka.
    glm::vec2 texCoords; ///< Wspolrzedne tekstury (UV).
    glm::vec4 color;     ///< Kolor wierzcholka.

    /**
     * @brief Konstruktor domyslny.
     * @param pos Pozycja wierzcholka.
     * @param norm Wektor normalny.
     * @param tex Wspolrzedne tekstury.
     * @param col Kolor wierzcholka.
     */
    Vertex(const glm::vec3& pos = glm::vec3(0.0f),
        const glm::vec3& norm = glm::vec3(0.0f, 0.0f, 1.0f),
        const glm::vec2& tex = glm::vec2(0.0f),
        const glm::vec4& col = glm::vec4(1.0f))
        : position(pos), normal(norm), texCoords(tex), color(col) {
    }
};

/**
 * @class BasePrimitive
 * @brief Klasa bazowa dla wszystkich prymitywow geometrycznych w silniku.
 * * Implementuje interfejsy IRenderable i ICollidable. Zarzadza meshem,
 * transformacjami, materialem i cieniowaniem prymitywu.
 * Kazdy prymityw moze byc renderowany, rzucac cienie i uczestniczyc w detekcji kolizji.
 */
class BasePrimitive : public IRenderable, public ICollidable {
public:
    /**
     * @brief Konstruktor domyslny BasePrimitive.
     * Inicjalizuje macierz modelu, pozycje, rotacje, skale oraz domyslny shader.
     */
    BasePrimitive();

    /**
     * @brief Destruktor wirtualny.
     * Zwalnia zasoby zaalokowane dla buforow VAO, VBO, EBO.
     */
    virtual ~BasePrimitive();

    /**
     * @brief Renderuje prymityw.
     * @param viewMatrix Macierz widoku.
     * @param projectionMatrix Macierz projekcji.
     * @see IRenderable::render
     */
    virtual void render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) override;

    /**
     * @brief Renderuje prymityw z dodatkowymi informacjami o kamerze.
     * @param viewMatrix Macierz widoku.
     * @param projectionMatrix Macierz projekcji.
     * @param camera Wskaznik do obiektu kamery.
     */
    void render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, Camera* camera);

    /**
     * @brief Przesuwa prymityw o podany wektor.
     * @param translation Wektor przesuniecia.
     */
    void translate(const glm::vec3& translation);

    /**
     * @brief Obraca prymityw wokol podanej osi o dany kat.
     * @param angle Kat obrotu w stopniach.
     * @param axis Os obrotu.
     */
    void rotate(float angle, const glm::vec3& axis);

    /**
     * @brief Skaluje prymityw o podany wektor.
     * @param scaleFactor Wektor skalowania dla osi X, Y, Z.
     */
    void scale(const glm::vec3& scaleFactor);

    /**
     * @brief Ustawia pozycje prymitywu w przestrzeni swiata.
     * @param position Nowa pozycja prymitywu.
     */
    void setPosition(const glm::vec3& position);

    /**
     * @brief Ustawia rotacje prymitywu za pomoca kwaternionu.
     * @param rotation Nowa rotacja prymitywu.
     */
    void setRotation(const glm::quat& rotation);

    /**
     * @brief Ustawia skale prymitywu.
     * @param scale Nowa skala prymitywu.
     */
    void setScale(const glm::vec3& scale);

    /**
     * @brief Bezposrednio ustawia macierz modelu prymitywu.
     * @param modelMatrix Nowa macierz modelu.
     */
    void setTransform(const glm::mat4& modelMatrix);

    /**
     * @brief Zwraca macierz modelu prymitywu.
     * @return Stala referencja do macierzy modelu.
     */
    const glm::mat4& getModelMatrix() const { return m_modelMatrix; }

    /**
     * @brief Ustawia program shadera dla prymitywu.
     * @param shader Wskaznik (shared_ptr) do obiektu shadera.
     */
    void setShader(std::shared_ptr<Shader> shader);

    /**
     * @brief Zwraca program shadera uzywany przez prymityw.
     * @return Wskaznik (shared_ptr) do obiektu shadera.
     */
    std::shared_ptr<Shader> getShader() const { return m_shaderProgram; }

    /**
     * @brief Ustawia wlasciwosci materialu prymitywu.
     * @param material Obiekt materialu.
     */
    void setMaterial(const Material& material);

    /**
     * @brief Zwraca wlasciwosci materialu prymitywu.
     * @return Stala referencja do obiektu materialu.
     */
    const Material& getMaterial() const { return m_material; }

    /**
     * @brief Ustawia skladowa ambient materialu.
     * @param ambient Wektor koloru ambient (RGB).
     */
    void setMaterialAmbient(const glm::vec3& ambient);

    /**
     * @brief Ustawia skladowa diffuse materialu.
     * @param diffuse Wektor koloru diffuse (RGB).
     */
    void setMaterialDiffuse(const glm::vec3& diffuse);

    /**
     * @brief Ustawia skladowa specular materialu.
     * @param specular Wektor koloru specular (RGB).
     */
    void setMaterialSpecular(const glm::vec3& specular);

    /**
     * @brief Ustawia polyskliwosc materialu.
     * @param shininess Wartosc polyskliwosci.
     */
    void setMaterialShininess(float shininess);

    /**
     * @brief Ustawia teksture diffuse dla materialu.
     * @param texture Wskaznik (shared_ptr) do obiektu tekstury diffuse.
     */
    void setDiffuseTexture(std::shared_ptr<Texture> texture);

    /**
     * @brief Ustawia teksture specular dla materialu.
     * @param texture Wskaznik (shared_ptr) do obiektu tekstury specular.
     */
    void setSpecularTexture(std::shared_ptr<Texture> texture);

    /**
     * @brief Ustawia, czy prymityw ma uzywac cieniowania plaskiego (flat shading).
     * @param useFlat True, jesli ma byc uzyte cieniowanie plaskie, false w przeciwnym razie.
     */
    void setUseFlatShading(bool useFlat);

    /**
     * @brief Sprawdza, czy prymityw uzywa cieniowania plaskiego.
     * @return True, jesli prymityw uzywa cieniowania plaskiego, false w przeciwnym razie.
     */
    bool getUseFlatShading() const;

    /**
     * @brief Renderuje prymityw wylacznie dla przebiegu generowania mapy glebokosci (np. dla cieni).
     * @param depthShader Wskaznik do shadera uzywanego do generowania mapy glebokosci.
     * @see IRenderable::renderForDepthPass
     */
    void renderForDepthPass(Shader* depthShader) override;

    /**
     * @brief Ustawia, czy prymityw ma rzucac cienie.
     * @param castsShadow True, jesli prymityw ma rzucac cienie, false w przeciwnym razie.
     * @see IRenderable::setCastsShadow
     */
    void setCastsShadow(bool castsShadow) override;

    /**
     * @brief Sprawdza, czy prymityw rzuca cienie.
     * @return True, jesli prymityw rzuca cienie, false w przeciwnym razie.
     * @see IRenderable::castsShadow
     */
    bool castsShadow() const override;

    /**
     * @brief Zwraca obiekt otaczajacy (bounding volume) prymitywu.
     * Jesli obiekt otaczajacy jest "brudny" (wymaga aktualizacji), zostanie zaktualizowany.
     * @return Wskaznik do obiektu BoundingVolume.
     * @see ICollidable::getBoundingVolume
     */
    BoundingVolume* getBoundingVolume() override;

    /**
     * @brief Zwraca staly wskaznik do obiektu otaczajacego prymitywu.
     * @return Staly wskaznik do obiektu BoundingVolume.
     * @see ICollidable::getBoundingVolume
     */
    const BoundingVolume* getBoundingVolume() const override;

    /**
     * @brief Ustawia, czy kolizje dla tego prymitywu sa wlaczone.
     * @param enabled True, aby wlaczyc kolizje, false aby wylaczyc.
     * @see ICollidable::setCollisionsEnabled
     */
    void setCollisionsEnabled(bool enabled) override;

    /**
     * @brief Sprawdza, czy kolizje dla tego prymitywu sa wlaczone.
     * @return True, jesli kolizje sa wlaczone, false w przeciwnym razie.
     * @see ICollidable::collisionsEnabled
     */
    bool collisionsEnabled() const override;

protected:
    std::vector<Vertex> m_vertices;         ///< Wektor wierzcholkow prymitywu.
    std::vector<GLuint> m_indices;          ///< Wektor indeksow prymitywu (dla EBO).
    std::shared_ptr<Shader> m_shaderProgram;///< Program shadera uzywany do renderowania.
    Material m_material;                    ///< Material prymitywu (kolory, tekstury, polysk).

    bool m_useFlatShading;                  ///< Flaga okreslajaca uzycie cieniowania plaskiego.
    bool m_castsShadow;                     ///< Flaga okreslajaca, czy obiekt rzuca cienie.
    bool m_collisionsEnabled;               ///< Flaga okreslajaca, czy kolizje sa wlaczone.

    GLuint m_VAO;                           ///< Vertex Array Object.
    GLuint m_VBO;                           ///< Vertex Buffer Object.
    GLuint m_EBO;                           ///< Element Buffer Object.

    glm::mat4 m_modelMatrix;                ///< Macierz modelu (transformacje).
    glm::vec3 m_position;                   ///< Pozycja prymitywu.
    glm::quat m_rotation;                   ///< Rotacja prymitywu (kwaternion).
    glm::vec3 m_scaleFactor;                ///< Wspolczynniki skalowania prymitywu.

    std::unique_ptr<BoundingVolume> m_boundingVolume; ///< Obiekt otaczajacy dla detekcji kolizji.
    bool m_isBoundingVolumeDirty;           ///< Flaga wskazujaca, czy obiekt otaczajacy wymaga aktualizacji.

    /**
     * @brief Konfiguruje VAO, VBO i EBO na podstawie danych wierzcholkow i indeksow.
     * Wywolywana po zdefiniowaniu m_vertices i m_indices.
     */
    virtual void setupMesh();

    /**
     * @brief Wirtualna metoda do aktualizacji obiektu otaczajacego w przestrzeni swiata.
     * Musi byc zaimplementowana przez klasy pochodne.
     */
    virtual void updateWorldBoundingVolume() = 0;

    /**
     * @brief Rekomponuje macierz modelu na podstawie aktualnej pozycji, rotacji i skali.
     * Ustawia rowniez flage m_isBoundingVolumeDirty na true.
     */
    void recomposeModelMatrix();

private:
    /**
     * @brief Usuwa bufory VAO, VBO, EBO z pamieci karty graficznej.
     */
    void clearBuffers();
};

// --- Definicje klas potomnych ---

/**
 * @class Cube
 * @brief Reprezentuje prymityw szescianu.
 * Dziedziczy po BasePrimitive.
 */
class Cube : public BasePrimitive {
public:
    /**
     * @brief Konstruktor szescianu.
     * @param centerPosition Pozycja srodka szescianu.
     * @param sideLength Dlugosc boku szescianu.
     * @param cubeColor Kolor szescianu (RGBA). Domyslnie bialy.
     */
    Cube(const glm::vec3& centerPosition, float sideLength, const glm::vec4& cubeColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    /**
     * @brief Destruktor domyslny.
     */
    virtual ~Cube() = default;

protected:
    /**
     * @brief Aktualizuje obiekt otaczajacy (OBB) szescianu.
     * @see BasePrimitive::updateWorldBoundingVolume
     */
    void updateWorldBoundingVolume() override;
private:
    glm::vec3 m_localCenterOffset;  ///< Przesuniecie srodka OBB wzgledem lokalnego (0,0,0).
    glm::vec3 m_localHalfExtents;   ///< Polowy wymiarow OBB w lokalnym ukladzie wspolrzednych.
    float m_sideLength;             ///< Dlugosc boku szescianu.
};

/**
 * @class Plane
 * @brief Reprezentuje prymityw plaszczyzny.
 * Dziedziczy po BasePrimitive.
 */
class Plane : public BasePrimitive {
public:
    /**
     * @brief Konstruktor plaszczyzny.
     * @param centerPosition Pozycja srodka plaszczyzny.
     * @param p_width Szerokosc plaszczyzny.
     * @param p_depth Glebokosc (dlugosc) plaszczyzny.
     * @param planeColor Kolor plaszczyzny (RGBA). Domyslnie bialy.
     * @param thickness Grubosc plaszczyzny dla celow bounding volume. Domyslnie bardzo mala.
     */
    Plane(const glm::vec3& centerPosition, float p_width, float p_depth,
        const glm::vec4& planeColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
        float thickness = 0.01f);

    /**
     * @brief Destruktor domyslny.
     */
    ~Plane() override = default;

protected:
    /**
     * @brief Aktualizuje obiekt otaczajacy (OBB) plaszczyzny.
     * @see BasePrimitive::updateWorldBoundingVolume
     */
    void updateWorldBoundingVolume() override;
private:
    glm::vec3 m_localCenterOffset;  ///< Przesuniecie srodka OBB wzgledem lokalnego (0,0,0).
    glm::vec3 m_localHalfExtents;   ///< Polowy wymiarow OBB (uwzgledniajac grubosc).
    float m_width;                  ///< Szerokosc plaszczyzny.
    float m_depth;                  ///< Glebokosc (dlugosc) plaszczyzny.
    glm::vec3 m_visualNormal;       ///< Normalna wizualna plaszczyzny (domyslnie +Y).
};

/**
 * @class Sphere
 * @brief Reprezentuje prymityw sfery.
 * Dziedziczy po BasePrimitive.
 */
class Sphere : public BasePrimitive {
public:
    /**
     * @brief Konstruktor sfery.
     * @param centerPosition Pozycja srodka sfery.
     * @param radius Promien sfery.
     * @param longitudeSegments Liczba segmentow wzdluz dlugosci geograficznej. Domyslnie 36.
     * @param latitudeSegments Liczba segmentow wzdluz szerokosci geograficznej. Domyslnie 18.
     * @param sphereColor Kolor sfery (RGBA). Domyslnie bialy.
     */
    Sphere(const glm::vec3& centerPosition, float radius, int longitudeSegments = 36, int latitudeSegments = 18,
        const glm::vec4& sphereColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    /**
     * @brief Destruktor domyslny.
     */
    ~Sphere() override = default;
protected:
    /**
     * @brief Aktualizuje obiekt otaczajacy (AABB) sfery.  
     * TODO: Zaimplementowac SphereBV.
     * @see BasePrimitive::updateWorldBoundingVolume
     */
    void updateWorldBoundingVolume() override;
private:
    float m_radius; ///< Promien sfery.
};

/**
 * @class Cylinder
 * @brief Reprezentuje prymityw cylindra.
 * Dziedziczy po BasePrimitive.
 */
class Cylinder : public BasePrimitive {
public:
    /**
     * @brief Konstruktor cylindra.  
     * @param centerPosition Pozycja srodka cylindra (srodek wysokosci).
     * @param radius Promien podstawy cylindra.
     * @param height Wysokosc cylindra.
     * @param segments Liczba segmentow uzytych do aproksymacji okregu podstawy. Domyslnie 36.
     * @param cylinderColor Kolor cylindra (RGBA). Domyslnie bialy.
     */
    Cylinder(const glm::vec3& centerPosition, float radius, float height, int segments = 36,
        const glm::vec4& cylinderColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    /**
     * @brief Destruktor domyslny.
     */
    ~Cylinder() override = default;

protected:
    /**
     * @brief Aktualizuje obiekt otaczajacy (CylinderBV) cylindra.  
     * @see BasePrimitive::updateWorldBoundingVolume
     */
    void updateWorldBoundingVolume() override;
private:
    glm::vec3 m_localBaseCenter1; ///< Srodek dolnej podstawy w lokalnym ukladzie wspolrzednych.
    glm::vec3 m_localBaseCenter2; ///< Srodek gornej podstawy w lokalnym ukladzie wspolrzednych.
    float m_localRadius;          ///< Promien cylindra.
    float m_height;               ///< Wysokosc cylindra.
    int m_segments;               ///< Liczba segmentow.
};

/**
 * @class SquarePyramid
 * @brief Reprezentuje prymityw ostroslupa o podstawie kwadratowej.
 * Dziedziczy po BasePrimitive.
 */
class SquarePyramid : public BasePrimitive {
public:
    /**
     * @brief Konstruktor ostroslupa o podstawie kwadratowej.  
     * @param centerPosition Pozycja srodka podstawy ostroslupa.
     * @param baseLength Dlugosc boku podstawy.
     * @param height Wysokosc ostroslupa.
     * @param pyramidColor Kolor ostroslupa (RGBA). Domyslnie bialy.
     */
    SquarePyramid(const glm::vec3& centerPosition, float baseLength, float height,
        const glm::vec4& pyramidColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    /**
     * @brief Destruktor domyslny.
     */
    ~SquarePyramid() override = default;
protected:
    /**
     * @brief Aktualizuje obiekt otaczajacy (AABB) ostroslupa.  
     * TODO: Rozwazyc dokladniejszy BoundingVolume.
     * @see BasePrimitive::updateWorldBoundingVolume
     */
    void updateWorldBoundingVolume() override;
private:
    float m_baseLength;     ///< Dlugosc boku podstawy.
    float m_pyramidHeight;  ///< Wysokosc ostroslupa.
};

/**
 * @class Cone
 * @brief Reprezentuje prymityw stozka.
 * Dziedziczy po BasePrimitive.
 */
class Cone : public BasePrimitive {
public:
    /**
     * @brief Konstruktor stozka.  
     * @param centerPosition Pozycja srodka podstawy stozka.
     * @param radius Promien podstawy stozka.
     * @param height Wysokosc stozka.
     * @param segments Liczba segmentow uzytych do aproksymacji okregu podstawy. Domyslnie 36.
     * @param coneColor Kolor stozka (RGBA). Domyslnie bialy.
     */
    Cone(const glm::vec3& centerPosition, float radius, float height, int segments = 36,
        const glm::vec4& coneColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    /**
     * @brief Destruktor domyslny.
     */
    ~Cone() override = default;
protected:
    /**
     * @brief Aktualizuje obiekt otaczajacy (AABB) stozka.  
     * TODO: Zaimplementowac ConeBV.
     * @see BasePrimitive::updateWorldBoundingVolume
     */
    void updateWorldBoundingVolume() override;
private:
    float m_radius;     ///< Promien podstawy stozka.
    float m_coneHeight; ///< Wysokosc stozka.
    int m_segments;     ///< Liczba segmentow.
};

#endif // PRIMITIVES_H