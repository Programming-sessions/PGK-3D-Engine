/**
* @file Model.h
* @brief Definicja klasy Model.
*
* Plik ten zawiera deklarację klasy Model, która reprezentuje
* trójwymiarowy model składający się z siatek (meshes) i materiałów.
*/
#ifndef MODEL_H
#define MODEL_H

#include <string>
#include <vector>
#include <memory> // Dla std::shared_ptr, std::unique_ptr

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "IRenderable.h"       // Interfejs renderowania
#include "ICollidable.h"       // Interfejs kolizji
#include "ModelData.h"         // Definicje MeshData i ModelAsset (ktore uzywaja Vertex, Material)
#include "BoundingVolume.h"    // Dla AABB, CylinderBV, BoundingShapeType, ColliderType

// Deklaracje wyprzedzajace
class Shader;
class Camera;
struct Vertex;

/**
 * @struct MeshRenderer
 * @brief Odpowiada za renderowanie pojedynczej siatki (mesh) modelu.
 * Przechowuje identyfikatory buforow GPU (VAO, VBO, EBO), liczbe indeksow
 * oraz wlasciwosci materialu dla danej siatki.
 */
struct MeshRenderer {
    unsigned int VAO = 0; ///< Vertex Array Object ID.
    unsigned int VBO = 0; ///< Vertex Buffer Object ID.
    unsigned int EBO = 0; ///< Element Buffer Object ID.
    size_t indexCount = 0; ///< Liczba indeksow do narysowania.
    Material materialProperties; ///< Wlasciwosci materialu dla tej siatki.

    /**
     * @brief Konstruktor domyslny.
     */
    MeshRenderer() = default;

    /**
     * @brief Konfiguruje bufory GPU (VAO, VBO, EBO) dla danych siatki.
     * @param meshData Dane siatki (wierzcholki, indeksy).
     * @param modelNameForLog Nazwa modelu uzywana do logowania, dla latwiejszej identyfikacji.
     */
    void setupGpuBuffers(const MeshData& meshData, const std::string& modelNameForLog);

    /**
     * @brief Zwalnia zasoby GPU (VAO, VBO, EBO) zajmowane przez siatke.
     * @param modelNameForLog Nazwa modelu uzywana do logowania.
     */
    void cleanupGpuBuffers(const std::string& modelNameForLog);
};

/**
 * @class Model
 * @brief Reprezentuje kompletny model 3D skladajacy sie z jednej lub wiecej siatek.
 * Implementuje interfejsy IRenderable i ICollidable, umozliwiajac renderowanie
 * oraz uczestniczenie w systemie detekcji kolizji.
 * Zarzadza transformacja (pozycja, rotacja, skala), shaderem, oraz obiektami MeshRenderer.
 */
class Model : public IRenderable, public ICollidable {
public:
    /**
     * @brief Konstruktor modelu.
     * @param name Nazwa modelu.
     * @param asset Wskaznik (shared_ptr) do zasobu ModelAsset zawierajacego dane siatek.
     * @param defaultShader Wskaznik (shared_ptr) do domyslnego shadera uzywanego przez model.
     */
    Model(const std::string& name, std::shared_ptr<ModelAsset> asset, std::shared_ptr<Shader> defaultShader);

    /**
     * @brief Wirtualny destruktor.
     * Zwalnia zasoby zajmowane przez obiekty MeshRenderer.
     */
    virtual ~Model();

    // --- Metody z interfejsu IRenderable ---

