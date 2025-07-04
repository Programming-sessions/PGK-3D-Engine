// Model.cpp
#include "Model.h" // Dołączenie odpowiedniego pliku nagłówkowego jako pierwsze

#include "Shader.h"
#include "Camera.h"
#include "Logger.h"
#include "Texture.h"    
#include "Primitives.h" 
#include "ModelData.h"  

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stddef.h> // Dla offsetof
#include <limits>   // Dla std::numeric_limits
#include <algorithm> // Dla std::min i std::max

// --- Implementacja MeshRenderer ---

void MeshRenderer::setupGpuBuffers(const MeshData& meshData, const std::string& modelNameForLog) {
    if (meshData.vertices.empty()) {
        Logger::getInstance().warning("MeshRenderer::setupGpuBuffers dla '" + modelNameForLog + "': Siatka nie zawiera wierzcholkow. Bufory nie zostana utworzone.");
        return;
    }

    indexCount = meshData.indices.size();
    materialProperties = meshData.material; // Kopia materialu dla tego renderera

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    if (indexCount > 0) { // EBO jest tworzone tylko jesli sa indeksy
        glGenBuffers(1, &EBO);
    }

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshData.vertices.size() * sizeof(Vertex), meshData.vertices.data(), GL_STATIC_DRAW);

    if (indexCount > 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData.indices.size() * sizeof(unsigned int), meshData.indices.data(), GL_STATIC_DRAW);
    }

    // Konfiguracja atrybutow wierzcholkow
    // Pozycja
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    // Normalna
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    // Wspolrzedne tekstury
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

    // Sprawdzenie, czy atrybut koloru jest dostepny w strukturze Vertex przed jego powiazaniem.
    // Jest to zabezpieczenie, jesli struktura Vertex moglaby miec rozne konfiguracje.
    if (offsetof(Vertex, color) < sizeof(Vertex)) {
        glEnableVertexAttribArray(3); // Kolor
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    }

    glBindVertexArray(0); // Odpiecie VAO po konfiguracji
    Logger::getInstance().debug("MeshRenderer::setupGpuBuffers dla '" + modelNameForLog + "' VAO ID: " + std::to_string(VAO) + ", VBO ID: " + std::to_string(VBO) + (EBO != 0 ? ", EBO ID: " + std::to_string(EBO) : ""));
}

void MeshRenderer::cleanupGpuBuffers(const std::string& modelNameForLog) {
    // Usuwanie buforow w odwrotnej kolejnosci do tworzenia, chociaz tutaj kolejnosc nie jest krytyczna.
    if (EBO != 0) {
        glDeleteBuffers(1, &EBO);
        EBO = 0; // Ustawienie na 0 po usunieciu, aby zapobiec podwojnemu usunieciu.
    }
    if (VBO != 0) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    indexCount = 0; // Resetowanie liczby indeksow.
    Logger::getInstance().debug("MeshRenderer::cleanupGpuBuffers dla '" + modelNameForLog + "' wykonane.");
}

// --- Implementacja Model ---

Model::Model(const std::string& name, std::shared_ptr<ModelAsset> asset, std::shared_ptr<Shader> defaultShader)
    : m_modelName(name),
    m_asset(asset),
    m_shader(defaultShader),
    m_modelMatrix(glm::mat4(1.0f)),
    m_position(0.0f),
    m_rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)), // Kwaternion jednostkowy reprezentuje brak rotacji.
    m_scaleFactor(1.0f),
    m_cameraForLighting(nullptr),
    m_castsShadow(true), // Modele domyslnie rzucaja cienie.
    m_boundingVolume(nullptr), // Inicjalizowane przez setBoundingShape.
    m_activeBoundingShapeType(BoundingShapeType::UNKNOWN), // Ustawiane przez setBoundingShape.
    m_collisionsEnabled(false), // Kolizje domyslnie wylaczone; wlaczane po ustawieniu bryly.
    m_colliderType(ColliderType::STATIC), // Domyslny typ kolidera.
    m_localCylinderRadius(0.0f), // Inicjalizacja pol dla bryly cylindrycznej.
    m_localCylinderP1(0.0f),
    m_localCylinderP2(0.0f) {
    Logger::getInstance().info("Model KONSTRUKTOR: Tworzenie modelu '" + m_modelName + "'.");

    if (!m_asset) {
        Logger::getInstance().error("Model '" + m_modelName + "': Utworzony z pustym zasobem ModelAsset (nullptr)! Model bedzie pusty i niezdolny do renderowania lub kolizji.");
        return; // Wczesne wyjscie, jesli brak kluczowych danych.
    }

    if (!m_shader) {
        Logger::getInstance().warning("Model '" + m_modelName + "': Utworzony bez domyslnego shadera. Renderowanie nie bedzie mozliwe bez recznego ustawienia shadera.");
    }

    initializeMeshRenderers();
    // Ustawienie domyslnej bryly kolizyjnej. ColliderType jest brany z domyslnej wartosci czlonkowskiej.
    setBoundingShape(BoundingShapeType::AABB, m_colliderType);
}

