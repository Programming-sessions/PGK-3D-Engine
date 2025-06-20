#include "Primitives.h"
#include "Logger.h"
#include "ResourceManager.h" // Potrzebne dla Texture, jeśli nie jest w Lighting.h (jest) i dla Shader
#include "Shader.h"
#include "Engine.h"          // Potrzebne dla getInstance()->getCamera()
#include "Camera.h"          // Potrzebne dla Camera*
#include "Texture.h"         // Bezpośrednie dołączenie definicji Texture jest dobre dla jasności

#include <stddef.h> // Dla offsetof
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp> // Dla glm::pi
#include <limits>                // Dla std::numeric_limits (nieużywane bezpośrednio tutaj, ale może być w BoundingVolume)

// Definicje stalych uzywanych do sprawdzania, czy kolor zostal przekazany w konstruktorze
// Zgodnie z PDF, nazwy stalych powinny byc UPPER_SNAKE_CASE
const glm::vec4 PRGR_DEFAULT_CONSTRUCTOR_COLOR = glm::vec4(1.00001f, 1.00002f, 1.00003f, 1.00004f); // Unikalna wartosc do porownan

//=================================================================================================
// Implementacja BasePrimitive
//=================================================================================================

BasePrimitive::BasePrimitive()
    : m_VAO(0), m_VBO(0), m_EBO(0), m_shaderProgram(nullptr),
    m_modelMatrix(glm::mat4(1.0f)),
    m_position(0.0f),
    m_rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),
    m_scaleFactor(1.0f),
    m_useFlatShading(false),
    m_castsShadow(true),
    m_boundingVolume(nullptr),
    m_isBoundingVolumeDirty(true),
    m_collisionsEnabled(true) {
    // Probba pobrania domyslnego shadera z ResourceManager
    m_shaderProgram = ResourceManager::getInstance().getShader("defaultPrimitiveShader");
    if (!m_shaderProgram) {
        Logger::getInstance().info("BasePrimitive: Domyslny shader prymitywow 'defaultPrimitiveShader' nie znaleziony, proba zaladowania...");
        // Sciezki do shaderow powinny byc konfigurowalne lub latwo dostepne jako stale
        m_shaderProgram = ResourceManager::getInstance().loadShader(
            "defaultPrimitiveShader",
            "assets/shaders/default_shader.vert",
            "assets/shaders/default_shader.frag"
        );
    }

    if (!m_shaderProgram) {
        Logger::getInstance().error("BasePrimitive: Nie udalo sie zaladowac lub pobrac domyslnego shadera.");
        // W tym miejscu moznaby rozwazyc rzucenie wyjatku lub ustawienie flagi bledu,
        // poniewaz renderowanie bez shadera bedzie niemozliwe.
    }
    // Domyslne wartosci materialu sa inicjalizowane w konstruktorze Material (Lighting.h)
}

BasePrimitive::~BasePrimitive() {
    clearBuffers(); // Zwolnienie zasobow OpenGL
}

void BasePrimitive::recomposeModelMatrix() {
    glm::mat4 trans = glm::translate(glm::mat4(1.0f), m_position);
    glm::mat4 rot = glm::mat4_cast(m_rotation);
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), m_scaleFactor); // Nazwa zmiennej zmieniona dla jasności
    m_modelMatrix = trans * rot * scaleMat;
    m_isBoundingVolumeDirty = true; // Kazda zmiana transformacji powinna oznaczyc BV jako "brudny"
}

void BasePrimitive::setPosition(const glm::vec3& position) {
    m_position = position;
    recomposeModelMatrix();
}

void BasePrimitive::setRotation(const glm::quat& rotation) {
    m_rotation = rotation;
    recomposeModelMatrix();
}

void BasePrimitive::setScale(const glm::vec3& scale) {
    m_scaleFactor = scale;
    recomposeModelMatrix();
}

void BasePrimitive::setTransform(const glm::mat4& modelMatrix) {
    m_modelMatrix = modelMatrix;
    // TODO: Nalezaloby zaktualizowac m_position, m_rotation, m_scaleFactor, jesli to mozliwe,
    // lub przynajmniej udokumentowac, ze bezposrednie ustawienie macierzy modelu
    // omija te skladowe i moze prowadzic do niespojnoci, jesli sa one pozniej uzywane.
    // Na razie, oznaczamy tylko BV jako brudny.
    m_isBoundingVolumeDirty = true;
}

void BasePrimitive::setShader(std::shared_ptr<Shader> shader) {
    if (shader) {
        m_shaderProgram = shader;
    }
    else {
        Logger::getInstance().warning("BasePrimitive: Probba ustawienia pustego shadera (nullptr).");
    }
}

void BasePrimitive::setMaterial(const Material& material) {
    m_material = material;
}

void BasePrimitive::setMaterialAmbient(const glm::vec3& ambient) {
    m_material.ambient = ambient;
}

void BasePrimitive::setMaterialDiffuse(const glm::vec3& diffuse) {
    m_material.diffuse = diffuse;
}

void BasePrimitive::setMaterialSpecular(const glm::vec3& specular) {
    m_material.specular = specular;
}

void BasePrimitive::setMaterialShininess(float shininess) {
    m_material.shininess = shininess;
}

void BasePrimitive::setDiffuseTexture(std::shared_ptr<Texture> texture) {
    m_material.diffuseTexture = texture;
}

void BasePrimitive::setSpecularTexture(std::shared_ptr<Texture> texture) {
    m_material.specularTexture = texture;
}

void BasePrimitive::setCastsShadow(bool castsShadow) {
    m_castsShadow = castsShadow;
}

bool BasePrimitive::castsShadow() const {
    return m_castsShadow;
}

void BasePrimitive::setUseFlatShading(bool useFlat) {
    m_useFlatShading = useFlat;
}

bool BasePrimitive::getUseFlatShading() const {
    return m_useFlatShading;
}

void BasePrimitive::clearBuffers() {
    // Usuwanie buforow w odwrotnej kolejnosci do tworzenia (chociaz tutaj kolejnosc nie ma krytycznego znaczenia)
    if (m_EBO != 0) {
        glDeleteBuffers(1, &m_EBO);
        m_EBO = 0;
    }
    if (m_VBO != 0) {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }
}

