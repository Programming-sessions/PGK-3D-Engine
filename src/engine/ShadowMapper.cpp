#include "ShadowMapper.h"
#include "Shader.h"
#include "ResourceManager.h"
#include "Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <utility> // Dla std::move

// Definicje stalych statycznych
const float ShadowMapper::UP_VECTOR_THRESHOLD = 0.99f;
const float ShadowMapper::SHADOW_BORDER_COLOR[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

ShadowMapper::ShadowMapper(unsigned int shadowWidth, unsigned int shadowHeight, ShadowMapType type)
    : m_depthMapFBO(0),
    m_depthMapTextureID(0),
    m_shadowWidth(shadowWidth),
    m_shadowHeight(shadowHeight),
    m_depthShader(nullptr),
    m_shadowMapType(type) {
    if (m_shadowWidth == 0 || m_shadowHeight == 0) {
        Logger::getInstance().warning("ShadowMapper: Szerokosc lub wysokosc mapy cieni wynosi 0. Ustawiam na 1.");
        if (m_shadowWidth == 0) m_shadowWidth = 1;
        if (m_shadowHeight == 0) m_shadowHeight = 1;
    }
}

ShadowMapper::~ShadowMapper() {
    cleanup();
}

ShadowMapper::ShadowMapper(ShadowMapper&& other) noexcept
    : m_depthMapFBO(other.m_depthMapFBO),
    m_depthMapTextureID(other.m_depthMapTextureID),
    m_shadowWidth(other.m_shadowWidth),
    m_shadowHeight(other.m_shadowHeight),
    m_lightSpaceMatrices(std::move(other.m_lightSpaceMatrices)),
    m_depthShader(std::move(other.m_depthShader)),
    m_shadowMapType(other.m_shadowMapType) {
    // Wazne: Zerowanie zasobow w przeniesionym obiekcie, aby jego destruktor
    // nie probowal zwolnic zasobow, ktore teraz naleza do tego obiektu.
    other.m_depthMapFBO = 0;
    other.m_depthMapTextureID = 0;
    other.m_shadowWidth = 0;
    other.m_shadowHeight = 0;
    // m_lightSpaceMatrices i m_depthShader sa juz poprawnie przeniesione (other jest w stanie "valid but unspecified")
}

ShadowMapper& ShadowMapper::operator=(ShadowMapper&& other) noexcept {
    if (this != &other) {
        // Najpierw zwolnij wlasne zasoby
        cleanup();

        // Przenies dane z 'other'
        m_depthMapFBO = other.m_depthMapFBO;
        m_depthMapTextureID = other.m_depthMapTextureID;
        m_shadowWidth = other.m_shadowWidth;
        m_shadowHeight = other.m_shadowHeight;
        m_lightSpaceMatrices = std::move(other.m_lightSpaceMatrices);
        m_depthShader = std::move(other.m_depthShader);
        m_shadowMapType = other.m_shadowMapType;

        // Zeruj zasoby w 'other'
        other.m_depthMapFBO = 0;
        other.m_depthMapTextureID = 0;
        other.m_shadowWidth = 0;
        other.m_shadowHeight = 0;
    }
    return *this;
}

void ShadowMapper::cleanup() {
    if (m_depthMapFBO != 0) {
        glDeleteFramebuffers(1, &m_depthMapFBO);
        m_depthMapFBO = 0;
    }
    if (m_depthMapTextureID != 0) {
        glDeleteTextures(1, &m_depthMapTextureID);
        m_depthMapTextureID = 0;
    }
    m_lightSpaceMatrices.clear();
    // m_depthShader (shared_ptr) zarzadza soba sam.
}

bool ShadowMapper::initialize(const std::string& shaderName,
    const std::string& depthVertexPath,
    const std::string& depthFragmentPath,
    ResourceManager* resourceManager) {
    // Proba uzyskania/zaladowania shadera glebi, jesli nie zostal wczesniej ustawiony
    if (!m_depthShader) {
        if (resourceManager) {
            if (!shaderName.empty()) {
                m_depthShader = resourceManager->getShader(shaderName);
                if (!m_depthShader && !depthVertexPath.empty() && !depthFragmentPath.empty()) {
                    m_depthShader = resourceManager->loadShader(shaderName, depthVertexPath, depthFragmentPath);
                }
            }
            else if (!depthVertexPath.empty() && !depthFragmentPath.empty()) {
                // Generowanie prostej nazwy dla shadera na podstawie sciezki, jesli nazwa nie jest podana
                std::string generatedName = "depth_shader_from_" + depthVertexPath;
                m_depthShader = resourceManager->loadShader(generatedName, depthVertexPath, depthFragmentPath);
            }
        }
        else if (!shaderName.empty() && !depthVertexPath.empty() && !depthFragmentPath.empty()) {
            // Ostrzezenie, jesli ResourceManager jest null, a probujemy ladowac shader
            Logger::getInstance().warning("ShadowMapper::initialize - ResourceManager jest null. Nie mozna zaladowac shadera '" + shaderName + "' jesli nie jest juz w cache.");
        }
    }

    // Mimo ze shader nie jest bezposrednio uzywany do tworzenia FBO,
    // jego brak jest problemem dla calosci dzialania mappera.
    // W ShadowSystem shader jest ustawiany przed initialize, wiec ten blok moze byc rzadko aktywny.
    if (!m_depthShader) {
        Logger::getInstance().warning("ShadowMapper::initialize - Shader glebi nie jest ustawiony ani zaladowany. Mapper moze nie dzialac poprawnie.");
    }

    glGenFramebuffers(1, &m_depthMapFBO);
    glGenTextures(1, &m_depthMapTextureID);

    if (m_shadowMapType == ShadowMapType::SHADOW_MAP_2D) {
        glBindTexture(GL_TEXTURE_2D, m_depthMapTextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_shadowWidth, m_shadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Prostszy (szybszy) filtr dla map glebi
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); // Zapobiega artefaktom na krawedziach mapy cieni
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, SHADOW_BORDER_COLOR);

        glBindFramebuffer(GL_FRAMEBUFFER, m_depthMapFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthMapTextureID, 0);
    }
    else { // ShadowMapType::SHADOW_MAP_CUBEMAP
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_depthMapTextureID);
        for (unsigned int i = 0; i < 6; ++i) {
            // Inicjalizacja kazdej z szesciu scian cubemapy
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                m_shadowWidth, m_shadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, m_depthMapFBO);
        // Dolaczenie jednej ze scian cubemapy jako attachment glebi FBO
        // (pozostale beda dolaczane dynamicznie podczas renderowania do kazdej sciany)
        // Tutaj dolaczamy pierwsza sciane, aby FBO bylo kompletne.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, m_depthMapTextureID, 0);
    }

    // Informujemy OpenGL, ze nie bedziemy renderowac do zadnego bufora koloru
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
        Logger::getInstance().error("ShadowMapper: Framebuffer nie jest kompletny! Status: " + std::to_string(fboStatus));
        glBindFramebuffer(GL_FRAMEBUFFER, 0); // Przywroc domyslny FBO przed cleanup
        cleanup();                            // Zwolnij utworzone zasoby
        return false;
    }

    // Odpiecie FBO i tekstury po konfiguracji
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture((m_shadowMapType == ShadowMapType::SHADOW_MAP_2D) ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP, 0);

    // Logger::getInstance().info("ShadowMapper FBO i tekstura glebi zainicjalizowane pomyslnie.");
    return true;
}

