#include "ShadowSystem.h"
#include "Shader.h"
#include "ResourceManager.h"
#include "Logger.h"
#include "IRenderable.h"
#include "LightingManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <glad/glad.h>


ShadowSystem::ShadowSystem(unsigned int shadowMapWidth, unsigned int shadowMapHeight,
    unsigned int shadowCubeMapWidth, unsigned int shadowCubeMapHeight)
    : m_depthShader(nullptr),
    m_shadowMapWidth(shadowMapWidth), m_shadowMapHeight(shadowMapHeight),
    m_shadowCubeMapWidth(shadowCubeMapWidth), m_shadowCubeMapHeight(shadowCubeMapHeight),
    m_dirLightShadowMapper(nullptr) {
    // Konstruktor systemu cieni
}

bool ShadowSystem::initialize(ResourceManager& resourceManager, const std::string& depthShaderName,
    const std::string& depthVertexPath, const std::string& depthFragmentPath) {
    m_depthShader = resourceManager.loadShader(depthShaderName, depthVertexPath, depthFragmentPath);
    if (!m_depthShader || m_depthShader->getID() == 0) {
        Logger::getInstance().error("ShadowSystem: Nie udalo sie zaladowac shadera glebi '" + depthShaderName + "'.");
        return false;
    }

    m_dirLightShadowMapper = std::make_unique<ShadowMapper>(m_shadowMapWidth, m_shadowMapHeight, ShadowMapper::ShadowMapType::SHADOW_MAP_2D);
    if (!m_dirLightShadowMapper) {
        Logger::getInstance().fatal("ShadowSystem: Nie udalo sie utworzyc ShadowMapper dla swiatla kierunkowego.");
        return false;
    }
    m_dirLightShadowMapper->setDepthShader(m_depthShader);
    // Puste sciezki dla shadera w initialize ShadowMappera oznaczaja, ze shader jest juz ustawiony (setDepthShader)
    if (!m_dirLightShadowMapper->initialize(m_depthShader->getName(), "", "", &resourceManager)) {
        Logger::getInstance().fatal("ShadowSystem: Inicjalizacja FBO/Tekstury dla ShadowMapper swiatla kierunkowego nie powiodla sie.");
        m_dirLightShadowMapper.reset(); // Zwolnienie zasobu przed wyjsciem
        return false;
    }
    return true;
}