void BasePrimitive::setupMesh() {
    clearBuffers(); // Najpierw czyscimy stare bufory, jesli istnieja

    if (m_vertices.empty()) {
        Logger::getInstance().warning("BasePrimitive::setupMesh: Wywolano z pustym wektorem wierzcholkow. Mesh nie zostanie stworzony.");
        // Jesli bounding volume istnieje, powinno zostac oznaczone jako brudne lub zresetowane
        if (m_boundingVolume) {
            m_isBoundingVolumeDirty = true;
        }
        return;
    }

    //    Generowanie identyfikatorów dla buforów OpenGL
    //    ----------------------------------------------
    //    Prosimy OpenGL o wygenerowanie unikalnych identyfikatorów (nazw) dla naszych obiektów:
    //    - VAO (Vertex Array Object): Przechowuje konfigurację stanu potrzebną do rysowania.
    //    - VBO (Vertex Buffer Object): Przechowuje dane samych wierzchołków (pozycje, normalne, kolory itd.).
    //    - EBO (Element Buffer Object / Index Buffer Object): Przechowuje indeksy, które definiują,
    //      które wierzchołki tworzą poszczególne trójkąty (jeśli jest używany).
    glGenVertexArrays(1, &m_VAO); // Generuj 1 VAO i zapisz jego ID w m_VAO
    glGenBuffers(1, &m_VBO);      // Generuj 1 VBO i zapisz jego ID w m_VBO
    if (!m_indices.empty()) {     // Jeśli używamy indeksów do rysowania
        glGenBuffers(1, &m_EBO);  // Generuj 1 EBO i zapisz jego ID w m_EBO
    }

   //    Konfiguracja VAO i VBO
   //    -----------------------
   //    Teraz będziemy konfigurować, jak dane wierzchołków są zorganizowane.
   //    Najpierw aktywujemy bindujemy nasz VAO. Wszystkie kolejne operacje na VBO
   //    i konfiguracje atrybutów wierzchołków będą powiązane z tym VAO.
    glBindVertexArray(m_VAO); // Aktywuj VAO. Od teraz konfigurujemy stan zapisany w tym VAO.

   
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO); // Aktywuj VBO jako bieżący bufor dla danych wierzchołków.
    // Kopiujemy dane wierzchołków z pamięci RAM (z wektora m_vertices) do pamięci VRAM (na karcie graficznej),
   // czyli do wcześniej utworzonego VBO.
   // - GL_ARRAY_BUFFER: Typ bufora (dane wierzchołków).
   // - m_vertices.size() * sizeof(Vertex): Całkowity rozmiar danych w bajtach.
   // - m_vertices.data(): Wskaźnik na początek danych w wektorze.
   // - GL_STATIC_DRAW: Wskazówka dla OpenGL, że dane te prawdopodobnie nie będą często zmieniane.
   //   OpenGL może dzięki temu zoptymalizować przechowywanie i dostęp do tych danych.
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), m_vertices.data(), GL_STATIC_DRAW); 

    //     Konfiguracja EBO (Element Buffer Object / Index Buffer Object) - jeśli jest używany
    //     ---------------------------------------------------------------------------------
    //     Jeśli prymityw używa indeksów (m_indices nie jest pusty), konfigurujemy EBO.
    if (!m_indices.empty()) { //
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO); // Aktywuj EBO jako bieżący bufor dla indeksów.
        // Kopiujemy dane indeksów z pamięci RAM (z wektora m_indices) do pamięci VRAM (do EBO).
        // - GL_ELEMENT_ARRAY_BUFFER: Typ bufora (dane indeksów).
        // - m_indices.size() * sizeof(GLuint): Całkowity rozmiar danych indeksów w bajtach.
        // - m_indices.data(): Wskaźnik na początek danych w wektorze indeksów.
        // - GL_STATIC_DRAW: Podobnie jak przy VBO, wskazówka o rzadkich zmianach.
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(GLuint), m_indices.data(), GL_STATIC_DRAW); //
    }

    //    Ustawienie atrybutów wierzchołków
    //    ----------------------------------
    //    Teraz musimy powiedzieć OpenGL, jak interpretować surowe dane w VBO.
    //    Nasza struktura 'Vertex' (z Primitives.h) zawiera pozycję, normalną, współrzędne tekstury i kolor.
    //    Dla każdego z tych elementów (atrybutów) musimy:
    //    a) Włączyć dany slot atrybutu (glEnableVertexAttribArray).
    //    b) Określić, jak dane dla tego atrybutu są ułożone w VBO (glVertexAttribPointer).

    // Atrybut 0: Pozycja (position)
    // -----------------------------
    // Włączamy slot atrybutu 0. W shaderze będziemy odnosić się do tego jako 'layout (location = 0)'.
    glEnableVertexAttribArray(0); //
    // Definiujemy format danych dla atrybutu 0:
    // - Index: 0 (numer slotu atrybutu, odpowiadający glEnableVertexAttribArray(0)).
    // - Size: 3 (pozycja składa się z 3 komponentów: x, y, z).
    // - Type: GL_FLOAT (komponenty są typu float).
    // - Normalized: GL_FALSE (dane nie powinny być normalizowane - wartości float nie są zwykle normalizowane w ten sposób).
    // - Stride: sizeof(Vertex) (odległość w bajtach między początkiem danych dla jednego wierzchołka
    //   a początkiem danych dla następnego wierzchołka. To jest rozmiar całej struktury Vertex).
    // - Pointer: (void*)offsetof(Vertex, position) (przesunięcie w bajtach od początku struktury Vertex
    //   do składowej 'position'. Makro offsetof jest tu bardzo pomocne).
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position)); //

    // Atrybut 1: Normalna (normal)
    // ---------------------------
    glEnableVertexAttribArray(1); //
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal)); //

    // Atrybut 2: Współrzędne tekstury (texCoords)
    // -------------------------------------------
    // (składają się z 2 komponentów: u, v)
    glEnableVertexAttribArray(2); //
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords)); //

    // Atrybut 3: Kolor (color)
    // ------------------------
    // (składa się z 4 komponentów: r, g, b, a)
    glEnableVertexAttribArray(3); //
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color)); //

    //    Odpięcie (unbind) obiektów OpenGL
    //    ----------------------------------
    //    Dobrą praktyką jest odpięcie obiektów po zakończeniu ich konfiguracji,
    //    aby uniknąć przypadkowych modyfikacji w innych częściach kodu.
    //    VAO(0) oznacza odpięcie aktualnie związanego VAO.
    glBindVertexArray(0); // Odpięcie VAO.
    // Podobnie odpinamy VBO i EBO (jeśli był używany).
    glBindBuffer(GL_ARRAY_BUFFER, 0); // Odpięcie VBO.
    if (!m_indices.empty()) { //
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // Odpięcie EBO.
    }

    m_isBoundingVolumeDirty = true; // Mesh sie zmienil, wiec BV jest nieaktualne
}

void BasePrimitive::translate(const glm::vec3& translation) {
    m_position += translation;
    recomposeModelMatrix();
}

void BasePrimitive::rotate(float angleDegrees, const glm::vec3& axis) { // Zmieniono nazwe parametru dla jasnosci
    m_rotation = glm::rotate(m_rotation, glm::radians(angleDegrees), axis);
    recomposeModelMatrix();
}

void BasePrimitive::scale(const glm::vec3& scaleFactorMultiplier) { // Zmieniono nazwe parametru dla jasnosci
    m_scaleFactor *= scaleFactorMultiplier; // Mnozenie obecnej skali przez nowy czynnik
    recomposeModelMatrix();
}

void BasePrimitive::render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    Camera* cam = nullptr;
    // Sprawdzenie, czy Engine jest dostepny i zainicjalizowany, aby uniknac problemow przy zamykaniu aplikacji
    if (Engine::getInstance()->isInitialized()) {
        cam = Engine::getInstance()->getCamera();
    }
    this->render(viewMatrix, projectionMatrix, cam); // Wywolanie drugiej wersji metody render
}

