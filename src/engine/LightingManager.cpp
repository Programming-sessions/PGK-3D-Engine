#include "LightingManager.h"
#include "Logger.h"   // Do logowania informacji i ostrzezen
#include "Shader.h"   // Dla std::shared_ptr<Shader> i metod setUniform
#include <string>     // Dla std::to_string, uzywane przy tworzeniu nazw uniformow
// Plik Lighting.h jest juz included przez LightingManager.h, wiec stale jak MAX_POINT_LIGHTS sa dostepne.

LightingManager::LightingManager() {
    // Inicjalizacja domyslnego swiatla kierunkowego moze byc tutaj,
    // lub pozostawiona do ustawienia przez uzytkownika.
    // Aktualnie m_directionalLight jest inicjalizowane domyslnym konstruktorem DirectionalLight.
    Logger::getInstance().info("LightingManager utworzony. Domyslne swiatlo kierunkowe zainicjalizowane.");
}

// --- Swiatlo Kierunkowe ---
void LightingManager::setDirectionalLight(const DirectionalLight& light) {
    m_directionalLight = light;
    // Logger::getInstance().info("Swiatlo kierunkowe zaktualizowane w LightingManager."); // Opcjonalny log
}

DirectionalLight& LightingManager::getDirectionalLight() {
    return m_directionalLight;
}

const DirectionalLight& LightingManager::getDirectionalLight() const {
    return m_directionalLight;
}

// --- Swiatla Punktowe ---
int LightingManager::addPointLight(const PointLight& light) {
    // Uzycie stalej MAX_POINT_LIGHTS zdefiniowanej w Lighting.h
    if (m_pointLights.size() >= MAX_POINT_LIGHTS) {
        Logger::getInstance().warning("LightingManager: Nie mozna dodac wiecej swiatel punktowych. Osiagnieto maksimum (" + std::to_string(MAX_POINT_LIGHTS) + ").");
        return -1; // Wskazuje na blad - nie udalo sie dodac swiatla
    }

    m_pointLights.push_back(light);
    // Logger::getInstance().info("Dodano swiatlo punktowe. Lacznie: " + std::to_string(m_pointLights.size())); // Opcjonalny log
    return static_cast<int>(m_pointLights.size() - 1); // Zwraca indeks dodanego swiatla
}

PointLight* LightingManager::getPointLight(size_t index) {
    if (index >= m_pointLights.size()) {
        // Logger::getInstance().warning("LightingManager::getPointLight - Indeks poza zakresem: " + std::to_string(index)); // Opcjonalny log
        return nullptr; // Bezpieczne zwrocenie nullptr, gdy indeks jest nieprawidlowy
    }
    return &m_pointLights[index];
}

const PointLight* LightingManager::getPointLight(size_t index) const {
    if (index >= m_pointLights.size()) {
        // Logger::getInstance().warning("LightingManager::getPointLight (const) - Indeks poza zakresem: " + std::to_string(index)); // Opcjonalny log
        return nullptr;
    }
    return &m_pointLights[index];
}

std::vector<PointLight>& LightingManager::getPointLights() {
    return m_pointLights;
}

const std::vector<PointLight>& LightingManager::getPointLights() const {
    return m_pointLights;
}

size_t LightingManager::getNumPointLights() const {
    return m_pointLights.size();
}

void LightingManager::clearPointLights() {
    m_pointLights.clear();
    Logger::getInstance().info("Wszystkie swiatla punktowe usuniete z LightingManager.");
}

// --- Reflektory ---
int LightingManager::addSpotLight(const SpotLight& light) {
    // Uzycie stalej MAX_SPOT_LIGHTS_TOTAL zdefiniowanej w Lighting.h
    if (m_spotLights.size() >= MAX_SPOT_LIGHTS_TOTAL) {
        Logger::getInstance().warning("LightingManager: Nie mozna dodac wiecej reflektorow. Osiagnieto maksimum (" + std::to_string(MAX_SPOT_LIGHTS_TOTAL) + ").");
        return -1;
    }

    m_spotLights.push_back(light);
    // Logger::getInstance().info("Dodano reflektor. Lacznie: " + std::to_string(m_spotLights.size())); // Opcjonalny log
    return static_cast<int>(m_spotLights.size() - 1);
}

SpotLight* LightingManager::getSpotLight(size_t index) {
    if (index >= m_spotLights.size()) {
        // Logger::getInstance().warning("LightingManager::getSpotLight - Indeks poza zakresem: " + std::to_string(index)); // Opcjonalny log
        return nullptr;
    }
    return &m_spotLights[index];
}

const SpotLight* LightingManager::getSpotLight(size_t index) const {
    if (index >= m_spotLights.size()) {
        // Logger::getInstance().warning("LightingManager::getSpotLight (const) - Indeks poza zakresem: " + std::to_string(index)); // Opcjonalny log
        return nullptr;
    }
    return &m_spotLights[index];
}

std::vector<SpotLight>& LightingManager::getSpotLights() {
    return m_spotLights;
}

const std::vector<SpotLight>& LightingManager::getSpotLights() const {
    return m_spotLights;
}

size_t LightingManager::getNumSpotLights() const {
    return m_spotLights.size();
}

void LightingManager::clearSpotLights() {
    m_spotLights.clear();
    Logger::getInstance().info("Wszystkie reflektory usuniete z LightingManager.");
}