template <typename LightType, typename GetLightFunc>
bool ShadowSystem::enableLightShadowInternal(int globalLightIndex, bool enable, LightingManager& lightingManager,
    GetLightFunc getLight,
    std::vector<std::unique_ptr<ShadowMapper>>& shadowMappers,
    std::vector<int>& activeLightGlobalIndices,
    int maxShadowCastingLights,
    unsigned int newMapperWidth, unsigned int newMapperHeight,
    ShadowMapper::ShadowMapType mapType,
    int textureUnitOffset,
    const std::string& lightTypeName) {
    LightType* lightPtr = getLight(lightingManager, globalLightIndex);
    if (!lightPtr) {
        Logger::getInstance().warning("ShadowSystem::enable" + lightTypeName + "Shadow - " + lightTypeName + " nie znaleziony (indeks: " + std::to_string(globalLightIndex) + ")");
        return false;
    }
    LightType& light = *lightPtr;

    if (enable) {
        if (light.castsShadow && light.shadowMapperId != -1) {
            return true;
        }
        if (activeLightGlobalIndices.size() >= static_cast<size_t>(maxShadowCastingLights)) {
            Logger::getInstance().warning("ShadowSystem::enable" + lightTypeName + "Shadow - Osiagnieto limit (" + std::to_string(maxShadowCastingLights) + ") swiatel rzucajacych cien.");
            return false;
        }

        int availableMapperSlot = -1;
        // Szukanie istniejacego, nieuzywanego slota mappera
        for (size_t i = 0; i < shadowMappers.size(); ++i) {
            bool slotInUse = false;
            for (int activeGlobalIdx : activeLightGlobalIndices) {
                const LightType* activeLight = getLight(lightingManager, activeGlobalIdx);
                if (activeLight && activeLight->castsShadow && activeLight->shadowMapperId == static_cast<int>(i)) {
                    slotInUse = true;
                    break;
                }
            }
            if (!slotInUse && shadowMappers[i]) { // Sprawdzenie czy mapper istnieje (na wszelki wypadek)
                availableMapperSlot = static_cast<int>(i);
                break;
            }
        }

        // Jesli nie ma wolnego slota, stworz nowy mapper (jesli nie przekroczono limitu puli)
        if (availableMapperSlot == -1) {
            if (shadowMappers.size() >= static_cast<size_t>(maxShadowCastingLights)) {
                Logger::getInstance().error("ShadowSystem - Nie mozna utworzyc nowego ShadowMapper dla " + lightTypeName + ", osiagnieto limit puli.");
                return false;
            }
            auto newMapper = std::make_unique<ShadowMapper>(newMapperWidth, newMapperHeight, mapType);
            if (!newMapper) {
                Logger::getInstance().error("ShadowSystem - Blad alokacji nowego ShadowMapper dla " + lightTypeName + " " + std::to_string(globalLightIndex));
                return false;
            }
            newMapper->setDepthShader(m_depthShader); // Ustawienie shadera glebi
            // Puste sciezki dla shadera w initialize ShadowMappera oznaczaja, ze shader jest juz ustawiony
            if (!newMapper->initialize(m_depthShader->getName(), "", "", &ResourceManager::getInstance())) {
                Logger::getInstance().error("ShadowSystem - Blad inicjalizacji FBO/Tekstury dla nowego ShadowMapper (" + lightTypeName + " " + std::to_string(globalLightIndex) + ")");
                return false; // Nie dodajemy mappera do puli jesli inicjalizacja sie nie powiodla
            }
            shadowMappers.push_back(std::move(newMapper));
            availableMapperSlot = static_cast<int>(shadowMappers.size() - 1);
        }

        light.castsShadow = true;
        light.shadowMapperId = availableMapperSlot;
        light.shadowMapTextureUnit = BASE_SHADOW_MAP_TEXTURE_UNIT + textureUnitOffset + availableMapperSlot;
        activeLightGlobalIndices.push_back(globalLightIndex);

    }
    else { // Wylaczanie cieni
        if (light.castsShadow && light.shadowMapperId != -1) {
            // Usuniecie z listy aktywnych indeksow
            auto it = std::remove(activeLightGlobalIndices.begin(), activeLightGlobalIndices.end(), globalLightIndex);
            if (it != activeLightGlobalIndices.end()) {
                activeLightGlobalIndices.erase(it);
            }
            // Nie usuwamy mappera z puli `shadowMappers` - moze byc ponownie uzyty
            light.castsShadow = false;
            // light.shadowMapperId pozostaje, aby wskazac na potencjalnie reuzywalny mapper, ale nie jest juz "aktywny"
            light.shadowMapTextureUnit = -1; // Reset jednostki tekstury
            light.shadowDataIndex = -1;      // Wazne, aby zresetowac indeks danych shadera
        }
    }
    return true;
}


bool ShadowSystem::enableSpotLightShadow(int globalLightIndex, bool enable, LightingManager& lightingManager) {
    return enableLightShadowInternal<SpotLight>(
        globalLightIndex, enable, lightingManager,
        [](LightingManager& lm, int idx) { return lm.getSpotLight(idx); },
        m_spotLightShadowMappers,
        m_activeSpotLightGlobalIndices,
        MAX_SHADOW_CASTING_SPOT_LIGHTS,
        m_shadowMapWidth / 2, // Szerokosc dla mappera SpotLight
        m_shadowMapHeight / 2, // Wysokosc dla mappera SpotLight
        ShadowMapper::ShadowMapType::SHADOW_MAP_2D,
        0, // Offset jednostki tekstury dla SpotLight
        "SpotLight"
    );
}