void BasePrimitive::render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, Camera* camera) {
    if (m_VAO == 0 || !m_shaderProgram) {
        // Nie mozna renderowac bez VAO lub shadera
        // Logger moglby tutaj ostrzec, jesli to nieoczekiwana sytuacja
        return;
    }

    m_shaderProgram->use();
    m_shaderProgram->setMat4("model", m_modelMatrix);
    m_shaderProgram->setMat4("view", viewMatrix);
    m_shaderProgram->setMat4("projection", projectionMatrix);

    // Ustawianie wlasciwosci materialu (kolory i polyskliwosc jako fallback)
    m_shaderProgram->setVec3("material.ambient", m_material.ambient);
    m_shaderProgram->setVec3("material.diffuse", m_material.diffuse);
    m_shaderProgram->setVec3("material.specular", m_material.specular);
    m_shaderProgram->setFloat("material.shininess", m_material.shininess);

    // Obsluga tekstury diffuse
    if (m_material.diffuseTexture && m_material.diffuseTexture->ID != 0) {
        glActiveTexture(GL_TEXTURE0); // Aktywacja jednostki teksturujacej 0
        glBindTexture(GL_TEXTURE_2D, m_material.diffuseTexture->ID);
        m_shaderProgram->setInt("material.diffuseTexture", 0); // Przekazanie samplerowi jednostki 0
        m_shaderProgram->setBool("material.useDiffuseTexture", true);
    }
    else {
        m_shaderProgram->setBool("material.useDiffuseTexture", false);
        // Opcjonalnie: Mozna zbindowac domyslna (np. biala 1x1) teksture do GL_TEXTURE0,
        // aby uniknac potencjalnych problemow na niektorych sterownikach.
        // glActiveTexture(GL_TEXTURE0);
        // glBindTexture(GL_TEXTURE_2D, 0); // lub ID domyslnej tekstury, jesli taka istnieje w ResourceManager
    }

    // Obsluga tekstury specular
    if (m_material.specularTexture && m_material.specularTexture->ID != 0) {
        glActiveTexture(GL_TEXTURE1); // Aktywacja jednostki teksturujacej 1
        glBindTexture(GL_TEXTURE_2D, m_material.specularTexture->ID);
        m_shaderProgram->setInt("material.specularTexture", 1); // Przekazanie samplerowi jednostki 1
        m_shaderProgram->setBool("material.useSpecularTexture", true);
    }
    else {
        m_shaderProgram->setBool("material.useSpecularTexture", false);
        // Podobnie jak dla diffuse, mozna rozwazyc bindowanie domyslnej tekstury.
        // glActiveTexture(GL_TEXTURE1);
        // glBindTexture(GL_TEXTURE_2D, 0); 
    }

    m_shaderProgram->setBool("u_useFlatShading", m_useFlatShading);

    if (camera) {
        m_shaderProgram->setVec3("viewPos_world", camera->getPosition());
    }
    else {
        // Fallback, jesli kamera nie jest dostepna (np. podczas zamykania lub testow)
        m_shaderProgram->setVec3("viewPos_world", glm::vec3(0.0f));
    }

    // Kwestia ustawiania uniformow dla swiatel:
    // Zakladamy, ze LightingManager  jest odpowiedzialny za
    // globalne ustawienie danych o swietle w shaderach, ktore tego wymagaja.

    glBindVertexArray(m_VAO);
    if (!m_indices.empty()) {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, 0);
    }
    else {
        // Rysowanie bez EBO (np. gdy kazde 3 wierzcholki tworza trojkat)
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
    }
    glBindVertexArray(0); // Odpiecie VAO

    // Dobra praktyka: zresetowanie aktywnej jednostki tekstury do domyslnej, aby uniknac wplywu na inne operacje.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0); // Odbindowanie tekstury z jednostki 0
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0); // Odbindowanie tekstury z jednostki 1
    glActiveTexture(GL_TEXTURE0); // Powrot do jednostki 0 jako domyslnej
}

void BasePrimitive::renderForDepthPass(Shader* depthShader) {
    if (m_VAO == 0 || !depthShader) {
        return; // Nie mozna renderowac bez VAO lub shadera glebokosci
    }

    depthShader->use();
    depthShader->setMat4("model", m_modelMatrix);
    // Dla map szesciennych (point light shadows), shader glebokosci moze wymagac dodatkowych uniformow,
    // np. lightPos, farPlane, macierzy lightSpaceMatrices.
    // Dla zwyklych map glebokosci (directional/spot), sama macierz modelu moze wystarczyc,
    // jesli macierze view/projection sa ustawiane globalnie dla calego depth pass.

    glBindVertexArray(m_VAO);
    if (!m_indices.empty()) {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, 0);
    }
    else {
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
    }
    glBindVertexArray(0);
}

BoundingVolume* BasePrimitive::getBoundingVolume() {
    // Jesli BV jest "brudne" i istnieje, aktualizujemy je.
    // Metoda updateWorldBoundingVolume() jest wirtualna i implementowana przez klasy pochodne.
    if (m_isBoundingVolumeDirty && m_boundingVolume) {
        updateWorldBoundingVolume();
        m_isBoundingVolumeDirty = false; // BV jest teraz aktualne
    }
    return m_boundingVolume.get();
}

const BoundingVolume* BasePrimitive::getBoundingVolume() const {
    // W wersji const nie powinnismy modyfikowac m_isBoundingVolumeDirty ani wywolywac updateWorldBoundingVolume().
    // Zakladamy, ze jesli potrzebna jest aktualna wersja BV, nie-const wersja getBoundingVolume()
    // zostala wczesniej wywolana.
    // Jesli flaga 'dirty' mialaby byc modyfikowalna w metodzie const, musialaby byc 'mutable'.
    // if (m_isBoundingVolumeDirty && m_boundingVolume) { // To by wymagalo mutable
    //     const_cast<BasePrimitive*>(this)->updateWorldBoundingVolume();
    //     const_cast<BasePrimitive*>(this)->m_isBoundingVolumeDirty = false;
    // }
    return m_boundingVolume.get();
}

void BasePrimitive::setCollisionsEnabled(bool enabled) {
    m_collisionsEnabled = enabled;
}

bool BasePrimitive::collisionsEnabled() const {
    return m_collisionsEnabled;
}

//=================================================================================================
// Implementacja Cube
//=================================================================================================