// Metoda do wysylania ogolnych danych oswietlenia do shadera
void LightingManager::uploadGeneralLightUniforms(std::shared_ptr<Shader> shader) const {
    // Wczesne wyjscie jesli shader jest nieprawidlowy
    if (!shader || shader->getID() == 0) {
        Logger::getInstance().error("LightingManager::uploadGeneralLightUniforms - Shader jest nieprawidlowy (null lub ID=0).");
        return;
    }

    // Zakladamy, ze shader jest juz aktywny (shader->use() zostalo wywolane wczesniej)
    // Logger::getInstance().debug("LightingManager: Wysylanie uniformow swiatel do shadera ID: " + std::to_string(shader->getID()));


    // --- Swiatlo kierunkowe ---
    shader->setBool("dirLight.enabled", m_directionalLight.enabled);
    if (m_directionalLight.enabled) {
        shader->setVec3("dirLight.direction", m_directionalLight.direction);
        shader->setVec3("dirLight.ambient", m_directionalLight.ambient);
        shader->setVec3("dirLight.diffuse", m_directionalLight.diffuse);
        shader->setVec3("dirLight.specular", m_directionalLight.specular);
        // Dane cienia dla swiatla kierunkowego (np. dirLightSpaceMatrix, dirShadowMap)
        // sa zazwyczaj ustawiane przez dedykowany system cieni (ShadowSystem),
        // wiec nie sa tutaj obslugiwane.
    }

    // --- Swiatla punktowe ---
    // Uzywamy stalej MAX_POINT_LIGHTS z Lighting.h, ktora powinna odpowiadac limitowi w shaderze.
    int numActivePointLights = 0;
    for (size_t i = 0; i < m_pointLights.size(); ++i) {
        if (numActivePointLights >= MAX_POINT_LIGHTS) break; // Nie przekraczaj limitu shadera (zdefiniowanego w Lighting.h)

        if (m_pointLights[i].enabled) {
            std::string prefix = "pointLights[" + std::to_string(numActivePointLights) + "].";
            shader->setBool(prefix + "enabled", true); // Wazne: najpierw wlacz, potem ustawiaj reszte
            shader->setVec3(prefix + "position", m_pointLights[i].position);
            shader->setFloat(prefix + "constant", m_pointLights[i].constant);
            shader->setFloat(prefix + "linear", m_pointLights[i].linear);
            shader->setFloat(prefix + "quadratic", m_pointLights[i].quadratic);
            shader->setVec3(prefix + "ambient", m_pointLights[i].ambient);
            shader->setVec3(prefix + "diffuse", m_pointLights[i].diffuse);
            shader->setVec3(prefix + "specular", m_pointLights[i].specular);
            shader->setBool(prefix + "castsShadow", m_pointLights[i].castsShadow);
            shader->setInt(prefix + "shadowDataIndex", m_pointLights[i].shadowDataIndex);
            numActivePointLights++;
        }
    }
    shader->setInt("numPointLights", numActivePointLights);

    // Deaktywuj pozostale sloty swiatel punktowych w shaderze
    for (int i = numActivePointLights; i < MAX_POINT_LIGHTS; ++i) { // Uzyj MAX_POINT_LIGHTS z Lighting.h
        std::string prefix = "pointLights[" + std::to_string(i) + "].";
        shader->setBool(prefix + "enabled", false);
    }

    // --- Reflektory ---
    // Uzywamy stalej MAX_SPOT_LIGHTS_TOTAL z Lighting.h
    int numActiveSpotLights = 0;
    for (size_t i = 0; i < m_spotLights.size(); ++i) {
        if (numActiveSpotLights >= MAX_SPOT_LIGHTS_TOTAL) break; // Nie przekraczaj limitu shadera (zdefiniowanego w Lighting.h)

        if (m_spotLights[i].enabled) {
            std::string prefix = "spotLights[" + std::to_string(numActiveSpotLights) + "].";
            shader->setBool(prefix + "enabled", true);
            shader->setVec3(prefix + "position", m_spotLights[i].position);
            shader->setVec3(prefix + "direction", m_spotLights[i].direction);
            shader->setFloat(prefix + "cutOff", m_spotLights[i].cutOff);
            shader->setFloat(prefix + "outerCutOff", m_spotLights[i].outerCutOff);
            shader->setFloat(prefix + "constant", m_spotLights[i].constant);
            shader->setFloat(prefix + "linear", m_spotLights[i].linear);
            shader->setFloat(prefix + "quadratic", m_spotLights[i].quadratic);
            shader->setVec3(prefix + "ambient", m_spotLights[i].ambient);
            shader->setVec3(prefix + "diffuse", m_spotLights[i].diffuse);
            shader->setVec3(prefix + "specular", m_spotLights[i].specular);
            shader->setBool(prefix + "castsShadow", m_spotLights[i].castsShadow);
            shader->setInt(prefix + "shadowDataIndex", m_spotLights[i].shadowDataIndex);
            numActiveSpotLights++;
        }
    }
    shader->setInt("numSpotLights", numActiveSpotLights);

    // Deaktywuj pozostale sloty reflektorow w shaderze
    for (int i = numActiveSpotLights; i < MAX_SPOT_LIGHTS_TOTAL; ++i) { // Uzyj MAX_SPOT_LIGHTS_TOTAL z Lighting.h
        std::string prefix = "spotLights[" + std::to_string(i) + "].";
        shader->setBool(prefix + "enabled", false);
    }
    // Logger::getInstance().debug("LightingManager: Wysylanie uniformow swiatel zakonczone.");
}
