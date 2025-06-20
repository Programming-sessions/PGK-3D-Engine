/**
* @file Lighting.h
* @brief Definicje struktur związanych z oświetleniem.
*
* Plik ten zawiera definicje struktur używanych do reprezentacji
* różnych typów świateł w scenie, takich jak światło kierunkowe,
* punktowe czy reflektorowe.
*/
#ifndef LIGHTING_H
#define LIGHTING_H

#include <glm/glm.hpp> // Potrzebne dla typow wektorowych glm::vec3
#include <memory>      // Potrzebne dla std::shared_ptr
#include "Texture.h"   // Potrzebne dla definicji klasy Texture

// --- Stale definiujace maksymalna liczbe swiatel ---

/**
 * @brief Maksymalna liczba swiatel punktowych obslugiwana jednoczesnie w scenie.
 * Wplywa na rozmiar tablic uniformow w shaderach.
 */
const int MAX_POINT_LIGHTS = 4;

/**
 * @brief Maksymalna laczna liczba reflektorow (SpotLight) obslugiwana w scenie.
 * Dotyczy to wszystkich reflektorow, zarowno rzucajacych cien, jak i nie.
 */
const int MAX_SPOT_LIGHTS_TOTAL = 4;

/**
 * @brief Maksymalna liczba reflektorow (SpotLight) mogacych jednoczesnie rzucac cien.
 * Wazne dla zarzadzania zasobami map cieni.
 */
const int MAX_SHADOW_CASTING_SPOT_LIGHTS = 4;

/**
 * @brief Maksymalna liczba swiatel punktowych (PointLight) mogacych jednoczesnie rzucac cien.
 * Wazne dla zarzadzania zasobami map cieni (cube maps).
 */
const int MAX_SHADOW_CASTING_POINT_LIGHTS = 4;

/**
 * @struct Material
 * @brief Struktura definiujaca wlasciwosci materialu powierzchni obiektu.
 * Okresla, jak obiekt oddzialuje ze swiatlem.
 */
struct Material {
    /** @brief Kolor swiatla otoczenia odbijanego przez material. */
    glm::vec3 ambient;
    /** @brief Kolor swiatla rozproszonego odbijanego przez material (jesli brak tekstury diffuse). */
    glm::vec3 diffuse;
    /** @brief Kolor swiatla lustrzanego odbijanego przez material (jesli brak tekstury specular). */
    glm::vec3 specular;
    /** @brief Wspolczynnik polysku materialu, wplywa na rozmiar i intensywnosc odbic lustrzanych. */
    float shininess;

    /** @brief Wskaznik na teksture rozpraszania (diffuse map), definiujaca bazowy kolor obiektu. */
    std::shared_ptr<Texture> diffuseTexture;
    /** @brief Wskaznik na teksture odbic lustrzanych (specular map), definiujaca intensywnosc odbic lustrzanych. */
    std::shared_ptr<Texture> specularTexture;
    // Mozna dodac wiecej typow tekstur w przyszlosci, np. normal map, height map, ambient occlusion map
    // std::shared_ptr<Texture> normalTexture;
    // std::shared_ptr<Texture> heightTexture;

    /**
     * @brief Konstruktor domyslny.
     * Inicjalizuje material domyslnymi wartosciami.
     */
    Material() :
        ambient(0.1f, 0.1f, 0.1f),
        diffuse(0.7f, 0.7f, 0.7f),
        specular(0.5f, 0.5f, 0.5f),
        shininess(32.0f),
        diffuseTexture(nullptr),
        specularTexture(nullptr)
        // normalTexture(nullptr),
        // heightTexture(nullptr)
    {
    }
};

/**
 * @struct DirectionalLight
 * @brief Struktura definiujaca wlasciwosci swiatla kierunkowego.
 * Symuluje zrodlo swiatla znajdujace sie bardzo daleko (np. slonce).
 */
struct DirectionalLight {
    /** @brief Kierunek padania promieni swiatla. */
    glm::vec3 direction;
    /** @brief Kolor skladowej otoczenia swiatla. */
    glm::vec3 ambient;
    /** @brief Kolor skladowej rozproszonej swiatla. */
    glm::vec3 diffuse;
    /** @brief Kolor skladowej lustrzanej swiatla. */
    glm::vec3 specular;
    /** @brief Okresla, czy swiatlo jest aktywne. */
    bool enabled;

    /**
     * @brief Konstruktor domyslny.
     * Inicjalizuje swiatlo kierunkowe domyslnymi wartosciami.
     */
    DirectionalLight() :
        direction(0.0f, -1.0f, 0.0f), // Domyślnie świeci w dół
        ambient(0.2f, 0.2f, 0.2f),
        diffuse(0.8f, 0.8f, 0.8f),
        specular(1.0f, 1.0f, 1.0f),
        enabled(true)
    {
    }
};

/**
 * @struct PointLight
 * @brief Struktura definiujaca wlasciwosci swiatla punktowego.
 * Emituje swiatlo we wszystkich kierunkach z jednego punktu.
 */
struct PointLight {
    /** @brief Pozycja zrodla swiatla w przestrzeni swiata. */
    glm::vec3 position;