Model::~Model() {
    Logger::getInstance().info("Model DESTRUKTOR: Usuwanie modelu '" + m_modelName + "'.");
    for (size_t i = 0; i < m_meshRenderers.size(); ++i) {
        // Tworzenie unikalnej nazwy dla logowania, aby latwiej bylo zidentyfikowac, ktora siatka jest czyszczona.
        std::string meshLogName = m_modelName + "_mesh_" + std::to_string(i);
        m_meshRenderers[i].cleanupGpuBuffers(meshLogName);
    }
    m_meshRenderers.clear(); // Oczyszczenie wektora, aby zwolnic pamiec.
}

void Model::initializeMeshRenderers() {
    if (!m_asset) { // Zabezpieczenie przed proba dostepu do nullptr.
        Logger::getInstance().warning("Model '" + m_modelName + "': Proba inicjalizacji rendererow siatek bez zasobu ModelAsset.");
        return;
    }
    if (m_asset->meshes.empty()) {
        Logger::getInstance().info("Model '" + m_modelName + "': Zasob ModelAsset nie zawiera siatek. Brak rendererow do inicjalizacji.");
        return;
    }

    m_meshRenderers.resize(m_asset->meshes.size());
    for (size_t i = 0; i < m_asset->meshes.size(); ++i) {
        std::string meshLogName = m_modelName + "_mesh_" + std::to_string(i);
        m_meshRenderers[i].setupGpuBuffers(m_asset->meshes[i], meshLogName);
    }
    Logger::getInstance().info("Model '" + m_modelName + "': Zainicjalizowano " + std::to_string(m_meshRenderers.size()) + " rendererow siatek.");
}

void Model::updateModelMatrix() {
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), m_position);
    glm::mat4 rotationMatrix = glm::mat4_cast(m_rotation);
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), m_scaleFactor);
    m_modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;

    // Aktualizacja bryly kolizyjnej jest konieczna po kazdej zmianie transformacji modelu.
    updateCurrentBoundingVolume();
}

