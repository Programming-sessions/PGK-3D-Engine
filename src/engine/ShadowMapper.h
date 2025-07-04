/**
* @file ShadowMapper.h
* @brief Definicja klasy ShadowMapper.
*
* Plik ten zawiera deklarację klasy ShadowMapper, która implementuje
* technikę mapowania cieni (shadow mapping).
*/
#ifndef SHADOWMAPPER_H
#define SHADOWMAPPER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <memory>
#include <vector>

class Shader;
class ResourceManager; // Deklaracja wyprzedzajaca

/**
 * @brief Zarzadza pojedyncza mapa cieni (2D lub Cubemap).
 * * Odpowiada za tworzenie i konfiguracje Framebuffer Object (FBO) dla mapy glebi,
 * aktualizacje macierzy przestrzeni swiatla oraz udostepnianie tekstury glebi.
 */
class ShadowMapper {
public:
    /**
     * @brief Typ mapy cieni obslugiwanej przez mapper.
     */
    enum class ShadowMapType {
        SHADOW_MAP_2D,      ///< Standardowa, dwuwymiarowa mapa cieni.
        SHADOW_MAP_CUBEMAP  ///< Szescienna mapa cieni (dla swiatel punktowych).
    };

    /**
     * @brief Konstruktor.
     * @param shadowWidth Szerokosc tekstury mapy cieni w pikselach.
     * @param shadowHeight Wysokosc tekstury mapy cieni w pikselach.
     * @param type Typ mapy cieni (domyslnie 2D).
     */
    ShadowMapper(unsigned int shadowWidth, unsigned int shadowHeight, ShadowMapType type = ShadowMapType::SHADOW_MAP_2D);

    /**
     * @brief Destruktor. Zwalnia zasoby OpenGL.
     */
    ~ShadowMapper();

    // Regula Piatki: Zapobieganie kopiowaniu, implementacja przenoszenia.
    ShadowMapper(const ShadowMapper&) = delete;
    ShadowMapper& operator=(const ShadowMapper&) = delete;

    /**
     * @brief Konstruktor przenoszacy.
     * @param other Obiekt ShadowMapper do przeniesienia.
     */
    ShadowMapper(ShadowMapper&& other) noexcept;

    /**
     * @brief Operator przypisania przenoszacego.
     * @param other Obiekt ShadowMapper do przeniesienia.
     * @return Referencja do tego obiektu.
     */
    ShadowMapper& operator=(ShadowMapper&& other) noexcept;

    /**
     * @brief Inicjalizuje Framebuffer Object (FBO) i teksture glebi.
     * * Moze opcjonalnie zaladowac shader glebi, jesli nie zostal wczesniej ustawiony
     * i podano odpowiednie sciezki oraz ResourceManager.
     * @param shaderName Nazwa shadera glebi (do pobrania z ResourceManager).
     * @param depthVertexPath Sciezka do vertex shadera glebi (jesli ladowany).
     * @param depthFragmentPath Sciezka do fragment shadera glebi (jesli ladowany).
     * @param resourceManager Wskaznik do menedzera zasobow (opcjonalny).
     * @return true Jesli inicjalizacja powiodla sie, false w przeciwnym wypadku.
     */
    bool initialize(const std::string& shaderName,
        const std::string& depthVertexPath,
        const std::string& depthFragmentPath,
        ResourceManager* resourceManager = nullptr);

    /**
     * @brief Aktualizuje macierz przestrzeni swiatla dla swiatla kierunkowego.
     * @param lightDirection Znormalizowany wektor kierunku swiatla.
     * @param sceneFocusPoint Punkt w scenie, na ktory skierowane jest swiatlo (centrum projekcji ortogonalnej).
     * @param orthoBoxWidth Szerokosc obszaru projekcji ortogonalnej.
     * @param orthoBoxHeight Wysokosc obszaru projekcji ortogonalnej.
     * @param nearPlane Bliska plaszczyzna odciecia.
     * @param farPlane Daleka plaszczyzna odciecia.
     * @param lightPositionOffset Przesuniecie pozycji swiatla wzdluz jego kierunku od `sceneFocusPoint`.
     */
    void updateLightSpaceMatrixForDirectionalLight(
        const glm::vec3& lightDirection,
        const glm::vec3& sceneFocusPoint,
        float orthoBoxWidth,
        float orthoBoxHeight,
        float nearPlane,
        float farPlane,
        float lightPositionOffset
    );

