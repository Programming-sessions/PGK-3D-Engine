/**
* @file ShadowSystem.h
* @brief Definicja klasy ShadowSystem.
*
* Plik ten zawiera deklarację klasy ShadowSystem, która zarządza
* procesem generowania i renderowania cieni w scenie.
*/
#ifndef SHADOWSYSTEM_H
#define SHADOWSYSTEM_H

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>

#include "ShadowMapper.h" // Wymagany dla ShadowMapper
#include "Lighting.h"     // Wymagany dla PointLight, etc. w deklaracjach metod

// Deklaracje wyprzedzajace dla typow uzywanych tylko jako wskazniki/referencje w naglowku
class Shader;
class IRenderable;
// BasePrimitive nie jest bezposrednio uzywany w interfejsie publicznym/prywatnym ShadowSystem, wiec mozna rozwazyc jego usuniecie, jesli nie jest potrzebny dla typow zawartych
class LightingManager;
class ResourceManager;

/**
 * @brief System zarzadzajacy generowaniem i obsluga map cieni.
 * * Odpowiada za inicjalizacje zasobow potrzebnych do renderowania cieni,
 * wlaczanie/wylaczanie cieni dla poszczegolnych swiatel, generowanie
 * map cieni (zarowno 2D jak i kubicznych) oraz dostarczanie danych
 * o cieniach do shaderow oswietlenia.
 */
class ShadowSystem {
public:
    /**
     * @brief Konstruktor systemu cieni.
     * @param shadowMapWidth Szerokosc tekstur map cieni 2D (dla swiatel kierunkowych i reflektorow).
     * @param shadowMapHeight Wysokosc tekstur map cieni 2D.
     * @param shadowCubeMapWidth Szerokosc (i wysokosc) scian tekstur map cieni kubicznych (dla swiatel punktowych).
     * @param shadowCubeMapHeight Wysokosc (i szerokosc, chociaz dla cubemap zwykle sa rowne) scian tekstur map cieni kubicznych.
     */
    ShadowSystem(unsigned int shadowMapWidth, unsigned int shadowMapHeight,
        unsigned int shadowCubeMapWidth, unsigned int shadowCubeMapHeight);

    /**
     * @brief Destruktor domyslny.
     */
    ~ShadowSystem() = default;

    // Zapobieganie kopiowaniu i przypisaniu
    ShadowSystem(const ShadowSystem&) = delete;
    ShadowSystem& operator=(const ShadowSystem&) = delete;
    ShadowSystem(ShadowSystem&&) = default; // Pozwolenie na przenoszenie
    ShadowSystem& operator=(ShadowSystem&&) = default; // Pozwolenie na przypisanie przenoszace


    /**
     * @brief Inicjalizuje system cieni.
     * * Laduje shadery glebi i tworzy wstepne mappery cieni.
     * @param resourceManager Referencja do menedzera zasobow.
     * @param depthShaderName Nazwa shadera glebi do zaladowania/uzycia.
     * @param depthVertexPath Sciezka do vertex shadera glebi.
     * @param depthFragmentPath Sciezka do fragment shadera glebi.
     * @return true Jesli inicjalizacja powiodla sie, false w przeciwnym wypadku.
     */
    bool initialize(ResourceManager& resourceManager, const std::string& depthShaderName,
        const std::string& depthVertexPath, const std::string& depthFragmentPath);

    /**
     * @brief Wlacza lub wylacza rzucanie cieni dla danego swiatla typu SpotLight.
     * @param globalLightIndex Globalny indeks swiatla SpotLight w LightingManager.
     * @param enable true aby wlaczyc cienie, false aby wylaczyc.
     * @param lightingManager Referencja do menedzera oswietlenia.
     * @return true Jesli operacja sie powiodla, false jesli np. swiatlo nie istnieje lub osiagnieto limit.
     */
    bool enableSpotLightShadow(int globalLightIndex, bool enable, LightingManager& lightingManager);

    /**
     * @brief Wlacza lub wylacza rzucanie cieni dla danego swiatla typu PointLight.
     * @param globalLightIndex Globalny indeks swiatla PointLight w LightingManager.
     * @param enable true aby wlaczyc cienie, false aby wylaczyc.
     * @param lightingManager Referencja do menedzera oswietlenia.
     * @return true Jesli operacja sie powiodla, false jesli np. swiatlo nie istnieje lub osiagnieto limit.
     */
    bool enablePointLightShadow(int globalLightIndex, bool enable, LightingManager& lightingManager);