void Model::updateCurrentBoundingVolume() {
    if (!m_boundingVolume) {
        Logger::getInstance().warning("Model '" + m_modelName + "' updateCurrentBoundingVolume: Bryla kolizyjna (m_boundingVolume) jest nullptr. Nie mozna zaktualizowac.");
        return;
    }

    switch (m_activeBoundingShapeType) {
    case BoundingShapeType::AABB: {
        AABB* currentAABB = static_cast<AABB*>(m_boundingVolume.get());
        // Inicjalizacja AABB przed obliczeniem nowych granic.
        currentAABB->minPoint = glm::vec3(std::numeric_limits<float>::max());
        currentAABB->maxPoint = glm::vec3(std::numeric_limits<float>::lowest());
        bool firstVertexProcessed = false;

        if (!m_asset || m_asset->meshes.empty()) {
            Logger::getInstance().debug("Model '" + m_modelName + "' updateCurrentBoundingVolume (AABB): Brak siatek. AABB ustawione na pozycje modelu.");
            currentAABB->minPoint = m_position;
            currentAABB->maxPoint = m_position;
            return;
        }

        // Obliczanie AABB na podstawie wszystkich wierzcholkow modelu, transformowanych do przestrzeni swiata.
        for (const auto& meshData : m_asset->meshes) {
            if (meshData.vertices.empty()) { // Pomijanie pustych siatek.
                continue;
            }
            for (const auto& localVertex : meshData.vertices) {
                glm::vec3 worldPos = glm::vec3(m_modelMatrix * glm::vec4(localVertex.position, 1.0f));

                if (!firstVertexProcessed) { // Inicjalizacja granic pierwszym znalezionym wierzcholkiem.
                    currentAABB->minPoint = worldPos;
                    currentAABB->maxPoint = worldPos;
                    firstVertexProcessed = true;
                }
                else { // Rozszerzanie granic o kolejne wierzcholki.
                    currentAABB->minPoint.x = std::min(currentAABB->minPoint.x, worldPos.x);
                    currentAABB->minPoint.y = std::min(currentAABB->minPoint.y, worldPos.y);
                    currentAABB->minPoint.z = std::min(currentAABB->minPoint.z, worldPos.z);
                    currentAABB->maxPoint.x = std::max(currentAABB->maxPoint.x, worldPos.x);
                    currentAABB->maxPoint.y = std::max(currentAABB->maxPoint.y, worldPos.y);
                    currentAABB->maxPoint.z = std::max(currentAABB->maxPoint.z, worldPos.z);
                }
            }
        }

        // Jesli model mial siatki, ale zadna z nich nie miala wierzcholkow.
        if (!firstVertexProcessed) {
            Logger::getInstance().debug("Model '" + m_modelName + "' updateCurrentBoundingVolume (AABB): Brak wierzcholkow w siatkach. AABB ustawione na pozycje modelu.");
            currentAABB->minPoint = m_position;
            currentAABB->maxPoint = m_position;
        }
        break;
    }
    case BoundingShapeType::CYLINDER: {
        CylinderBV* currentCylinder = static_cast<CylinderBV*>(m_boundingVolume.get());
        // Aktualizacja bryly cylindrycznej przy uzyciu jej lokalnych parametrow i macierzy modelu.
        currentCylinder->updateWorldVolume(m_localCylinderP1, m_localCylinderP2, m_localCylinderRadius, m_modelMatrix);
        break;
    }
    case BoundingShapeType::OBB:
        // TODO: Implementacja aktualizacji OBB dla modelu.
        Logger::getInstance().warning("Model '" + m_modelName + "' updateCurrentBoundingVolume: Typ OBB nie jest jeszcze obslugiwany.");
        break;
    case BoundingShapeType::SPHERE:
        // TODO: Implementacja aktualizacji SphereBV dla modelu. 
        Logger::getInstance().warning("Model '" + m_modelName + "' updateCurrentBoundingVolume: Typ SPHERE nie jest jeszcze obslugiwany.");
        break;
    default:
        Logger::getInstance().warning("Model '" + m_modelName + "' updateCurrentBoundingVolume: Nieznany lub nieobslugiwany typ BoundingShapeType: " + std::to_string(static_cast<int>(m_activeBoundingShapeType)));
        break;
    }
}

// Lokalna funkcja pomocnicza do konwersji ColliderType na string.
// Umieszczona w anonimowej przestrzeni nazw, aby ograniczyc jej zasieg do tego pliku.
namespace {
    std::string localColliderTypeToString(ColliderType type) {
        switch (type) {
        case ColliderType::STATIC:  return "STATIC";
        case ColliderType::DYNAMIC: return "DYNAMIC";
        case ColliderType::TRIGGER: return "TRIGGER";
        default: return "UNKNOWN_COLLIDER_TYPE"; // Powinno byc logowane jako blad, jesli wystapi.
        }
    }
}