Cube::Cube(const glm::vec3& centerPosition, float sideLength, const glm::vec4& cubeColor)
    : BasePrimitive(), m_sideLength(sideLength) {

    m_position = centerPosition; // Ustawienie pozycji z BasePrimitive

    // Inicjalizacja pozostalych transformacji (rotacja, skala) w BasePrimitive::recomposeModelMatrix()
    // Definicja lokalnych wlasciwosci OBB dla szescianu
    m_localCenterOffset = glm::vec3(0.0f); // Srodek szescianu jest jego lokalnym (0,0,0)
    m_localHalfExtents = glm::vec3(sideLength / 2.0f);
    m_boundingVolume = std::make_unique<OBB>(); // Szescian naturalnie uzywa OBB

    // Ustawienie koloru, jesli zostal przekazany inny niz domyslny placeholder
    if (cubeColor != PRGR_DEFAULT_CONSTRUCTOR_COLOR) {
        m_material.diffuse = glm::vec3(cubeColor.r, cubeColor.g, cubeColor.b);
        // Mozna tez ustawic m_material.ambient, np. na polowe diffuse, jesli nie ma innych zalozen
        // m_material.ambient = glm::vec3(cubeColor.r, cubeColor.g, cubeColor.b) * 0.5f;
    }
    // Domyslne wartosci m_material (np. specular, shininess) sa ustawiane w konstruktorze Material lub BasePrimitive

    float halfSide = sideLength / 2.0f;

    // Wierzcholki definiowane sa wokol lokalnego (0,0,0)
    m_vertices = {
		// Format:          {position,                    normal,                      texCoords,           color}
        // Sciana przednia (+Z)
        {glm::vec3(-halfSide, -halfSide,  halfSide), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f), cubeColor},
        {glm::vec3(halfSide, -halfSide,  halfSide), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f), cubeColor},
        {glm::vec3(halfSide,  halfSide,  halfSide), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f), cubeColor},
        {glm::vec3(-halfSide,  halfSide,  halfSide), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f), cubeColor},
        // Sciana tylna (-Z)
        {glm::vec3(-halfSide, -halfSide, -halfSide), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 0.0f), cubeColor},
        {glm::vec3(halfSide, -halfSide, -halfSide), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 0.0f), cubeColor},
        {glm::vec3(halfSide,  halfSide, -halfSide), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 1.0f), cubeColor},
        {glm::vec3(-halfSide,  halfSide, -halfSide), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 1.0f), cubeColor},
        // Sciana gorna (+Y)
        {glm::vec3(-halfSide,  halfSide,  halfSide), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f), cubeColor},
        {glm::vec3(halfSide,  halfSide,  halfSide), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f), cubeColor},
        {glm::vec3(halfSide,  halfSide, -halfSide), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f), cubeColor},
        {glm::vec3(-halfSide,  halfSide, -halfSide), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f), cubeColor},
        // Sciana dolna (-Y)
        {glm::vec3(-halfSide, -halfSide, -halfSide), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f), cubeColor},
        {glm::vec3(halfSide, -halfSide, -halfSide), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f), cubeColor},
        {glm::vec3(halfSide, -halfSide,  halfSide), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 1.0f), cubeColor},
        {glm::vec3(-halfSide, -halfSide,  halfSide), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f), cubeColor},
        // Sciana prawa (+X)
        {glm::vec3(halfSide, -halfSide,  halfSide), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f), cubeColor},
        {glm::vec3(halfSide, -halfSide, -halfSide), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f), cubeColor},
        {glm::vec3(halfSide,  halfSide, -halfSide), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f), cubeColor},
        {glm::vec3(halfSide,  halfSide,  halfSide), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f), cubeColor},
        // Sciana lewa (-X)
        {glm::vec3(-halfSide, -halfSide, -halfSide), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f), cubeColor},
        {glm::vec3(-halfSide, -halfSide,  halfSide), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f), cubeColor},
        {glm::vec3(-halfSide,  halfSide,  halfSide), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f), cubeColor},
        {glm::vec3(-halfSide,  halfSide, -halfSide), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f), cubeColor}
    };

    m_indices = {
         0,  1,  2,  0,  2,  3, // Przednia
         4,  5,  6,  4,  6,  7, // Tylna
         8,  9, 10,  8, 10, 11, // Gorna
        12, 13, 14, 12, 14, 15, // Dolna
        16, 17, 18, 16, 18, 19, // Prawa
        20, 21, 22, 20, 22, 23  // Lewa
    };

    recomposeModelMatrix(); // Upewnienie sie, ze macierz modelu jest aktualna po ustawieniu pozycji
    setupMesh(); // Utworzenie buforow OpenGL
    updateWorldBoundingVolume(); // Inicjalna aktualizacja BV
    m_isBoundingVolumeDirty = false; // BV jest teraz aktualne
}

void Cube::updateWorldBoundingVolume() {
    if (m_boundingVolume && m_boundingVolume->getType() == BoundingShapeType::OBB) {
        OBB* obb = static_cast<OBB*>(m_boundingVolume.get());
        // Aktualizacja OBB na podstawie lokalnych wymiarow i macierzy modelu
        obb->updateWorldVolume(m_localCenterOffset, m_localHalfExtents, getModelMatrix());
    }
    else {
        // Ten przypadek nie powinien sie zdarzyc, jesli konstruktor poprawnie tworzy OBB.
        // Jesli m_boundingVolume jest nullem lub innego typu, logujemy blad.
        Logger::getInstance().error("Cube::updateWorldBoundingVolume: BoundingVolume nie jest typu OBB lub jest nullptr.");
        // Mozna rozwazyc probe (re)stworzenia OBB, jesli jest null.
        if (!m_boundingVolume) {
            Logger::getInstance().warning("Cube::updateWorldBoundingVolume: Proba stworzenia domyslnego OBB.");
            m_boundingVolume = std::make_unique<OBB>();
            // Po stworzeniu, nalezaloby je zaktualizowac, jesli to mozliwe
            // static_cast<OBB*>(m_boundingVolume.get())->updateWorldVolume(m_localCenterOffset, m_localHalfExtents, getModelMatrix());
        }
    }
}

//=================================================================================================
// Implementacja Plane
//=================================================================================================

Plane::Plane(const glm::vec3& centerPosition, float p_width, float p_depth, const glm::vec4& planeColor, float thickness)
    : BasePrimitive(), m_width(p_width), m_depth(p_depth) {
    m_position = centerPosition;
    m_visualNormal = glm::vec3(0.0f, 1.0f, 0.0f); // Plaszczyzna domyslnie skierowana w gore osi Y

    // Definicja lokalnych wlasciwosci OBB dla plaszczyzny
    // Plaszczyzna jest traktowana jako bardzo cienki prostopadloscian dla celow kolizji
    m_localCenterOffset = glm::vec3(0.0f);
    m_localHalfExtents = glm::vec3(m_width / 2.0f, thickness / 2.0f, m_depth / 2.0f);
    m_boundingVolume = std::make_unique<OBB>();

    if (planeColor != PRGR_DEFAULT_CONSTRUCTOR_COLOR) {
        m_material.diffuse = glm::vec3(planeColor.r, planeColor.g, planeColor.b);
    }

    float halfWidth = m_width / 2.0f;
    float halfDepth = m_depth / 2.0f;

    // Wierzcholki plaszczyzny (Y=0 w ukladzie lokalnym)
    m_vertices = {
        {glm::vec3(-halfWidth, 0.0f,  halfDepth), m_visualNormal, glm::vec2(0.0f, 0.0f), planeColor}, // Lewy-przedni
        {glm::vec3(halfWidth, 0.0f,  halfDepth), m_visualNormal, glm::vec2(1.0f, 0.0f), planeColor}, // Prawy-przedni
        {glm::vec3(halfWidth, 0.0f, -halfDepth), m_visualNormal, glm::vec2(1.0f, 1.0f), planeColor}, // Prawy-tylny
        {glm::vec3(-halfWidth, 0.0f, -halfDepth), m_visualNormal, glm::vec2(0.0f, 1.0f), planeColor}  // Lewy-tylny
    };
    m_indices = {
        0, 1, 2, // Pierwszy trojkat
        0, 2, 3  // Drugi trojkat
    };

    recomposeModelMatrix();
    setupMesh();
    updateWorldBoundingVolume();
    m_isBoundingVolumeDirty = false;
}