    /** @brief Staly wspolczynnik tlumienia swiatla. */
    float constant;
    /** @brief Liniowy wspolczynnik tlumienia swiatla. */
    float linear;
    /** @brief Kwadratowy wspolczynnik tlumienia swiatla. */
    float quadratic;

    /** @brief Kolor skladowej otoczenia swiatla. */
    glm::vec3 ambient;
    /** @brief Kolor skladowej rozproszonej swiatla. */
    glm::vec3 diffuse;
    /** @brief Kolor skladowej lustrzanej swiatla. */
    glm::vec3 specular;
    /** @brief Okresla, czy swiatlo jest aktywne. */
    bool enabled;

    /** @brief Okresla, czy swiatlo rzuca cien. */
    bool castsShadow;
    /** @brief Jednostka teksturujaca, do ktorej przypisana jest mapa cieni (cube map). Ustawiana przez system cieni. */
    int shadowMapTextureUnit;
    /** @brief Identyfikator obiektu ShadowMapper odpowiedzialnego za to swiatlo. Ustawiane przez system cieni. */
    int shadowMapperId;
    /** @brief Indeks danych cienia w globalnej tablicy/buforze danych cieni. Ustawiane przez system cieni. */
    int shadowDataIndex;
    /** @brief Odleglosc plaszczyzny bliskiej dla projekcji mapy cienia. */
    float shadowNearPlane;
    /** @brief Odleglosc plaszczyzny dalekiej dla projekcji mapy cienia. */
    float shadowFarPlane;

    /**
     * @brief Konstruktor domyslny.
     * Inicjalizuje swiatlo punktowe domyslnymi wartosciami.
     */
    PointLight() :
        position(0.0f, 0.0f, 0.0f),
        constant(1.0f),
        linear(0.09f),
        quadratic(0.032f),
        ambient(0.05f, 0.05f, 0.05f),
        diffuse(0.8f, 0.8f, 0.8f),
        specular(1.0f, 1.0f, 1.0f),
        enabled(true),
        castsShadow(true),
        shadowMapTextureUnit(-1), // -1 oznacza nieustawione/nieuzywane
        shadowMapperId(-1),
        shadowDataIndex(-1),
        shadowNearPlane(0.1f),
        shadowFarPlane(25.0f)
    {
    }
};

/**
 * @struct SpotLight
 * @brief Struktura definiujaca wlasciwosci reflektora (swiatla stozkowego).
 * Emituje swiatlo w okreslonym kierunku w ksztalcie stozka.
 */
struct SpotLight {
    /** @brief Pozycja zrodla swiatla w przestrzeni swiata. */
    glm::vec3 position;
    /** @brief Kierunek swiecenia reflektora. */
    glm::vec3 direction;

    /** @brief Cosinus kata wewnetrznego stozka swiatla (pelnna intensywnosc). */
    float cutOff;
    /** @brief Cosinus kata zewnetrznego stozka swiatla (stopniowe zanikanie do zera). */
    float outerCutOff;

    /** @brief Staly wspolczynnik tlumienia swiatla. */
    float constant;
    /** @brief Liniowy wspolczynnik tlumienia swiatla. */
    float linear;
    /** @brief Kwadratowy wspolczynnik tlumienia swiatla. */
    float quadratic;

    /** @brief Kolor skladowej otoczenia swiatla. */
    glm::vec3 ambient;
    /** @brief Kolor skladowej rozproszonej swiatla. */
    glm::vec3 diffuse;
    /** @brief Kolor skladowej lustrzanej swiatla. */
    glm::vec3 specular;
    /** @brief Okresla, czy swiatlo jest aktywne. */
    bool enabled;

    /** @brief Okresla, czy swiatlo rzuca cien. */
    bool castsShadow;
    /** @brief Jednostka teksturujaca, do ktorej przypisana jest mapa cieni. Ustawiana przez system cieni. */
    int shadowMapTextureUnit;
    /** @brief Identyfikator obiektu ShadowMapper odpowiedzialnego za to swiatlo. Ustawiane przez system cieni. */
    int shadowMapperId;
    /** @brief Indeks danych cienia w globalnej tablicy/buforze danych cieni. Ustawiane przez system cieni. */
    int shadowDataIndex;


    /**
     * @brief Konstruktor domyslny.
     * Inicjalizuje reflektor domyslnymi wartosciami.
     */
    SpotLight() :
        position(0.0f, 0.0f, 0.0f),
        direction(0.0f, 0.0f, -1.0f), // Domyślnie świeci wzdłuż -Z
        cutOff(glm::cos(glm::radians(12.5f))),
        outerCutOff(glm::cos(glm::radians(17.5f))),
        constant(1.0f),
        linear(0.09f),
        quadratic(0.032f),
        ambient(0.0f, 0.0f, 0.0f),
        diffuse(1.0f, 1.0f, 1.0f),
        specular(1.0f, 1.0f, 1.0f),
        enabled(true),
        castsShadow(true),
        shadowMapTextureUnit(-1), // -1 oznacza nieustawione/nieuzywane
        shadowMapperId(-1),
        shadowDataIndex(-1)
    {
    }
};

#endif // LIGHTING_H
