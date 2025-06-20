/**
* @file LightingManager.h
* @brief Definicja klasy LightingManager.
*
* Plik ten zawiera deklarację klasy LightingManager, która zarządza
* wszystkimi źródłami światła na scenie i przekazuje ich dane do shaderów.
*/
#ifndef LIGHTING_MANAGER_H
#define LIGHTING_MANAGER_H

#include "Lighting.h" // Potrzebne dla definicji DirectionalLight, PointLight, SpotLight
#include <vector>
#include <memory> // Potrzebne dla std::shared_ptr
// Usunieto <glm/glm.hpp> - nie jest bezposrednio uzywane w deklaracjach tego pliku naglowkowego
// Usunieto <string> - nie jest bezposrednio uzywane w deklaracjach tego pliku naglowkowego

// Deklaracja wyprzedzajaca dla klasy Shader, aby uniknac pelnego include
class Shader;

/**
 * @class LightingManager
 * @brief Klasa zarzadzajaca wszystkimi rodzajami swiatel w scenie.
 *
 * Odpowiada za przechowywanie, modyfikowanie oraz przekazywanie informacji
 * o swiatlach do shaderow. Umozliwia zarzadzanie swiatlem kierunkowym,
 * swiatlami punktowymi oraz reflektorami.
 */
class LightingManager {
public:
    /**
     * @brief Konstruktor domyslny.
     * Inicjalizuje menedzera swiatel.
     */
    LightingManager();

    /**
     * @brief Destruktor domyslny.
     */
    ~LightingManager() = default;

    // --- Swiatlo Kierunkowe ---

    /**
     * @brief Ustawia parametry swiatla kierunkowego.
     * @param light Obiekt DirectionalLight zawierajacy nowe parametry swiatla.
     */
    void setDirectionalLight(const DirectionalLight& light);

    /**
     * @brief Pobiera referencje do modyfikowalnego obiektu swiatla kierunkowego.
     * @return Referencja do DirectionalLight.
     */
    DirectionalLight& getDirectionalLight();

    /**
     * @brief Pobiera referencje do stalego obiektu swiatla kierunkowego.
     * @return Stala referencja do DirectionalLight.
     */
    const DirectionalLight& getDirectionalLight() const;

    // --- Swiatla Punktowe ---

    /**
     * @brief Dodaje nowe swiatlo punktowe do menedzera.
     * @param light Obiekt PointLight do dodania.
     * @return Indeks dodanego swiatla lub -1 jesli osiagnieto limit.
     * @note Sprawdza, czy nie przekroczono maksymalnej liczby swiatel punktowych.
     */
    int addPointLight(const PointLight& light);

    /**
     * @brief Pobiera wskaznik do modyfikowalnego swiatla punktowego o podanym indeksie.
     * @param index Indeks swiatla punktowego.
     * @return Wskaznik do PointLight lub nullptr jesli indeks jest nieprawidlowy.
     */
    PointLight* getPointLight(size_t index);

    /**
     * @brief Pobiera wskaznik do stalego swiatla punktowego o podanym indeksie.
     * @param index Indeks swiatla punktowego.
     * @return Staly wskaznik do PointLight lub nullptr jesli indeks jest nieprawidlowy.
     */
    const PointLight* getPointLight(size_t index) const;

    /**
     * @brief Pobiera referencje do wektora modyfikowalnych swiatel punktowych.
     * @return Referencja do std::vector<PointLight>.
     */
    std::vector<PointLight>& getPointLights();

    /**
     * @brief Pobiera referencje do wektora stalych swiatel punktowych.
     * @return Stala referencja do std::vector<PointLight>.
     */
    const std::vector<PointLight>& getPointLights() const;

    /**
     * @brief Pobiera aktualna liczbe swiatel punktowych.
     * @return Liczba swiatel punktowych (typu size_t).
     */
    size_t getNumPointLights() const;

    /**
     * @brief Usuwa wszystkie swiatla punktowe z menedzera.
     */
    void clearPointLights();