void Plane::updateWorldBoundingVolume() {
    if (m_boundingVolume && m_boundingVolume->getType() == BoundingShapeType::OBB) {
        OBB* obb = static_cast<OBB*>(m_boundingVolume.get());
        obb->updateWorldVolume(m_localCenterOffset, m_localHalfExtents, getModelMatrix());
    }
    else {
        Logger::getInstance().error("Plane::updateWorldBoundingVolume: BoundingVolume nie jest typu OBB lub jest nullptr.");
        if (!m_boundingVolume) {
            Logger::getInstance().warning("Plane::updateWorldBoundingVolume: Proba stworzenia domyslnego OBB.");
            m_boundingVolume = std::make_unique<OBB>();
            // static_cast<OBB*>(m_boundingVolume.get())->updateWorldVolume(m_localCenterOffset, m_localHalfExtents, getModelMatrix());
        }
    }
}

//=================================================================================================
// Implementacja Sphere
//=================================================================================================

Sphere::Sphere(const glm::vec3& centerPosition, float radius, int longitudeSegments, int latitudeSegments, const glm::vec4& sphereColor)
    : BasePrimitive(), m_radius(radius) {
    m_position = centerPosition;

    // UWAGA: Sphere powinna uzywac SphereBV dla dokladniejszych kolizji.
    // Na razie AABB jako placeholder.
    // TODO: Zaimplementowac i uzyc SphereBV.
    m_boundingVolume = std::make_unique<AABB>();

    if (sphereColor != PRGR_DEFAULT_CONSTRUCTOR_COLOR) {
        m_material.diffuse = glm::vec3(sphereColor.r, sphereColor.g, sphereColor.b);
    }

    // Walidacja parametrow
    if (m_radius <= 0.0f) {
        m_radius = 1.0f;
        Logger::getInstance().warning("Sphere: Promien <= 0, ustawiono na 1.0.");
    }
    if (longitudeSegments < 3) {
        longitudeSegments = 3; // Minimum 3 segmenty dla sensownego okregu
        Logger::getInstance().warning("Sphere: longitudeSegments < 3, ustawiono na 3.");
    }
    if (latitudeSegments < 2) {
        latitudeSegments = 2; // Minimum 2 segmenty dla sensownej sfery (gorna i dolna polkula)
        Logger::getInstance().warning("Sphere: latitudeSegments < 2, ustawiono na 2.");
    }

    m_vertices.clear();
    m_indices.clear();

    for (int lat = 0; lat <= latitudeSegments; ++lat) {
        float theta = static_cast<float>(lat) * glm::pi<float>() / static_cast<float>(latitudeSegments); // Kat theta od 0 do PI
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (int lon = 0; lon <= longitudeSegments; ++lon) {
            float phi = static_cast<float>(lon) * 2.0f * glm::pi<float>() / static_cast<float>(longitudeSegments); // Kat phi od 0 do 2*PI
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            // Pozycja wierzcholka
            glm::vec3 pos(m_radius * sinTheta * cosPhi,   // x
                m_radius * cosTheta,            // y
                m_radius * sinTheta * sinPhi);  // z

            glm::vec3 norm = glm::normalize(pos); // Dla sfery o srodku w (0,0,0), normalna to znormalizowana pozycja

            // Wspolrzedne tekstury (standardowe mapowanie sferyczne)
            glm::vec2 texCoords(static_cast<float>(lon) / static_cast<float>(longitudeSegments),   // u
                static_cast<float>(lat) / static_cast<float>(latitudeSegments));  // v

            m_vertices.emplace_back(pos, norm, texCoords, sphereColor);
        }
    }

    // Generowanie indeksow dla trojkatow (tworzenie pasow trojkatow)
    for (int lat = 0; lat < latitudeSegments; ++lat) {
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            // Indeksy wierzcholkow tworzacych prostokat (quad) na siatce sfery
            int first = (lat * (longitudeSegments + 1)) + lon;
            int second = first + longitudeSegments + 1;

            // Trojkat 1
            m_indices.push_back(first);
            m_indices.push_back(second);
            m_indices.push_back(first + 1);

            // Trojkat 2
            m_indices.push_back(second);
            m_indices.push_back(second + 1);
            m_indices.push_back(first + 1);
        }
    }
    recomposeModelMatrix();
    setupMesh();
    updateWorldBoundingVolume();
    m_isBoundingVolumeDirty = false;
}

void Sphere::updateWorldBoundingVolume() {
    // TODO: Zaimplementowac SphereBV i uzyc go tutaj.
    // Obecnie, jako placeholder, uzywamy AABB generowanego na podstawie wierzcholkow.
    if (m_boundingVolume && m_boundingVolume->getType() == BoundingShapeType::AABB) {
        if (!m_vertices.empty()) { // Upewnijmy sie, ze sa wierzcholki do przetworzenia
            static_cast<AABB*>(m_boundingVolume.get())->updateFromVertices(m_vertices, getModelMatrix());
        }
        else {
            // Jesli brak wierzcholkow, AABB powinno byc "puste" lub zresetowane
            static_cast<AABB*>(m_boundingVolume.get())->reset(); // Zakladajac, ze AABB ma metode reset
        }
    }
    else if (!m_boundingVolume) {
        // Ten przypadek nie powinien sie zdarzyc, jesli konstruktor poprawnie tworzy BV.
        Logger::getInstance().warning("Sphere::updateWorldBoundingVolume: BoundingVolume jest nullptr. Tworzenie domyslnego AABB.");
        m_boundingVolume = std::make_unique<AABB>();
        if (!m_vertices.empty()) {
            static_cast<AABB*>(m_boundingVolume.get())->updateFromVertices(m_vertices, getModelMatrix());
        }
    }
    else {
        // Typ BV jest nieoczekiwany
        Logger::getInstance().error("Sphere::updateWorldBoundingVolume: BoundingVolume nie jest typu AABB.");
    }
}

//=================================================================================================
// Implementacja Cylinder
//=================================================================================================