    /**
     * @brief Generuje mapy cieni dla wszystkich aktywnych swiatel rzucajacych cienie.
     * @param lightingManager Referencja do menedzera oswietlenia.
     * @param renderables Wektor wskaznikow do obiektow renderowalnych w scenie.
     * @param originalViewportWidth Oryginalna szerokosc viewportu (do jego przywrocenia).
     * @param originalViewportHeight Oryginalna wysokosc viewportu (do jego przywrocenia).
     */
    void generateShadowMaps(LightingManager& lightingManager,
        const std::vector<IRenderable*>& renderables,
        int originalViewportWidth, int originalViewportHeight);

    /**
     * @brief Wysyla uniformy zwiazane z cieniami do podanego shadera.
     * * Metoda ta powinna byc wolana po aktywacji shadera, ktory bedzie uzywal map cieni.
     * @param shader Wskaznik do shadera, do ktorego maja byc wyslane dane.
     * @param lightingManager Referencja do menedzera oswietlenia.
     */
    void uploadShadowUniforms(std::shared_ptr<Shader> shader, LightingManager& lightingManager) const;

    /**
     * @brief Zwraca wskaznik do mappera cieni dla swiatla kierunkowego.
     * @return Wskaznik do ShadowMapper lub nullptr, jesli nie zainicjalizowany.
     */
    ShadowMapper* getDirLightShadowMapper() const { return m_dirLightShadowMapper.get(); }

    /**
     * @brief Zwraca wskaznik do mappera cieni dla swiatla SpotLight o podanym ID mappera.
     * @param mapperId ID mappera (przechowywane w obiekcie SpotLight).
     * @return Wskaznik do ShadowMapper lub nullptr, jesli ID jest nieprawidlowe.
     */
    ShadowMapper* getSpotLightShadowMapperByID(int mapperId) const;

    /**
     * @brief Zwraca wskaznik do mappera cieni dla swiatla PointLight o podanym ID mappera.
     * @param mapperId ID mappera (przechowywane w obiekcie PointLight).
     * @return Wskaznik do ShadowMapper lub nullptr, jesli ID jest nieprawidlowe.
     */
    ShadowMapper* getPointLightShadowMapperByID(int mapperId) const;

    /**
     * @brief Zwraca liczbe aktywnych swiatel SpotLight rzucajacych cienie.
     * @return Liczba aktywnych SpotLight rzucajacych cienie.
     */
    size_t getActiveSpotLightShadowCastersCount() const { return m_activeSpotLightGlobalIndices.size(); }

    /**
     * @brief Zwraca liczbe aktywnych swiatel PointLight rzucajacych cienie.
     * @return Liczba aktywnych PointLight rzucajacych cienie.
     */
    size_t getActivePointLightShadowCastersCount() const { return m_activePointLightGlobalIndices.size(); }

    /**
     * @brief Wywolywana, gdy swiatlo SpotLight jest usuwane z systemu.
     * * Dezaktywuje cienie dla tego swiatla, jesli byly wlaczone.
     * @param globalLightIndex Globalny indeks usuwanego swiatla SpotLight.
     * @param lightingManager Referencja do menedzera oswietlenia.
     */
    void onSpotLightRemoved(int globalLightIndex, LightingManager& lightingManager);

    /**
     * @brief Wywolywana, gdy swiatlo PointLight jest usuwane z systemu.
     * * Dezaktywuje cienie dla tego swiatla, jesli byly wlaczone.
     * @param globalLightIndex Globalny indeks usuwanego swiatla PointLight.
     * @param lightingManager Referencja do menedzera oswietlenia.
     */
    void onPointLightRemoved(int globalLightIndex, LightingManager& lightingManager);

    /**
     * @brief Dezaktywuje rzucanie cieni dla wszystkich obecnie aktywnych swiatel.
     * @param lightingManager Referencja do menedzera oswietlenia.
     */
    void clearAllShadowCasters(LightingManager& lightingManager);

private:
    std::shared_ptr<Shader> m_depthShader;
    unsigned int m_shadowMapWidth;
    unsigned int m_shadowMapHeight;
    unsigned int m_shadowCubeMapWidth;
    unsigned int m_shadowCubeMapHeight;

    std::unique_ptr<ShadowMapper> m_dirLightShadowMapper;
    std::vector<std::unique_ptr<ShadowMapper>> m_spotLightShadowMappers;
    std::vector<std::unique_ptr<ShadowMapper>> m_pointLightShadowMappers;