    // --- Reflektory ---

    /**
     * @brief Dodaje nowy reflektor do menedzera.
     * @param light Obiekt SpotLight do dodania.
     * @return Indeks dodanego reflektora lub -1 jesli osiagnieto limit.
     * @note Sprawdza, czy nie przekroczono maksymalnej liczby reflektorow.
     */
    int addSpotLight(const SpotLight& light);

    /**
     * @brief Pobiera wskaznik do modyfikowalnego reflektora o podanym indeksie.
     * @param index Indeks reflektora.
     * @return Wskaznik do SpotLight lub nullptr jesli indeks jest nieprawidlowy.
     */
    SpotLight* getSpotLight(size_t index);

    /**
     * @brief Pobiera wskaznik do stalego reflektora o podanym indeksie.
     * @param index Indeks reflektora.
     * @return Staly wskaznik do SpotLight lub nullptr jesli indeks jest nieprawidlowy.
     */
    const SpotLight* getSpotLight(size_t index) const;

    /**
     * @brief Pobiera referencje do wektora modyfikowalnych reflektorow.
     * @return Referencja do std::vector<SpotLight>.
     */
    std::vector<SpotLight>& getSpotLights();

    /**
     * @brief Pobiera referencje do wektora stalych reflektorow.
     * @return Stala referencja do std::vector<SpotLight>.
     */
    const std::vector<SpotLight>& getSpotLights() const;

    /**
     * @brief Pobiera aktualna liczbe reflektorow.
     * @return Liczba reflektorow (typu size_t).
     */
    size_t getNumSpotLights() const;

    /**
     * @brief Usuwa wszystkie reflektory z menedzera.
     */
    void clearSpotLights();

    /**
     * @brief Wysyla ogolne dane oswietlenia do podanego shadera.
     * Metoda ta odpowiada za ustawienie uniformow zwiazanych z parametrami
     * swiatla kierunkowego, swiatel punktowych i reflektorow.
     * Nie ustawia danych specyficznych dla cieni (np. macierzy przestrzeni swiatla).
     * @param shader Wskaznik (shared_ptr) do obiektu Shader, do ktorego maja byc wyslane dane.
     * @note Shader powinien byc juz aktywny (uzyty) przed wywolaniem tej metody.
     */
    void uploadGeneralLightUniforms(std::shared_ptr<Shader> shader) const;

private:
    DirectionalLight m_directionalLight;          ///< Pojedyncze swiatlo kierunkowe w scenie.
    std::vector<PointLight> m_pointLights;        ///< Wektor przechowujacy swiatla punktowe.
    std::vector<SpotLight> m_spotLights;          ///< Wektor przechowujacy reflektory.

    // Stale okreslajace maksymalna liczbe swiatel, zgodne z oczekiwaniami shaderow.
    // Te stale powinny byc zsynchronizowane z definicjami w shaderach.
    // Zostaly przeniesione z pliku .cpp dla lepszej widocznosci i ewentualnej konfiguracji.
    // Jesli sa one rowniez zdefiniowane w Lighting.h (np. MAX_POINT_LIGHTS),
    // nalezy rozwazyc uzycie tych z Lighting.h lub zapewnic spojnosc.
    // Zakladajac, ze MAX_POINT_LIGHTS i MAX_SPOT_LIGHTS_TOTAL sa zdefiniowane
    // globalnie lub w Lighting.h, nie ma potrzeby ich tutaj redefiniowac.
    // Jesli nie, mozna je tu zdefiniowac jako static constexpr.
    // Przyklad:
    // static constexpr size_t MAX_POINT_LIGHTS_SHADER = 16; // Maksymalna liczba swiatel punktowych obslugiwana przez shader
    // static constexpr size_t MAX_SPOT_LIGHTS_SHADER = 8;   // Maksymalna liczba reflektorow obslugiwana przez shader
};

#endif // LIGHTING_MANAGER_H