bool ShadowSystem::enablePointLightShadow(int globalLightIndex, bool enable, LightingManager& lightingManager) {
    return enableLightShadowInternal<PointLight>(
        globalLightIndex, enable, lightingManager,
        [](LightingManager& lm, int idx) { return lm.getPointLight(idx); },
        m_pointLightShadowMappers,
        m_activePointLightGlobalIndices,
        MAX_SHADOW_CASTING_POINT_LIGHTS,
        m_shadowCubeMapWidth,  // Szerokosc dla mappera PointLight (cubemap)
        m_shadowCubeMapHeight, // Wysokosc dla mappera PointLight (cubemap)
        ShadowMapper::ShadowMapType::SHADOW_MAP_CUBEMAP,
        MAX_SHADOW_CASTING_SPOT_LIGHTS, // Offset jednostki tekstury dla PointLight
        "PointLight"
    );
}

void ShadowSystem::generateShadowMaps(LightingManager& lightingManager,
    const std::vector<IRenderable*>& renderables,
    int originalViewportWidth, int originalViewportHeight) {
    if (!m_depthShader || m_depthShader->getID() == 0) {
        Logger::getInstance().error("ShadowSystem::generateShadowMaps - Shader glebi nie jest zainicjalizowany lub jest nieprawidlowy.");
        return;
    }
    m_depthShader->use();

    // Cien swiatla kierunkowego
    DirectionalLight& dirLight = lightingManager.getDirectionalLight();
    if (dirLight.enabled && m_dirLightShadowMapper) {
        // TODO: Te wartosci powinny byc konfigurowalne lub obliczane dynamicznie na podstawie sceny  
        const float orthoBoxSize = 40.0f;
        const float lightNearPlane = 1.0f;
        const float lightFarPlaneDir = 75.0f; // Unikniecie konfliktu nazw z point light far plane
        const float lightPosOffset = 30.0f;
        const glm::vec3 sceneFocus(0.0f, 0.0f, 0.0f); // Powinno byc centrum sceny lub kamery

        m_dirLightShadowMapper->updateLightSpaceMatrixForDirectionalLight(
            dirLight.direction, sceneFocus, orthoBoxSize, orthoBoxSize,
            lightNearPlane, lightFarPlaneDir, lightPosOffset);
        m_depthShader->setBool("u_isCubeMapPass", false);
        renderSceneToDepthMap(m_dirLightShadowMapper.get(), m_dirLightShadowMapper->getLightSpaceMatrix(), renderables);
        m_dirLightShadowMapper->unbindAfterWriting(originalViewportWidth, originalViewportHeight);
    }

    // Cienie reflektorow (SpotLight)
    int currentSpotShadowShaderSlot = 0;
    for (int globalLightIndex : m_activeSpotLightGlobalIndices) {
        SpotLight* spotLightPtr = lightingManager.getSpotLight(globalLightIndex);
        // Sprawdzenie czy swiatlo nadal istnieje, jest wlaczone i ma poprawny mapper
        if (!spotLightPtr || !spotLightPtr->enabled || !spotLightPtr->castsShadow || spotLightPtr->shadowMapperId == -1 ||
            static_cast<size_t>(spotLightPtr->shadowMapperId) >= m_spotLightShadowMappers.size() || !m_spotLightShadowMappers[spotLightPtr->shadowMapperId]) {
            if (spotLightPtr) spotLightPtr->shadowDataIndex = -1; // Zresetuj, jesli cos jest nie tak
            continue;
        }
        SpotLight& spotLight = *spotLightPtr;

        if (currentSpotShadowShaderSlot >= MAX_SHADOW_CASTING_SPOT_LIGHTS) {
            spotLight.shadowDataIndex = -1; // Przekroczono limit slotow w shaderze
            continue; // Nie przetwarzaj wiecej niz pozwala shader
        }

        ShadowMapper* spotShadowMapper = m_spotLightShadowMappers[spotLight.shadowMapperId].get();
        // To sprawdzenie jest juz czesciowo wyzej, ale dla pewnosci
        if (!spotShadowMapper) {
            spotLight.shadowDataIndex = -1;
            continue;
        }

        // Obliczenia dla macierzy projekcji swiatla SpotLight
        float halfAngleRad = acos(spotLight.outerCutOff); // outerCutOff to cos(kata)
        float fovYDegrees = glm::degrees(halfAngleRad * 2.0f);
        float aspectRatioSM = static_cast<float>(spotShadowMapper->getShadowWidth()) / static_cast<float>(spotShadowMapper->getShadowHeight());

        // TODO: Uczynic stale konfigurowalnymi lub dynamicznymi  
        const float spotNearPlane = 0.1f;
        const float defaultSpotFarPlane = 75.0f;
        const float maxSpotFarPlane = 100.0f; // Ograniczenie zasiegu
        const float quadraticThreshold = 0.00001f; // Prog dla sensownego obliczenia zasiegu z tlumienia
        const float attenuationFactorForRange = 0.01f; // Wspolczynnik okreslajacy, przy jakim tlumieniu obliczamy zasieg

        float spotFarPlane = spotLight.quadratic > quadraticThreshold
            ? glm::sqrt(1.0f / (attenuationFactorForRange * spotLight.quadratic))
            : defaultSpotFarPlane;
        spotFarPlane = std::min(spotFarPlane, maxSpotFarPlane);

        spotShadowMapper->updateLightSpaceMatrixForSpotlight(
            spotLight.position, spotLight.direction, fovYDegrees, aspectRatioSM, spotNearPlane, spotFarPlane);

        m_depthShader->setBool("u_isCubeMapPass", false);
        renderSceneToDepthMap(spotShadowMapper, spotShadowMapper->getLightSpaceMatrix(), renderables);
        spotShadowMapper->unbindAfterWriting(originalViewportWidth, originalViewportHeight);
        spotLight.shadowDataIndex = currentSpotShadowShaderSlot++; // Przypisz slot shadera i inkrementuj
    }
    // Resetowanie shadowDataIndex dla reflektorow, ktore nie zostaly przetworzone w tej klatce
    // (np. wylaczone, przekroczyly limit aktywnych, usuniete)
    std::vector<SpotLight>& allSpotLights = lightingManager.getSpotLights();
    for (SpotLight& sl : allSpotLights) {
        bool processedThisFrame = false;
        if (sl.castsShadow && sl.shadowMapperId != -1 && sl.shadowDataIndex != -1) { // Musi byc aktywny i miec przypisany slot
            for (int activeIdx : m_activeSpotLightGlobalIndices) {
                if (lightingManager.getSpotLight(activeIdx) == &sl) { // Czy to jest to swiatlo z listy aktywnych?
                    // Dodatkowo, czy faktycznie dostalo slot w shaderze?
                    if (sl.shadowDataIndex < currentSpotShadowShaderSlot) { // Sprawdzenie, czy slot zostal uzyty
                        processedThisFrame = true;
                    }
                    break;
                }
            }
        }
        if (!processedThisFrame && sl.shadowDataIndex != -1) { // Jesli mial slot, ale nie zostal przetworzony
            sl.shadowDataIndex = -1;
        }
        else if (!sl.castsShadow && sl.shadowDataIndex != -1) { // Jesli nie rzuca cieni, a ma slot
            sl.shadowDataIndex = -1;
        }
    }


    // Cienie swiatel punktowych (PointLight)
    int currentPointShadowShaderSlot = 0;
    for (int globalLightIndex : m_activePointLightGlobalIndices) {
        PointLight* pointLightPtr = lightingManager.getPointLight(globalLightIndex);
        if (!pointLightPtr || !pointLightPtr->enabled || !pointLightPtr->castsShadow || pointLightPtr->shadowMapperId == -1 ||
            static_cast<size_t>(pointLightPtr->shadowMapperId) >= m_pointLightShadowMappers.size() || !m_pointLightShadowMappers[pointLightPtr->shadowMapperId]) {
            if (pointLightPtr) pointLightPtr->shadowDataIndex = -1;
            continue;
        }
        PointLight& pointLight = *pointLightPtr;

        if (currentPointShadowShaderSlot >= MAX_SHADOW_CASTING_POINT_LIGHTS) {
            pointLight.shadowDataIndex = -1;
            continue;
        }

        ShadowMapper* pointShadowMapper = m_pointLightShadowMappers[pointLight.shadowMapperId].get();
        if (!pointShadowMapper) {
            pointLight.shadowDataIndex = -1;
            continue;
        }

        renderSceneToCubeDepthMap(pointShadowMapper, pointLight, renderables);
        pointShadowMapper->unbindAfterWriting(originalViewportWidth, originalViewportHeight);
        pointLight.shadowDataIndex = currentPointShadowShaderSlot++;
    }
    // Resetowanie shadowDataIndex dla swiatel punktowych, ktore nie zostaly przetworzone
    std::vector<PointLight>& allPointLights = lightingManager.getPointLights();
    for (PointLight& pl : allPointLights) {
        bool processedThisFrame = false;
        if (pl.castsShadow && pl.shadowMapperId != -1 && pl.shadowDataIndex != -1) {
            for (int activeIdx : m_activePointLightGlobalIndices) {
                if (lightingManager.getPointLight(activeIdx) == &pl) {
                    if (pl.shadowDataIndex < currentPointShadowShaderSlot) {
                        processedThisFrame = true;
                    }
                    break;
                }
            }
        }
        if (!processedThisFrame && pl.shadowDataIndex != -1) {
            pl.shadowDataIndex = -1;
        }
        else if (!pl.castsShadow && pl.shadowDataIndex != -1) {
            pl.shadowDataIndex = -1;
        }
    }
}