    std::vector<int> m_activeSpotLightGlobalIndices; // Przechowuje globalne indeksy aktywnych swiatel SpotLight
    std::vector<int> m_activePointLightGlobalIndices; // Przechowuje globalne indeksy aktywnych swiatel PointLight

    // Stale dla logiki wewnetrznej, np. maksymalna liczba swiatel rzucajacych cienie
    // Powinny byc zdefiniowane tutaj lub w bardziej globalnym miejscu, jesli sa wspoldzielone.
    // Na potrzeby tego przykladu zakladam, ze MAX_SHADOW_CASTING_SPOT_LIGHTS i MAX_SHADOW_CASTING_POINT_LIGHTS sa zdefiniowane gdzies indziej (np. Lighting.h)
    // static const int MAX_SHADOW_CASTING_SPOT_LIGHTS = 4; // Przykladowa wartosc
    // static const int MAX_SHADOW_CASTING_POINT_LIGHTS = 2; // Przykladowa wartosc
    static const int BASE_SHADOW_MAP_TEXTURE_UNIT = 4; // Bazowa jednostka teksturujaca dla map cieni (po kierunkowej)


    /**
     * @brief Renderuje scene do mapy glebi 2D z perspektywy danego swiatla.
     * @param shadowMapper Wskaznik do mappera cieni, ktory ma byc uzyty.
     * @param lightSpaceMatrix Macierz transformacji do przestrzeni swiatla.
     * @param renderables Wektor wskaznikow do obiektow renderowalnych.
     */
    void renderSceneToDepthMap(ShadowMapper* shadowMapper, const glm::mat4& lightSpaceMatrix,
        const std::vector<IRenderable*>& renderables);

    /**
     * @brief Renderuje scene do szesciu scian mapy glebi kubicznej z perspektywy danego swiatla punktowego.
     * @param shadowMapper Wskaznik do mappera cieni kubicznych, ktory ma byc uzyty.
     * @param pointLight Referencja do swiatla punktowego.
     * @param renderables Wektor wskaznikow do obiektow renderowalnych.
     */
    void renderSceneToCubeDepthMap(ShadowMapper* shadowMapper, const PointLight& pointLight,
        const std::vector<IRenderable*>& renderables);

    /**
     * @brief Wewnetrzna metoda pomocnicza do zarzadzania wlaczaniem/wylaczaniem cieni.
     * * Ta metoda jest szablonem, aby obsluzyc zarowno SpotLight jak i PointLight,
     * redukujac duplikacje kodu.
     * @tparam LightType Typ swiatla (SpotLight lub PointLight).
     * @tparam GetLightFunc Typ funkcji pobierajacej swiatlo z LightingManager.
     * @param globalLightIndex Globalny indeks swiatla.
     * @param enable Czy wlaczyc cienie.
     * @param lightingManager Menedzer oswietlenia.
     * @param getLight Lambda lub wskaznik do funkcji pobierajacej konkretny typ swiatla.
     * @param shadowMappers Wektor unikalnych wskaznikow do mapperow cieni.
     * @param activeLightGlobalIndices Wektor globalnych indeksow aktywnych swiatel rzucajacych cienie.
     * @param maxShadowCastingLights Maksymalna liczba swiatel tego typu mogacych rzucac cienie.
     * @param shadowMapWidth Szerokosc mapy cieni dla nowego mappera.
     * @param shadowMapHeight Wysokosc mapy cieni dla nowego mappera.
     * @param mapType Typ mapy cieni (2D lub Cubemap).
     * @param textureUnitOffset Przesuniecie dla jednostki teksturujacej (0 dla SpotLight, MAX_SHADOW_CASTING_SPOT_LIGHTS dla PointLight).
     * @param lightTypeName Nazwa typu swiatla uzywana w logach.
     * @return true jesli operacja sie powiodla.
     */
    template <typename LightType, typename GetLightFunc>
    bool enableLightShadowInternal(int globalLightIndex, bool enable, LightingManager& lightingManager,
        GetLightFunc getLight,
        std::vector<std::unique_ptr<ShadowMapper>>& shadowMappers,
        std::vector<int>& activeLightGlobalIndices,
        int maxShadowCastingLights,
        unsigned int shadowMapWidth, unsigned int shadowMapHeight,
        ShadowMapper::ShadowMapType mapType,
        int textureUnitOffset,
        const std::string& lightTypeName);
};

#endif // SHADOWSYSTEM_H