void Model::setBoundingShape(BoundingShapeType newShape, ColliderType colliderType, const glm::vec3& p1, const glm::vec3& p2, float r) {
    m_activeBoundingShapeType = newShape;
    m_colliderType = colliderType;

    std::string shapeName = "NIEZNANY"; // Domyslna nazwa ksztaltu dla logowania.

    switch (newShape) {
    case BoundingShapeType::AABB:
        m_boundingVolume = std::make_unique<AABB>();
        shapeName = "AABB";
        // Parametry p1, p2, r sa ignorowane dla AABB, poniewaz jest ono obliczane dynamicznie z wierzcholkow.
        break;
    case BoundingShapeType::CYLINDER:
        // Zapisanie lokalnych parametrow cylindra, ktore beda uzywane do jego aktualizacji.
        m_localCylinderP1 = p1;
        m_localCylinderP2 = p2;
        m_localCylinderRadius = r;
        m_boundingVolume = std::make_unique<CylinderBV>(p1, p2, r); // Inicjalizacja z podanymi (lokalnymi) parametrami.
        shapeName = "CYLINDER";
        // Logowanie ustawionych parametrow lokalnych.
        Logger::getInstance().info("Model '" + m_modelName + "' parametry lokalne cylindra: P1(" +
            std::to_string(p1.x) + "," + std::to_string(p1.y) + "," + std::to_string(p1.z) + "), P2(" +
            std::to_string(p2.x) + "," + std::to_string(p2.y) + "," + std::to_string(p2.z) + "), R=" + std::to_string(r));
        break;
    case BoundingShapeType::OBB:
        // TODO: Implementacja OBB dla modelu. 
        shapeName = "OBB (niezaimplementowany)";
        Logger::getInstance().warning("Model '" + m_modelName + "': Typ OBB nie jest jeszcze obslugiwany. Ustawiono AABB jako fallback.");
        m_activeBoundingShapeType = BoundingShapeType::AABB; // Bezpieczny fallback.
        m_boundingVolume = std::make_unique<AABB>();
        break;
    case BoundingShapeType::SPHERE:
        // TODO: Implementacja SPHERE dla modelu. 
        shapeName = "SPHERE (niezaimplementowany)";
        Logger::getInstance().warning("Model '" + m_modelName + "': Typ SPHERE nie jest jeszcze obslugiwany. Ustawiono AABB jako fallback.");
        m_activeBoundingShapeType = BoundingShapeType::AABB; // Bezpieczny fallback.
        m_boundingVolume = std::make_unique<AABB>();
        break;
    default:
        Logger::getInstance().error("Model '" + m_modelName + "' setBoundingShape: Proba ustawienia nieznanego typu BoundingShapeType. Ustawiono AABB jako fallback.");
        m_activeBoundingShapeType = BoundingShapeType::AABB; // Zapewnienie, ze zawsze jest jakas bryla.
        m_boundingVolume = std::make_unique<AABB>();
        shapeName = "AABB (Fallback)";
        break;
    }
    Logger::getInstance().info("Model '" + m_modelName + "': Bryla kolizyjna ustawiona na " + shapeName + ", typ kolidera: " + localColliderTypeToString(m_colliderType));

    setCollisionsEnabled(true); // Wlaczenie kolizji jest sensowne po zdefiniowaniu bryly.
    updateCurrentBoundingVolume(); // Natychmiastowa aktualizacja nowo utworzonej bryly.
}

void Model::render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    if (!m_shader) { // Wczesne wyjscie, jesli shader nie jest ustawiony.
        return;
    }
    if (m_meshRenderers.empty()) { // Wczesne wyjscie, jesli nie ma siatek do renderowania.
        return;
    }

    m_shader->use();
    m_shader->setMat4("model", m_modelMatrix);
    m_shader->setMat4("view", viewMatrix);
    m_shader->setMat4("projection", projectionMatrix);

    // Ustawienie pozycji kamery dla celow oswietleniowych (np. obliczanie wektora widoku).
    if (m_cameraForLighting) {
        m_shader->setVec3("viewPos_world", m_cameraForLighting->getPosition());
    }
    else {
        m_shader->setVec3("viewPos_world", glm::vec3(0.0f)); // Fallback, jesli kamera nie jest dostepna.
    }

    for (const auto& meshRenderer : m_meshRenderers) {
        if (meshRenderer.VAO == 0) { // Pominiecie siatek, ktore nie maja poprawnie skonfigurowanego VAO.
            continue;
        }

        // Ustawienie wlasciwosci materialu specyficznych dla danej siatki.
        const Material& mat = meshRenderer.materialProperties;
        m_shader->setVec3("material.ambient", mat.ambient);
        m_shader->setVec3("material.diffuse", mat.diffuse);
        m_shader->setVec3("material.specular", mat.specular);
        m_shader->setFloat("material.shininess", mat.shininess);

        // Obsluga tekstury diffuse.
        bool useDiffuseTexture = false;
        if (mat.diffuseTexture && mat.diffuseTexture->ID != 0) {
            glActiveTexture(GL_TEXTURE0); // Aktywacja jednostki teksturujacej dla diffuse.
            glBindTexture(GL_TEXTURE_2D, mat.diffuseTexture->ID);
            m_shader->setInt("material.diffuseTexture", 0); // Informowanie shadera, ze sampler diffuse ma uzywac jednostki 0.
            useDiffuseTexture = true;
        }
        m_shader->setBool("material.useDiffuseTexture", useDiffuseTexture); // Informowanie shadera, czy ma uzyc tekstury diffuse.

        // Obsluga tekstury specular.
        bool useSpecularTexture = false;
        if (mat.specularTexture && mat.specularTexture->ID != 0) {
            glActiveTexture(GL_TEXTURE1); // Aktywacja jednostki teksturujacej dla specular.
            glBindTexture(GL_TEXTURE_2D, mat.specularTexture->ID);
            m_shader->setInt("material.specularTexture", 1); // Informowanie shadera, ze sampler specular ma uzywac jednostki 1.
            useSpecularTexture = true;
        }
        m_shader->setBool("material.useSpecularTexture", useSpecularTexture); // Informowanie shadera, czy ma uzyc tekstury specular.

        // Renderowanie siatki.
        glBindVertexArray(meshRenderer.VAO);
        if (meshRenderer.indexCount > 0) {
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(meshRenderer.indexCount), GL_UNSIGNED_INT, 0);
        }
        // Renderowanie bez indeksow nie jest tutaj obslugiwane, poniewaz setupGpuBuffers oczekuje ich.
        glBindVertexArray(0); // Odpiecie VAO po renderowaniu siatki.
    }
    // Resetowanie stanow OpenGL po renderowaniu calego modelu jest dobra praktyka.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0); // Odbindowanie tekstury z jednostki 0.
    // Jesli byly uzywane inne jednostki tekstur, je rowniez nalezaloby zresetowac.
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0); // Odbindowanie tekstury z jednostki 1.
    glActiveTexture(GL_TEXTURE0); // Ustawienie domyslnej aktywnej jednostki na 0.
}