void ShadowSystem::renderSceneToDepthMap(ShadowMapper* shadowMapper, const glm::mat4& lightSpaceMatrix,
    const std::vector<IRenderable*>& renderables) {
    if (!shadowMapper || !m_depthShader || m_depthShader->getID() == 0) {
        return; // Wczesne wyjscie, jesli brak wymaganych obiektow
    }

    shadowMapper->bindForWriting(); // Aktywacja FBO mappera
    glClear(GL_DEPTH_BUFFER_BIT);   // Czyszczenie bufora glebi

    m_depthShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
    // u_isCubeMapPass jest ustawiane w generateShadowMaps przed wywolaniem tej funkcji

    for (IRenderable* renderable : renderables) {
        if (renderable && renderable->castsShadow()) { // Renderuj tylko obiekty rzucajace cien
            renderable->renderForDepthPass(m_depthShader.get());
        }
    }
    // Unbind jest robiony w generateShadowMaps po zakonczeniu pracy z danym mapperem
}

void ShadowSystem::renderSceneToCubeDepthMap(ShadowMapper* shadowMapper, const PointLight& pointLight,
    const std::vector<IRenderable*>& renderables) {
    if (!shadowMapper || !m_depthShader || m_depthShader->getID() == 0 ||
        shadowMapper->getShadowMapType() != ShadowMapper::ShadowMapType::SHADOW_MAP_CUBEMAP) {
        return; // Wymagany mapper typu Cubemap
    }

    shadowMapper->updateLightSpaceMatricesForPointLight(pointLight.position, pointLight.shadowNearPlane, pointLight.shadowFarPlane);
    const auto& lightSpaceMatrices = shadowMapper->getLightSpaceMatrices();

    if (lightSpaceMatrices.size() != 6) { // Ochrona przed bledna liczba macierzy
        Logger::getInstance().error("ShadowSystem::renderSceneToCubeDepthMap - Nieprawidlowa liczba macierzy przestrzeni swiatla dla cubemapy.");
        return;
    }

    shadowMapper->bindForWriting(); // Aktywacja FBO mappera (dla cubemapy)

    // Ustawienia shadera specyficzne dla przejscia cubemapy
    m_depthShader->setBool("u_isCubeMapPass", true);
    m_depthShader->setVec3("u_lightPos_World", pointLight.position); // Pozycja swiatla w koordynatach swiata
    m_depthShader->setFloat("u_lightFarPlane", pointLight.shadowFarPlane); // Daleka plaszczyzna odciecia

    for (unsigned int i = 0; i < 6; ++i) { // Iteracja przez 6 scian cubemapy
        // Ustawienie odpowiedniej sciany cubemapy jako cel renderowania w FBO
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
            shadowMapper->getDepthMapTexture(), 0);
        glClear(GL_DEPTH_BUFFER_BIT); // Czyszczenie bufora glebi dla kazdej sciany

        m_depthShader->setMat4("lightSpaceMatrix", lightSpaceMatrices[i]); // Uzycie macierzy dla biezacej sciany

        for (IRenderable* renderable : renderables) {
            if (renderable && renderable->castsShadow()) {
                renderable->renderForDepthPass(m_depthShader.get());
            }
        }
    }
    // Unbind jest robiony w generateShadowMaps
}