    /**
     * @brief Renderuje model.
     * @param viewMatrix Macierz widoku.
     * @param projectionMatrix Macierz projekcji.
     */
    void render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) override;

    /**
     * @brief Renderuje model dla przebiegu generowania mapy glebokosci (np. cieni).
     * @param depthShader Shader uzywany do generowania mapy glebokosci.
     */
    void renderForDepthPass(Shader* depthShader) override;

    /**
     * @brief Sprawdza, czy model rzuca cienie.
     * @return True, jesli model rzuca cienie, false w przeciwnym razie.
     */
    bool castsShadow() const override;

    /**
     * @brief Ustawia, czy model ma rzucac cienie.
     * @param castsShadow True, jesli model ma rzucac cienie.
     */
    void setCastsShadow(bool castsShadow) override;

    // --- Metody z interfejsu ICollidable ---

    /**
     * @brief Zwraca obiekt otaczajacy (bounding volume) modelu.
     * @return Wskaznik do obiektu BoundingVolume.
     */
    BoundingVolume* getBoundingVolume() override;

    /**
     * @brief Zwraca staly wskaznik do obiektu otaczajacego modelu.
     * @return Staly wskaznik do obiektu BoundingVolume.
     */
    const BoundingVolume* getBoundingVolume() const override;

    /**
     * @brief Zwraca typ kolidera modelu (np. statyczny, dynamiczny).
     * @return Typ kolidera.
     */
    ColliderType getColliderType() const override { return m_colliderType; }

    /**
     * @brief Ustawia, czy kolizje dla tego modelu sa wlaczone.
     * @param enabled True, aby wlaczyc kolizje, false aby wylaczyc.
     */
    void setCollisionsEnabled(bool enabled) override;

    /**
     * @brief Sprawdza, czy kolizje dla tego modelu sa wlaczone.
     * @return True, jesli kolizje sa wlaczone, false w przeciwnym razie.
     */
    bool collisionsEnabled() const override;

    // --- Zarzadzanie transformacja ---

    /**
     * @brief Ustawia pozycje modelu w przestrzeni swiata.
     * @param position Nowa pozycja modelu.
     */
    void setPosition(const glm::vec3& position);

    /**
     * @brief Ustawia rotacje modelu za pomoca kwaternionu.
     * @param rotation Nowa rotacja modelu.
     */
    void setRotation(const glm::quat& rotation);

    /**
     * @brief Ustawia skale modelu.
     * @param scale Nowa skala modelu.
     */
    void setScale(const glm::vec3& scale);

    /**
     * @brief Zwraca macierz modelu.
     * @return Stala referencja do macierzy modelu.
     */
    const glm::mat4& getModelMatrix() const { return m_modelMatrix; }

    /**
     * @brief Zwraca pozycje modelu.
     * @return Stala referencja do wektora pozycji.
     */
    const glm::vec3& getPosition() const { return m_position; }

    /**
     * @brief Zwraca rotacje modelu.
     * @return Stala referencja do kwaternionu rotacji.
     */
    const glm::quat& getRotation() const { return m_rotation; }

    /**
     * @brief Zwraca skale modelu.
     * @return Stala referencja do wektora skali.
     */
    const glm::vec3& getScale() const { return m_scaleFactor; }

    /**
     * @brief Zwraca nazwe modelu.
     * @return Stala referencja do nazwy modelu.
     */
    const std::string& getName() const { return m_modelName; }

    /**
    * @brief Zwraca referencję do wektora rendererów siatek modelu.
    * Pozwala na modyfikację właściwości materiałów poszczególnych siatek.
    * @return Referencja do std::vector<MeshRenderer>.
    */
    std::vector<MeshRenderer>& getMeshRenderers() { return m_meshRenderers; }

    /**
    * @brief Zwraca stałą referencję do wektora rendererów siatek modelu.
    * @return Stała referencja do std::vector<MeshRenderer>.
    */
    const std::vector<MeshRenderer>& getMeshRenderers() const { return m_meshRenderers; }


    // --- Zarzadzanie shaderem ---

    /**
     * @brief Ustawia program shadera dla modelu.
     * @param shader Wskaznik (shared_ptr) do obiektu shadera.
     */
    void setShader(std::shared_ptr<Shader> shader);

    /**
     * @brief Zwraca program shadera uzywany przez model.
     * @return Wskaznik (shared_ptr) do obiektu shadera.
     */
    std::shared_ptr<Shader> getShader() const { return m_shader; }

    /**
     * @brief Ustawia kamere, ktora bedzie uzywana do informacji o oswietleniu (np. pozycja kamery).
     * @param camera Wskaznik do obiektu kamery.
     */
    void setCameraForLighting(Camera* camera) { m_cameraForLighting = camera; }

    /**
     * @brief Ustawia typ i parametry bryly kolizyjnej dla modelu.
     * @param newShape Typ nowej bryly kolizyjnej (np. AABB, CYLINDER).
     * @param colliderType Typ kolidera (np. statyczny, dynamiczny). Domyslnie DYNAMIC.
     * @param p1 Pierwszy punkt definiujacy bryle (np. dla cylindra: srodek jednej podstawy). Uzywany dla CYLINDER.
     * @param p2 Drugi punkt definiujacy bryle (np. dla cylindra: srodek drugiej podstawy). Uzywany dla CYLINDER.
     * @param r Promien (np. dla cylindra lub sfery). Uzywany dla CYLINDER.
     * Dla AABB, parametry p1, p2, r sa ignorowane, poniewaz AABB jest obliczane na podstawie wierzcholkow modelu.
     */
    void setBoundingShape(BoundingShapeType newShape,
        ColliderType colliderType = ColliderType::DYNAMIC,
        const glm::vec3& p1 = glm::vec3(0.0f, -0.5f, 0.0f),
        const glm::vec3& p2 = glm::vec3(0.0f, 0.5f, 0.0f),
        float r = 0.5f);

    /**
     * @brief Zwraca typ aktualnie aktywnej bryly kolizyjnej.
     * @return Typ aktywnej bryly kolizyjnej.
     */
    BoundingShapeType getActiveBoundingShapeType() const { return m_activeBoundingShapeType; }