    /**
     * @brief Aktualizuje macierz przestrzeni swiatla dla swiatla typu SpotLight.
     * @param lightPosition Pozycja swiatla w przestrzeni swiata.
     * @param lightDirection Znormalizowany wektor kierunku swiatla.
     * @param fovYDegrees Kat widzenia (FOV) w pionie, w stopniach.
     * @param aspectRatio Wspolczynnik proporcji (szerokosc / wysokosc) mapy cieni.
     * @param nearPlane Bliska plaszczyzna odciecia.
     * @param farPlane Daleka plaszczyzna odciecia.
     */
    void updateLightSpaceMatrixForSpotlight(
        const glm::vec3& lightPosition,
        const glm::vec3& lightDirection,
        float fovYDegrees,
        float aspectRatio,
        float nearPlane,
        float farPlane
    );

    /**
     * @brief Aktualizuje macierze przestrzeni swiatla (6) dla swiatla punktowego (Cubemap).
     * @param lightPosition Pozycja swiatla w przestrzeni swiata.
     * @param nearPlane Bliska plaszczyzna odciecia.
     * @param farPlane Daleka plaszczyzna odciecia.
     */
    void updateLightSpaceMatricesForPointLight(
        const glm::vec3& lightPosition,
        float nearPlane,
        float farPlane
    );

    /**
     * @brief Aktywuje FBO tego mappera do zapisu (renderowania mapy glebi).
     * Ustawia rowniez odpowiedni viewport.
     */
    void bindForWriting();

    /**
     * @brief Deaktywuje FBO (przywraca domyslny framebuffer) i oryginalny viewport.
     * @param originalViewportWidth Oryginalna szerokosc viewportu do przywrocenia.
     * @param originalViewportHeight Oryginalna wysokosc viewportu do przywrocenia.
     */
    void unbindAfterWriting(int originalViewportWidth, int originalViewportHeight);

    /**
     * @brief Zwraca ID tekstury glebi OpenGL.
     * @return ID tekstury glebi.
     */
    GLuint getDepthMapTexture() const { return m_depthMapTextureID; }

    /**
     * @brief Zwraca macierz przestrzeni swiatla (glownie dla map 2D).
     * Jesli macierze nie sa dostepne, zwraca macierz jednostkowa.
     * @return Const referencja do macierzy przestrzeni swiatla.
     */
    const glm::mat4& getLightSpaceMatrix() const;

    /**
     * @brief Zwraca wektor macierzy przestrzeni swiatla (glownie dla Cubemap).
     * @return Const referencja do wektora macierzy.
     */
    const std::vector<glm::mat4>& getLightSpaceMatrices() const { return m_lightSpaceMatrices; }

    /**
     * @brief Zwraca wspoldzielony wskaznik do shadera glebi.
     * @return std::shared_ptr<Shader> do shadera glebi.
     */
    std::shared_ptr<Shader> getDepthShader() const { return m_depthShader; }

    /**
     * @brief Ustawia shader glebi dla tego mappera.
     * @param shader Wspoldzielony wskaznik do shadera glebi.
     */
    void setDepthShader(std::shared_ptr<Shader> shader);

    /**
     * @brief Zwraca szerokosc mapy cieni.
     * @return Szerokosc w pikselach.
     */
    unsigned int getShadowWidth() const { return m_shadowWidth; }

    /**
     * @brief Zwraca wysokosc mapy cieni.
     * @return Wysokosc w pikselach.
     */
    unsigned int getShadowHeight() const { return m_shadowHeight; }

    /**
     * @brief Zwraca typ mapy cieni.
     * @return Typ mapy cieni (2D lub Cubemap).
     */
    ShadowMapType getShadowMapType() const { return m_shadowMapType; }

private:
    GLuint m_depthMapFBO;           ///< ID Framebuffer Object.
    GLuint m_depthMapTextureID;     ///< ID tekstury glebi.
    unsigned int m_shadowWidth;     ///< Szerokosc mapy cieni.
    unsigned int m_shadowHeight;    ///< Wysokosc mapy cieni.
    std::vector<glm::mat4> m_lightSpaceMatrices; ///< Macierz(e) transformacji do przestrzeni swiatla.
    std::shared_ptr<Shader> m_depthShader;      ///< Shader uzywany do renderowania mapy glebi.
    ShadowMapType m_shadowMapType;  ///< Typ mapy cieni.

    /**
     * @brief Prywatna metoda do zwalniania zasobow OpenGL (FBO i tekstury).
     */
    void cleanup();

    // Stale uzywane wewnetrznie
    static const float UP_VECTOR_THRESHOLD;       ///< Prog dla wyboru wektora "up" przy obliczaniu macierzy widoku.
    static const float SHADOW_BORDER_COLOR[4];    ///< Kolor ramki dla tekstur map cieni 2D.
};

#endif // SHADOWMAPPER_H