void ShadowSystem::uploadShadowUniforms(std::shared_ptr<Shader> shader, LightingManager& lightingManager) const {
    if (!shader || shader->getID() == 0) {
        // Logger::getInstance().error("ShadowSystem::uploadShadowUniforms - Shader jest nieprawidlowy."); // Mozna odkomentowac w razie potrzeby
        return;
    }
    // Zaklada sie, ze shader jest juz aktywny (uzyty) przed wywolaniem tej funkcji

    // --- Cien swiatla kierunkowego ---
    const DirectionalLight& dirLight = lightingManager.getDirectionalLight();
    const int dirLightShadowMapUnit = 3; // Zgodnie z oryginalnym kodem
    shader->setInt("dirShadowMap", dirLightShadowMapUnit); // Ustawienie samplera nawet jesli nieaktywny

    if (dirLight.enabled && m_dirLightShadowMapper && m_dirLightShadowMapper->getDepthMapTexture() != 0) {
        shader->setMat4("dirLightSpaceMatrix", m_dirLightShadowMapper->getLightSpaceMatrix());
        glActiveTexture(GL_TEXTURE0 + dirLightShadowMapUnit);
        glBindTexture(GL_TEXTURE_2D, m_dirLightShadowMapper->getDepthMapTexture());
        if (m_dirLightShadowMapper->getShadowWidth() > 0) {
            shader->setFloat("shadowMapTexelSize", 1.0f / static_cast<float>(m_dirLightShadowMapper->getShadowWidth()));
        }
        shader->setBool("dirLightCastsShadow", true);
    }
    else {
        glActiveTexture(GL_TEXTURE0 + dirLightShadowMapUnit);
        glBindTexture(GL_TEXTURE_2D, 0); // Odpiecie tekstury, jesli nie uzywana
        shader->setBool("dirLightCastsShadow", false);
    }

    // --- Cienie reflektorow (SpotLight) ---
    int activeSpotShadowShaderSlotsUsed = 0;
    const auto& spotLightsVec = lightingManager.getSpotLights();

    for (const auto& spotLight : spotLightsVec) {
        std::string baseUniformNameFS = "spotLightShadowData[" + std::to_string(activeSpotShadowShaderSlotsUsed) + "].";
        std::string baseUniformNameVS = "spotLightSpaceMatricesVS[" + std::to_string(activeSpotShadowShaderSlotsUsed) + "]";

        if (spotLight.enabled && spotLight.castsShadow && spotLight.shadowDataIndex != -1 &&
            spotLight.shadowDataIndex < MAX_SHADOW_CASTING_SPOT_LIGHTS && // Upewnij sie, ze shadowDataIndex jest poprawny
            activeSpotShadowShaderSlotsUsed < MAX_SHADOW_CASTING_SPOT_LIGHTS) { // Dodatkowe zabezpieczenie

            // Uzyj shadowDataIndex przypisanego podczas generateShadowMaps jako indeksu w tablicy uniformow
            // Zakladamy, ze shadowDataIndex jest ciagly od 0 do N-1 dla aktywnych swiatel.
            // Jesli nie, logika tutaj i w shaderze musialaby byc bardziej skomplikowana.
            // W obecnej implementacji generateShadowMaps stara sie zapewnic ciaglosc.
            int shaderSlot = spotLight.shadowDataIndex; // To jest slot, ktory zostal przypisany w generateShadowMaps
            // Nalezy sie upewnic, ze spotLight.shadowDataIndex odpowiada faktycznemu slotowi, ktory chcemy tu uzyc.
            // Dla uproszczenia, zalozmy, ze chcemy wypelnic kolejne sloty w shaderze az do MAX.
            // Jesli shadowDataIndex jest -1, to swiatlo nie rzuca cienia w tej klatce.
            // Ta petla iteruje po wszystkich swiatlach, wiec musimy wybrac te, ktore maja aktywny shadowDataIndex.

            ShadowMapper* spotShadowMapper = getSpotLightShadowMapperByID(spotLight.shadowMapperId);
            if (spotShadowMapper && spotShadowMapper->getDepthMapTexture() != 0) {
                // Uzywamy 'activeSpotShadowShaderSlotsUsed' jako indeksu do tablicy uniformow w shaderze
                baseUniformNameFS = "spotLightShadowData[" + std::to_string(activeSpotShadowShaderSlotsUsed) + "].";
                baseUniformNameVS = "spotLightSpaceMatricesVS[" + std::to_string(activeSpotShadowShaderSlotsUsed) + "]";

                shader->setMat4(baseUniformNameVS, spotShadowMapper->getLightSpaceMatrix());

                glActiveTexture(GL_TEXTURE0 + spotLight.shadowMapTextureUnit);
                glBindTexture(GL_TEXTURE_2D, spotShadowMapper->getDepthMapTexture());
                shader->setInt(baseUniformNameFS + "shadowMap", spotLight.shadowMapTextureUnit);
                if (spotShadowMapper->getShadowWidth() > 0) {
                    shader->setFloat(baseUniformNameFS + "texelSize", 1.0f / static_cast<float>(spotShadowMapper->getShadowWidth()));
                }
                shader->setBool(baseUniformNameFS + "enabled", true);
                activeSpotShadowShaderSlotsUsed++;
            }
        }
    }
    shader->setInt("numActiveSpotShadowCastersFS", activeSpotShadowShaderSlotsUsed);
    shader->setInt("numActiveSpotShadowCastersVS", activeSpotShadowShaderSlotsUsed); // Jesli VS rowniez potrzebuje tej informacji

    // Wylaczanie nieuzywanych slotow dla SpotLight
    for (int i = activeSpotShadowShaderSlotsUsed; i < MAX_SHADOW_CASTING_SPOT_LIGHTS; ++i) {
        std::string baseFS = "spotLightShadowData[" + std::to_string(i) + "].";
        shader->setBool(baseFS + "enabled", false);
    }


    // --- Cienie swiatel punktowych (PointLight) ---
    int activePointShadowShaderSlotsUsed = 0;
    const auto& pointLightsVec = lightingManager.getPointLights();

    for (const auto& pointLight : pointLightsVec) {
        std::string baseUniformNameFS = "pointLightShadowData[" + std::to_string(activePointShadowShaderSlotsUsed) + "].";

        if (pointLight.enabled && pointLight.castsShadow && pointLight.shadowDataIndex != -1 &&
            pointLight.shadowDataIndex < MAX_SHADOW_CASTING_POINT_LIGHTS && // Poprawny shadowDataIndex
            activePointShadowShaderSlotsUsed < MAX_SHADOW_CASTING_POINT_LIGHTS) {

            ShadowMapper* pointShadowMapper = getPointLightShadowMapperByID(pointLight.shadowMapperId);
            if (pointShadowMapper && pointShadowMapper->getDepthMapTexture() != 0) {
                // Uzywamy 'activePointShadowShaderSlotsUsed' jako indeksu do tablicy uniformow
                baseUniformNameFS = "pointLightShadowData[" + std::to_string(activePointShadowShaderSlotsUsed) + "].";

                glActiveTexture(GL_TEXTURE0 + pointLight.shadowMapTextureUnit);
                glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowMapper->getDepthMapTexture());
                shader->setInt(baseUniformNameFS + "shadowCubeMap", pointLight.shadowMapTextureUnit);
                shader->setFloat(baseUniformNameFS + "farPlane", pointLight.shadowFarPlane); // Far plane dla tego swiatla
                shader->setBool(baseUniformNameFS + "enabled", true);
                activePointShadowShaderSlotsUsed++;
            }
        }
    }
    shader->setInt("numActivePointShadowCastersFS", activePointShadowShaderSlotsUsed);

    // Wylaczanie nieuzywanych slotow dla PointLight
    for (int i = activePointShadowShaderSlotsUsed; i < MAX_SHADOW_CASTING_POINT_LIGHTS; ++i) {
        std::string baseFS = "pointLightShadowData[" + std::to_string(i) + "].";
        shader->setBool(baseFS + "enabled", false);
    }
}