void ShadowMapper::setDepthShader(std::shared_ptr<Shader> shader) {
    m_depthShader = shader;
}

void ShadowMapper::updateLightSpaceMatrixForDirectionalLight(
    const glm::vec3& lightDirection,
    const glm::vec3& sceneFocusPoint,
    float orthoBoxWidth,
    float orthoBoxHeight,
    float nearPlane,
    float farPlane,
    float lightPositionOffset) {
    if (m_shadowMapType != ShadowMapType::SHADOW_MAP_2D) {
        // Opcjonalne ostrzezenie, jesli typ mapy nie pasuje
        // Logger::getInstance().warning("ShadowMapper: Proba aktualizacji macierzy dla swiatla kierunkowego na mapperze typu CUBEMAP.");
    }
    m_lightSpaceMatrices.assign(1, glm::mat4(1.0f)); // Zapewnienie, ze jest jedna macierz

    glm::mat4 lightProjection = glm::ortho(
        -orthoBoxWidth / 2.0f, orthoBoxWidth / 2.0f,
        -orthoBoxHeight / 2.0f, orthoBoxHeight / 2.0f,
        nearPlane, farPlane
    );

    // Obliczenie pozycji swiatla na podstawie punktu skupienia i kierunku
    glm::vec3 lightPos = sceneFocusPoint - glm::normalize(lightDirection) * lightPositionOffset;

    // Wybor wektora "up" tak, aby uniknac problemow, gdy kierunek swiatla jest rownolegly do domyslnego "up"
    glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);
    if (abs(glm::dot(glm::normalize(lightDirection), upVector)) > UP_VECTOR_THRESHOLD) {
        upVector = glm::vec3(1.0f, 0.0f, 0.0f); // Alternatywny wektor "up"
    }

    glm::mat4 lightView = glm::lookAt(lightPos, sceneFocusPoint, upVector);
    m_lightSpaceMatrices[0] = lightProjection * lightView;
}