Cylinder::Cylinder(const glm::vec3& centerPosition, float radius, float height, int segments, const glm::vec4& cylinderColor)
    : BasePrimitive(), m_height(height), m_segments(segments) {
    m_position = centerPosition; // Srodek cylindra (geometryczny srodek jego wysokosci)

    // Inicjalizacja lokalnych wymiarow dla BoundingVolume typu CylinderBV
    m_localRadius = radius;
    m_localBaseCenter1 = glm::vec3(0.0f, -height / 2.0f, 0.0f); // Srodek dolnej podstawy
    m_localBaseCenter2 = glm::vec3(0.0f, height / 2.0f, 0.0f); // Srodek gornej podstawy
    m_boundingVolume = std::make_unique<CylinderBV>();

    if (cylinderColor != PRGR_DEFAULT_CONSTRUCTOR_COLOR) {
        m_material.diffuse = glm::vec3(cylinderColor.r, cylinderColor.g, cylinderColor.b);
    }

    // Walidacja parametrow
    if (m_localRadius <= 0.0f) {
        m_localRadius = 1.0f;
        Logger::getInstance().warning("Cylinder: Promien <= 0, ustawiono na 1.0.");
    }
    if (m_height <= 0.0f) {
        m_height = 1.0f; // Uzywamy juz m_height, wiec to jest redundantne, ale zostawiam jako dodatkowa asekuracja
        Logger::getInstance().warning("Cylinder: Wysokosc <= 0, ustawiono na 1.0.");
    }
    if (m_segments < 3) {
        m_segments = 3;
        Logger::getInstance().warning("Cylinder: Liczba segmentow < 3, ustawiono na 3.");
    }

    float currentHalfHeight = m_height / 2.0f; // Poprawne uzycie m_height

    m_vertices.clear();
    m_indices.clear();

    // --- Dolna podstawa ---
    // Srodkowy wierzcholek dolnej podstawy
    m_vertices.emplace_back(glm::vec3(0.0f, -currentHalfHeight, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.5f, 0.5f), cylinderColor);
    unsigned int bottomCenterIndex = 0; // Indeks srodkowego wierzcholka dolnej podstawy

    // Wierzcholki na obwodzie dolnej podstawy
    for (int i = 0; i <= m_segments; ++i) {
        float angle = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(m_segments);
        float x = m_localRadius * std::cos(angle);
        float z = m_localRadius * std::sin(angle);
        // Wspolrzedne tekstury dla podstawy - mapowanie radialne
        float u = (std::cos(angle) + 1.0f) * 0.5f; // (x / m_localRadius + 1.0f) * 0.5f;
        float v = (std::sin(angle) + 1.0f) * 0.5f; // (z / m_localRadius + 1.0f) * 0.5f;
        m_vertices.emplace_back(glm::vec3(x, -currentHalfHeight, z), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(u, v), cylinderColor);
    }
    // Indeksy dla dolnej podstawy (trojkaty wachlarzowe)
    for (int i = 0; i < m_segments; ++i) {
        m_indices.push_back(bottomCenterIndex);
        m_indices.push_back(bottomCenterIndex + 1 + i);
        m_indices.push_back(bottomCenterIndex + 1 + (i + 1));
    }

    // --- Gorna podstawa ---
    unsigned int topCenterIndexOffset = m_vertices.size();
    // Srodkowy wierzcholek gornej podstawy
    m_vertices.emplace_back(glm::vec3(0.0f, currentHalfHeight, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.5f, 0.5f), cylinderColor);
    unsigned int topCenterIndex = topCenterIndexOffset;

    // Wierzcholki na obwodzie gornej podstawy
    for (int i = 0; i <= m_segments; ++i) {
        float angle = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(m_segments);
        float x = m_localRadius * std::cos(angle);
        float z = m_localRadius * std::sin(angle);
        float u = (std::cos(angle) + 1.0f) * 0.5f;
        float v = (std::sin(angle) + 1.0f) * 0.5f;
        m_vertices.emplace_back(glm::vec3(x, currentHalfHeight, z), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(u, v), cylinderColor);
    }
    // Indeksy dla gornej podstawy (trojkaty wachlarzowe, odwrocona kolejnosc dla normalnej skierowanej w gore)
    for (int i = 0; i < m_segments; ++i) {
        m_indices.push_back(topCenterIndex);
        m_indices.push_back(topCenterIndex + 1 + (i + 1)); // Odwrocona kolejnosc
        m_indices.push_back(topCenterIndex + 1 + i);
    }

    // --- Powierzchnia boczna ---
    unsigned int sideIndexOffset = m_vertices.size();
    for (int i = 0; i <= m_segments; ++i) {
        float angle = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(m_segments);
        float x = m_localRadius * std::cos(angle);
        float z = m_localRadius * std::sin(angle);
        glm::vec3 normal = glm::normalize(glm::vec3(x, 0.0f, z)); // Normalna prostopadla do osi Y, skierowana na zewnatrz

        // Wspolrzedne tekstury dla powierzchni bocznej - rozwijane na plaszczyzne
        float uCoord = static_cast<float>(i) / static_cast<float>(m_segments);

        // Dolny wierzcholek boku
        m_vertices.emplace_back(glm::vec3(x, -currentHalfHeight, z), normal, glm::vec2(uCoord, 0.0f), cylinderColor);
        // Gorny wierzcholek boku
        m_vertices.emplace_back(glm::vec3(x, currentHalfHeight, z), normal, glm::vec2(uCoord, 1.0f), cylinderColor);
    }
    // Indeksy dla powierzchni bocznej (tworzenie pasow trojkatow)
    for (int i = 0; i < m_segments; ++i) {
        unsigned int bl = sideIndexOffset + i * 2;     // bottom-left
        unsigned int tl = sideIndexOffset + i * 2 + 1; // top-left
        unsigned int br = sideIndexOffset + (i + 1) * 2;     // bottom-right (nastepny segment)
        unsigned int tr = sideIndexOffset + (i + 1) * 2 + 1; // top-right  (nastepny segment)

        // Trojkat 1
        m_indices.push_back(bl);
        m_indices.push_back(tl);
        m_indices.push_back(tr);
        // Trojkat 2
        m_indices.push_back(bl);
        m_indices.push_back(tr);
        m_indices.push_back(br);
    }

    recomposeModelMatrix();
    setupMesh();
    updateWorldBoundingVolume();
    m_isBoundingVolumeDirty = false;
}

void Cylinder::updateWorldBoundingVolume() {
    if (m_boundingVolume && m_boundingVolume->getType() == BoundingShapeType::CYLINDER) {
        CylinderBV* cylBv = static_cast<CylinderBV*>(m_boundingVolume.get());
        // Aktualizacja CylinderBV na podstawie lokalnych parametrow i macierzy modelu
        cylBv->updateWorldVolume(m_localBaseCenter1, m_localBaseCenter2, m_localRadius, getModelMatrix());
    }
    else {
        Logger::getInstance().error("Cylinder::updateWorldBoundingVolume: BoundingVolume nie jest typu CYLINDER lub jest nullptr.");
        if (!m_boundingVolume) {
            Logger::getInstance().warning("Cylinder::updateWorldBoundingVolume: Proba stworzenia domyslnego CylinderBV.");
            m_boundingVolume = std::make_unique<CylinderBV>();
            // static_cast<CylinderBV*>(m_boundingVolume.get())->updateWorldVolume(m_localBaseCenter1, m_localBaseCenter2, m_localRadius, getModelMatrix());
        }
    }
}

//=================================================================================================
// Implementacja SquarePyramid
//=================================================================================================