ShadowMapper* ShadowSystem::getSpotLightShadowMapperByID(int mapperId) const {
    if (mapperId >= 0 && static_cast<size_t>(mapperId) < m_spotLightShadowMappers.size()) {
        return m_spotLightShadowMappers[mapperId].get();
    }
    // Logger::getInstance().warning("ShadowSystem::getSpotLightShadowMapperByID - Nieprawidlowy ID mappera: " + std::to_string(mapperId));
    return nullptr;
}

ShadowMapper* ShadowSystem::getPointLightShadowMapperByID(int mapperId) const {
    if (mapperId >= 0 && static_cast<size_t>(mapperId) < m_pointLightShadowMappers.size()) {
        return m_pointLightShadowMappers[mapperId].get();
    }
    // Logger::getInstance().warning("ShadowSystem::getPointLightShadowMapperByID - Nieprawidlowy ID mappera: " + std::to_string(mapperId));
    return nullptr;
}

void ShadowSystem::onSpotLightRemoved(int globalLightIndex, LightingManager& lightingManager) {
    // Sprawdzenie, czy swiatlo istnieje jest robione wewnatrz enableSpotLightShadow
    // Jesli swiatlo rzucalo cien, zostanie ono wylaczone, a zasoby (mapper) oznaczone jako dostepne
    enableSpotLightShadow(globalLightIndex, false, lightingManager);
}

void ShadowSystem::onPointLightRemoved(int globalLightIndex, LightingManager& lightingManager) {
    enablePointLightShadow(globalLightIndex, false, lightingManager);
}

void ShadowSystem::clearAllShadowCasters(LightingManager& lightingManager) {
    // Kopiujemy wektory indeksow, poniewaz enable...Shadow modyfikuje oryginalne listy
    std::vector<int> activeSpots = m_activeSpotLightGlobalIndices;
    for (int globalLightIndex : activeSpots) {
        enableSpotLightShadow(globalLightIndex, false, lightingManager);
    }
    std::vector<int> activePoints = m_activePointLightGlobalIndices;
    for (int globalLightIndex : activePoints) {
        enablePointLightShadow(globalLightIndex, false, lightingManager);
    }
    // Logger::getInstance().info("ShadowSystem: Wszystkie swiatla rzucajace cienie (listy aktywne) wyczyszczone.");
}

// Usunieto zbedna klamre zamykajaca, ktora mogla byc pozostaloscia po namespace