void ShadowMapper::updateLightSpaceMatrixForSpotlight(
    const glm::vec3& lightPosition,
    const glm::vec3& lightDirection,
    float fovYDegrees,
    float aspectRatio,
    float nearPlane,
    float farPlane) {
    if (m_shadowMapType != ShadowMapType::SHADOW_MAP_2D) {
        // Opcjonalne ostrzezenie
    }
    m_lightSpaceMatrices.assign(1, glm::mat4(1.0f));

    glm::mat4 lightProjection = glm::perspective(glm::radians(fovYDegrees), aspectRatio, nearPlane, farPlane);

    glm::vec3 normalizedLightDir = glm::normalize(lightDirection);
    glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);

    // Korekta wektora "up" dla przypadkow skrajnych (swiatlo skierowane prosto w gore lub w dol)
    if (abs(glm::dot(normalizedLightDir, glm::vec3(0.0f, 1.0f, 0.0f))) > UP_VECTOR_THRESHOLD) { // Swiatlo patrzy w gore
        upVector = glm::vec3(0.0f, 0.0f, 1.0f); // Patrzymy "do przodu" wzgledem osi Z kamery
    }
    else if (abs(glm::dot(normalizedLightDir, glm::vec3(0.0f, -1.0f, 0.0f))) > UP_VECTOR_THRESHOLD) { // Swiatlo patrzy w dol
        upVector = glm::vec3(0.0f, 0.0f, -1.0f); // Patrzymy "do tylu" wzgledem osi Z kamery
    }
    // Uwaga: oryginalny kod mial -1.0f i 1.0f dla Z. Dla standardowego lookAt, przy patrzeniu w dół (0,-1,0),
    // "up" (0,0,-1) oznacza, że góra kamery jest skierowana w stronę ujemnego Z.
    // Przy patrzeniu w górę (0,1,0), "up" (0,0,1) oznacza, że góra kamery jest w stronę dodatniego Z.
    // To zachowuje spójność.

    glm::mat4 lightView = glm::lookAt(lightPosition, lightPosition + normalizedLightDir, upVector);
    m_lightSpaceMatrices[0] = lightProjection * lightView;
}

void ShadowMapper::updateLightSpaceMatricesForPointLight(
    const glm::vec3& lightPosition,
    float nearPlane,
    float farPlane) {
    if (m_shadowMapType != ShadowMapType::SHADOW_MAP_CUBEMAP) {
        // Opcjonalne ostrzezenie
    }
    m_lightSpaceMatrices.resize(6); // Potrzebujemy 6 macierzy dla cubemapy

    // Kat widzenia 90 stopni i wspolczynnik proporcji 1.0 dla kazdej sciany cubemapy
    glm::mat4 lightProjection = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);

    // Definicje macierzy widoku dla kazdej z 6 scian cubemapy
    // Kolejnosc i orientacja zgodna z konwencja OpenGL dla cubemap:
    // GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    // GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    // GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
    m_lightSpaceMatrices[0] = lightProjection * glm::lookAt(lightPosition, lightPosition + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)); // +X
    m_lightSpaceMatrices[1] = lightProjection * glm::lookAt(lightPosition, lightPosition + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)); // -X
    m_lightSpaceMatrices[2] = lightProjection * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)); // +Y
    m_lightSpaceMatrices[3] = lightProjection * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)); // -Y
    m_lightSpaceMatrices[4] = lightProjection * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)); // +Z
    m_lightSpaceMatrices[5] = lightProjection * glm::lookAt(lightPosition, lightPosition + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)); // -Z
}

void ShadowMapper::bindForWriting() {
    if (m_depthMapFBO == 0) {
        Logger::getInstance().error("ShadowMapper::bindForWriting - FBO nie jest zainicjalizowane.");
        return;
    }
    // Ustawienie viewportu na rozmiar mapy cieni
    glViewport(0, 0, m_shadowWidth, m_shadowHeight);
    // Aktywacja FBO do zapisu
    glBindFramebuffer(GL_FRAMEBUFFER, m_depthMapFBO);
}

void ShadowMapper::unbindAfterWriting(int originalViewportWidth, int originalViewportHeight) {
    // Przywrocenie domyslnego FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // Przywrocenie oryginalnego rozmiaru viewportu
    glViewport(0, 0, originalViewportWidth, originalViewportHeight);
}

const glm::mat4& ShadowMapper::getLightSpaceMatrix() const {
    if (m_lightSpaceMatrices.empty()) {
        // Zwracanie statycznej macierzy jednostkowej, aby uniknac bledow,
        // jesli macierze nie zostaly jeszcze obliczone.
        static const glm::mat4 identityMatrix(1.0f);
        Logger::getInstance().warning("ShadowMapper::getLightSpaceMatrix - Brak dostepnych macierzy przestrzeni swiatla. Zwracam macierz jednostkowa.");
        return identityMatrix;
    }
    return m_lightSpaceMatrices[0]; // Dla map 2D, interesuje nas pierwsza (i jedyna) macierz
}