private:
    std::string m_modelName; ///< Nazwa identyfikujaca model.
    std::shared_ptr<ModelAsset> m_asset; ///< Wskaznik do zasobu ModelAsset z danymi siatek.
    std::vector<MeshRenderer> m_meshRenderers; ///< Wektor rendererow dla kazdej siatki modelu.
    std::shared_ptr<Shader> m_shader; ///< Shader uzywany do renderowania modelu.

    glm::mat4 m_modelMatrix; ///< Macierz transformacji modelu w przestrzeni swiata.
    glm::vec3 m_position;    ///< Pozycja modelu.
    glm::quat m_rotation;    ///< Rotacja modelu (kwaternion).
    glm::vec3 m_scaleFactor; ///< Wspolczynniki skalowania modelu.

    Camera* m_cameraForLighting; ///< Wskaznik do kamery dla celow oswietlenia (np. viewPos).

    bool m_castsShadow; ///< Czy model rzuca cienie.

    // --- Czlonkowie zwiazani z kolizjami ---
    std::unique_ptr<BoundingVolume> m_boundingVolume; ///< Polimorficzny wskaznik na aktywna bryle kolizyjna.
    BoundingShapeType m_activeBoundingShapeType;      ///< Typ aktualnie uzywanej bryly kolizyjnej.
    bool m_collisionsEnabled;                         ///< Czy kolizje dla modelu sa wlaczone.
    ColliderType m_colliderType;                      ///< Typ kolidera (statyczny, dynamiczny itp.).

    // Lokalne parametry dla specyficznych typow bryl kolizyjnych
    // Przyklad dla cylindra:
    glm::vec3 m_localCylinderP1;     ///< Lokalny punkt P1 dla cylindrycznej bryly kolizyjnej.
    glm::vec3 m_localCylinderP2;     ///< Lokalny punkt P2 dla cylindrycznej bryly kolizyjnej.
    float m_localCylinderRadius; ///< Lokalny promien dla cylindrycznej bryly kolizyjnej.
    // Mozna dodac podobne pola dla innych typow bryl, np. OBB (localHalfExtents, localCenterOffset).

    /**
     * @brief Aktualizuje macierz modelu na podstawie aktualnej pozycji, rotacji i skali.
     * Po aktualizacji macierzy modelu, wywoluje rowniez aktualizacje bryly kolizyjnej.
     */
    void updateModelMatrix();

    /**
     * @brief Inicjalizuje obiekty MeshRenderer dla kazdej siatki w ModelAsset.
     * Tworzy bufory GPU dla kazdej siatki.
     */
    void initializeMeshRenderers();

    /**
     * @brief Aktualizuje pozycje i wymiary aktualnie aktywnej bryly kolizyjnej.
     * Wywolywana po zmianie macierzy modelu lub zmianie typu bryly.
     */
    void updateCurrentBoundingVolume();
};

#endif // MODEL_H