SquarePyramid::SquarePyramid(const glm::vec3& centerPosition, float baseLength, float height, const glm::vec4& pyramidColor)
    : BasePrimitive(), m_baseLength(baseLength), m_pyramidHeight(height) {
    m_position = centerPosition; // Pozycja srodka podstawy piramidy

    // TODO: Rozwazyc stworzenie dedykowanego PyramidBV lub uzycie OBB dla dokladniejszego BV.
    // Na razie AABB jako placeholder.
    m_boundingVolume = std::make_unique<AABB>();

    if (pyramidColor != PRGR_DEFAULT_CONSTRUCTOR_COLOR) {
        m_material.diffuse = glm::vec3(pyramidColor.r, pyramidColor.g, pyramidColor.b);
    }

    // Walidacja
    if (m_baseLength <= 0.0f) {
        m_baseLength = 1.0f;
        Logger::getInstance().warning("SquarePyramid: Dlugosc podstawy <= 0, ustawiono na 1.0.");
    }
    if (m_pyramidHeight <= 0.0f) {
        m_pyramidHeight = 1.0f;
        Logger::getInstance().warning("SquarePyramid: Wysokosc <= 0, ustawiono na 1.0.");
    }

    float halfBase = m_baseLength / 2.0f;

    // Wierzcholki podstawy (Y=0 w ukladzie lokalnym, srodek podstawy w (0,0,0) lokalnie)
    // Wierzcholek piramidy (apex) na osi Y
    glm::vec3 b0(-halfBase, 0.0f, halfBase); // Lewy-przedni
    glm::vec3 b1(halfBase, 0.0f, halfBase); // Prawy-przedni
    glm::vec3 b2(halfBase, 0.0f, -halfBase); // Prawy-tylny
    glm::vec3 b3(-halfBase, 0.0f, -halfBase); // Lewy-tylny
    glm::vec3 apex(0.0f, m_pyramidHeight, 0.0f); // Wierzcholek

    m_vertices.clear();
    m_indices.clear();
    unsigned int currentIndex = 0;

    // --- Podstawa piramidy (skierowana w dol, -Y) ---
    glm::vec3 baseNormal(0.0f, -1.0f, 0.0f);
    m_vertices.emplace_back(b0, baseNormal, glm::vec2(0.0f, 0.0f), pyramidColor); // 0
    m_vertices.emplace_back(b1, baseNormal, glm::vec2(1.0f, 0.0f), pyramidColor); // 1
    m_vertices.emplace_back(b2, baseNormal, glm::vec2(1.0f, 1.0f), pyramidColor); // 2
    m_vertices.emplace_back(b3, baseNormal, glm::vec2(0.0f, 1.0f), pyramidColor); // 3

    m_indices.push_back(currentIndex + 0); m_indices.push_back(currentIndex + 2); m_indices.push_back(currentIndex + 1); // b0, b2, b1 (CCW dla -Y)
    m_indices.push_back(currentIndex + 0); m_indices.push_back(currentIndex + 3); m_indices.push_back(currentIndex + 2); // b0, b3, b2 (CCW dla -Y)
    currentIndex += 4;

    // --- Sciany boczne ---
    // Sciana przednia (b0, b1, apex)
    glm::vec3 nFront = glm::normalize(glm::cross(b1 - b0, apex - b0));
    m_vertices.emplace_back(b0, nFront, glm::vec2(0.0f, 0.0f), pyramidColor); // 4
    m_vertices.emplace_back(b1, nFront, glm::vec2(1.0f, 0.0f), pyramidColor); // 5
    m_vertices.emplace_back(apex, nFront, glm::vec2(0.5f, 1.0f), pyramidColor); // 6
    m_indices.push_back(currentIndex + 0); m_indices.push_back(currentIndex + 1); m_indices.push_back(currentIndex + 2);
    currentIndex += 3;

    // Sciana prawa (b1, b2, apex)
    glm::vec3 nRight = glm::normalize(glm::cross(b2 - b1, apex - b1));
    m_vertices.emplace_back(b1, nRight, glm::vec2(0.0f, 0.0f), pyramidColor); // 7
    m_vertices.emplace_back(b2, nRight, glm::vec2(1.0f, 0.0f), pyramidColor); // 8
    m_vertices.emplace_back(apex, nRight, glm::vec2(0.5f, 1.0f), pyramidColor); // 9
    m_indices.push_back(currentIndex + 0); m_indices.push_back(currentIndex + 1); m_indices.push_back(currentIndex + 2);
    currentIndex += 3;

    // Sciana tylna (b2, b3, apex)
    glm::vec3 nBack = glm::normalize(glm::cross(b3 - b2, apex - b2));
    m_vertices.emplace_back(b2, nBack, glm::vec2(0.0f, 0.0f), pyramidColor); // 10
    m_vertices.emplace_back(b3, nBack, glm::vec2(1.0f, 0.0f), pyramidColor); // 11
    m_vertices.emplace_back(apex, nBack, glm::vec2(0.5f, 1.0f), pyramidColor); // 12
    m_indices.push_back(currentIndex + 0); m_indices.push_back(currentIndex + 1); m_indices.push_back(currentIndex + 2);
    currentIndex += 3;

    // Sciana lewa (b3, b0, apex)
    glm::vec3 nLeft = glm::normalize(glm::cross(b0 - b3, apex - b3));
    m_vertices.emplace_back(b3, nLeft, glm::vec2(0.0f, 0.0f), pyramidColor); // 13
    m_vertices.emplace_back(b0, nLeft, glm::vec2(1.0f, 0.0f), pyramidColor); // 14
    m_vertices.emplace_back(apex, nLeft, glm::vec2(0.5f, 1.0f), pyramidColor); // 15
    m_indices.push_back(currentIndex + 0); m_indices.push_back(currentIndex + 1); m_indices.push_back(currentIndex + 2);
    // currentIndex += 3; // Niepotrzebne juz

    recomposeModelMatrix();
    setupMesh();
    updateWorldBoundingVolume();
    m_isBoundingVolumeDirty = false;
}

void SquarePyramid::updateWorldBoundingVolume() {
    // TODO: Rozwazyc PyramidBV lub OBB dla dokladniejszego BV.
    if (m_boundingVolume && m_boundingVolume->getType() == BoundingShapeType::AABB) {
        if (!m_vertices.empty()) {
            static_cast<AABB*>(m_boundingVolume.get())->updateFromVertices(m_vertices, getModelMatrix());
        }
        else {
            static_cast<AABB*>(m_boundingVolume.get())->reset();
        }
    }
    else if (!m_boundingVolume) {
        Logger::getInstance().warning("SquarePyramid::updateWorldBoundingVolume: BoundingVolume jest nullptr. Tworzenie domyslnego AABB.");
        m_boundingVolume = std::make_unique<AABB>();
        if (!m_vertices.empty()) {
            static_cast<AABB*>(m_boundingVolume.get())->updateFromVertices(m_vertices, getModelMatrix());
        }
    }
    else {
        Logger::getInstance().error("SquarePyramid::updateWorldBoundingVolume: BoundingVolume nie jest typu AABB.");
    }
}

//=================================================================================================
// Implementacja Cone
//=================================================================================================