void Model::renderForDepthPass(Shader* depthShader) {
    if (!depthShader) { // Wczesne wyjscie, jesli brak shadera glebokosci.
        Logger::getInstance().warning("Model::renderForDepthPass() dla '" + m_modelName + "': Brak depthShader!");
        return;
    }
    if (m_meshRenderers.empty()) { // Wczesne wyjscie, jesli brak siatek.
        return;
    }

    depthShader->use();
    depthShader->setMat4("model", m_modelMatrix);
    // Dla przebiegu glebokosci zwykle nie sa potrzebne informacje o materiale czy teksturach,
    // chyba ze shader obsluguje np. alpha testing.

    for (const auto& meshRenderer : m_meshRenderers) {
        if (meshRenderer.VAO == 0) { // Pominiecie niepoprawnie skonfigurowanych siatek.
            continue;
        }
        glBindVertexArray(meshRenderer.VAO);
        if (meshRenderer.indexCount > 0) {
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(meshRenderer.indexCount), GL_UNSIGNED_INT, 0);
        }
        // Podobnie jak w render(), pomijamy renderowanie bez indeksow.
        glBindVertexArray(0);
    }
}

bool Model::castsShadow() const {
    return m_castsShadow;
}

void Model::setCastsShadow(bool castsShadow) {
    m_castsShadow = castsShadow;
}

BoundingVolume* Model::getBoundingVolume() {
    // Bryla kolizyjna jest aktualizowana przez updateModelMatrix -> updateCurrentBoundingVolume.
    // Ta metoda tylko zwraca do niej dostep.
    return m_boundingVolume.get();
}

const BoundingVolume* Model::getBoundingVolume() const {
    return m_boundingVolume.get();
}

void Model::setCollisionsEnabled(bool enabled) {
    m_collisionsEnabled = enabled;
}

bool Model::collisionsEnabled() const {
    return m_collisionsEnabled;
}

void Model::setPosition(const glm::vec3& position) {
    m_position = position;
    updateModelMatrix(); // Zmiana pozycji wymaga aktualizacji macierzy modelu i bryly kolizyjnej.
}

void Model::setRotation(const glm::quat& rotation) {
    m_rotation = glm::normalize(rotation); // Normalizacja kwaternionu zapobiega problemom numerycznym.
    updateModelMatrix(); // Zmiana rotacji wymaga aktualizacji.
}

void Model::setScale(const glm::vec3& scale) {
    m_scaleFactor = scale;
    updateModelMatrix(); // Zmiana skali wymaga aktualizacji.
}

void Model::setShader(std::shared_ptr<Shader> shader) {
    if (shader) {
        m_shader = shader;
        Logger::getInstance().info("Model '" + m_modelName + "' shader ustawiony na: " + (shader->getName().empty() ? "[Nienazwany Shader]" : shader->getName()));
    }
    else {
        m_shader = nullptr; // Umozliwia wylaczenie renderowania przez ustawienie shadera na nullptr.
        Logger::getInstance().warning("Model '" + m_modelName + "': Ustawiono pusty shader (nullptr). Model nie bedzie renderowany.");
    }
}