Cone::Cone(const glm::vec3& centerPosition, float radius, float height, int segments, const glm::vec4& coneColor)
    : BasePrimitive(), m_radius(radius), m_coneHeight(height), m_segments(segments) {
    m_position = centerPosition; // Pozycja srodka podstawy stozka

    // TODO: Zaimplementowac ConeBV dla dokladniejszych kolizji.
    m_boundingVolume = std::make_unique<AABB>();

    if (coneColor != PRGR_DEFAULT_CONSTRUCTOR_COLOR) {
        m_material.diffuse = glm::vec3(coneColor.r, coneColor.g, coneColor.b);
    }

    // Walidacja parametrow
    if (m_radius <= 0.0f) {
        m_radius = 1.0f;
        Logger::getInstance().warning("Cone: Promien <= 0, ustawiono na 1.0.");
    }
    if (m_coneHeight <= 0.0f) {
        m_coneHeight = 1.0f;
        Logger::getInstance().warning("Cone: Wysokosc <= 0, ustawiono na 1.0.");
    }
    if (m_segments < 3) {
        m_segments = 3;
        Logger::getInstance().warning("Cone: Liczba segmentow < 3, ustawiono na 3.");
    }

    glm::vec3 apexPos(0.0f, m_coneHeight, 0.0f);    // Wierzcholek stozka (Y-up)
    glm::vec3 baseCenterPos(0.0f, 0.0f, 0.0f); // Srodek podstawy stozka w (0,0,0) lokalnie

    m_vertices.clear();
    m_indices.clear();
    unsigned int currentIndex = 0;

    // --- Podstawa stozka (okragla, skierowana w dol -Y) ---
    // Srodkowy wierzcholek podstawy
    m_vertices.emplace_back(baseCenterPos, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.5f, 0.5f), coneColor);
    unsigned int baseCenterIndex = currentIndex++;

    // Wierzcholki na obwodzie podstawy
    for (int i = 0; i <= m_segments; ++i) {
        float angle = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(m_segments);
        float x = m_radius * std::cos(angle);
        float z = m_radius * std::sin(angle);
        // Wspolrzedne tekstury dla podstawy - mapowanie radialne
        float u = (std::cos(angle) + 1.0f) * 0.5f;
        float v = (std::sin(angle) + 1.0f) * 0.5f;
        m_vertices.emplace_back(glm::vec3(x, 0.0f, z), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(u, v), coneColor);
        currentIndex++;
    }
    // Indeksy dla podstawy (trojkaty wachlarzowe, CCW dla -Y)
    for (int i = 0; i < m_segments; ++i) {
        m_indices.push_back(baseCenterIndex);
        m_indices.push_back(baseCenterIndex + 1 + (i + 1)); // Odwrocona kolejnosc dla normalnej -Y
        m_indices.push_back(baseCenterIndex + 1 + i);
    }

    // --- Powierzchnia boczna stozka ---
    // Dla kazdego segmentu tworzymy jeden trojkat (wierzcholek stozka + dwa wierzcholki na podstawie)
    // Potrzebujemy nowych wierzcholkow dla powierzchni bocznej, poniewaz normalne i texCoordy sa inne.
    unsigned int sideVertexStartIndex = currentIndex; // m_vertices.size();

    for (int i = 0; i < m_segments; ++i) { // Iterujemy do m_segments, a nie m_segments + 1
        float angle0 = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(m_segments);
        float angle1 = 2.0f * glm::pi<float>() * static_cast<float>(i + 1) / static_cast<float>(m_segments);

        glm::vec3 p0Base(m_radius * std::cos(angle0), 0.0f, m_radius * std::sin(angle0));
        glm::vec3 p1Base(m_radius * std::cos(angle1), 0.0f, m_radius * std::sin(angle1));

        // Obliczanie normalnych dla powierzchni bocznej stozka.
        // Wektor T = p1_base - p0_base (styczna do podstawy)
        // Wektor S = apex_pos - p0_base (tworzaca stozka)
        // Normalna N = normalize(cross(S, T)) - trzeba uwazac na kolejnosc dla orientacji na zewnatrz
        // Alternatywnie, normalna w punkcie (x,0,z) na krawedzi podstawy dla stozka o wysokosci H i promieniu R:
        // Komponenty XZ normalnej sa proporcjonalne do XZ pozycji, a komponent Y jest staly (R).
        // N = normalize(vec3(H*x/R, R, H*z/R))
        // Jesli R jest w mianowniku, to (H*cos(angle), R, H*sin(angle))
        glm::vec3 n0Side = glm::normalize(glm::vec3(m_coneHeight * std::cos(angle0), m_radius, m_coneHeight * std::sin(angle0)));
        glm::vec3 n1Side = glm::normalize(glm::vec3(m_coneHeight * std::cos(angle1), m_radius, m_coneHeight * std::sin(angle1)));
        // Normalna dla wierzcholka stozka moze byc usredniona lub po prostu (0,1,0) jesli stozek jest idealnie ostry
        // Dla gladkiego cieniowania, uzyjemy normalnych odpowiednich dla tworzacych.
        // Przy generowaniu wierzcholkow w petli, wierzcholek stozka (apex) bedzie mial normalna zalezna od segmentu.

        // Wspolrzedne tekstury dla powierzchni bocznej
        float u0 = static_cast<float>(i) / static_cast<float>(m_segments);
        float u1 = static_cast<float>(i + 1) / static_cast<float>(m_segments);
        float uApex = (u0 + u1) / 2.0f; // Srednia dla wierzcholka stozka w tym trojkacie

        m_vertices.emplace_back(p0Base, n0Side, glm::vec2(u0, 0.0f), coneColor); // idx: sideVertexStartIndex + i*3 + 0
        m_vertices.emplace_back(p1Base, n1Side, glm::vec2(u1, 0.0f), coneColor); // idx: sideVertexStartIndex + i*3 + 1
        // Dla wierzcholka stozka, normalna powinna byc usredniona z normalnych tworzacych, 
        // ktore sie w nim spotykaja, lub uzyjemy normalnej tworzacej.
        // Tutaj dla uproszczenia, uzyjemy normalnej interpolowanej miedzy n0 a n1 (chociaz to nie jest idealne dla samego wierzcholka).
        // Lepszym podejsciem byloby stworzenie jednego wierzcholka 'apex' z usredniona normalna i reuzywanie go.
        // Na razie, dla kazdego trojkata tworzymy nowy wierzcholek 'apex' z normalna (n0Side+n1Side)/2
        glm::vec3 apexNormalForThisTriangle = glm::normalize(n0Side + n1Side); // Prosta srednia
        m_vertices.emplace_back(apexPos, apexNormalForThisTriangle, glm::vec2(uApex, 1.0f), coneColor); // idx: sideVertexStartIndex + i*3 + 2

        m_indices.push_back(sideVertexStartIndex + i * 3 + 0); // p0Base
        m_indices.push_back(sideVertexStartIndex + i * 3 + 1); // p1Base
        m_indices.push_back(sideVertexStartIndex + i * 3 + 2); // apex
    }
    // currentIndex = m_vertices.size(); // Aktualizacja currentIndex, jesli bylby uzywany dalej

    recomposeModelMatrix();
    setupMesh();
    updateWorldBoundingVolume();
    m_isBoundingVolumeDirty = false;
}

void Cone::updateWorldBoundingVolume() {
    // TODO: Zaimplementowac ConeBV i uzyc go tutaj.
    if (m_boundingVolume && m_boundingVolume->getType() == BoundingShapeType::AABB) {
        if (!m_vertices.empty()) {
            static_cast<AABB*>(m_boundingVolume.get())->updateFromVertices(m_vertices, getModelMatrix());
        }
        else {
            static_cast<AABB*>(m_boundingVolume.get())->reset();
        }
    }
    else if (!m_boundingVolume) {
        Logger::getInstance().warning("Cone::updateWorldBoundingVolume: BoundingVolume jest nullptr. Tworzenie domyslnego AABB.");
        m_boundingVolume = std::make_unique<AABB>();
        if (!m_vertices.empty()) {
            static_cast<AABB*>(m_boundingVolume.get())->updateFromVertices(m_vertices, getModelMatrix());
        }
    }
    else {
        Logger::getInstance().error("Cone::updateWorldBoundingVolume: BoundingVolume nie jest typu AABB.");
    }
}