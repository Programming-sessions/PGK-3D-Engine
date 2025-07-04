/**
* @file Shader.h
* @brief Definicja klasy Shader.
*
* Plik ten zawiera deklarację klasy Shader, która jest odpowiedzialna
* za kompilację, łączenie i zarządzanie programami cieniującymi (shaderami).
*/
#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <glm/glm.hpp> // Dla typow wektorow i macierzy w metodach setUniform


/**
 * @brief Reprezentuje program shaderow OpenGL.
 * * Odpowiada za wczytywanie kodu zrodlowego shaderow (vertex i fragment),
 * ich kompilacje, linkowanie do programu shaderow oraz aktywacje
 * i ustawianie wartosci uniformow.
 */
class Shader {
public:
    /**
     * @brief Konstruktor. Wczytuje, kompiluje i linkuje shadery.
     * @param name Nazwa identyfikujaca shader (np. do logowania lub zarzadzania).
     * @param vertexPath Sciezka do pliku z kodem zrodlowym vertex shadera.
     * @param fragmentPath Sciezka do pliku z kodem zrodlowym fragment shadera.
     */
    Shader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath);

    /**
     * @brief Destruktor. Usuwa program shaderow z pamieci GPU.
     */
    ~Shader();

    // Regula Piatki: Zapobieganie kopiowaniu, implementacja przenoszenia.
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    /**
     * @brief Konstruktor przenoszacy.
     * @param other Obiekt Shader do przeniesienia.
     */
    Shader(Shader&& other) noexcept;

    /**
     * @brief Operator przypisania przenoszacego.
     * @param other Obiekt Shader do przeniesienia.
     * @return Referencja do tego obiektu.
     */
    Shader& operator=(Shader&& other) noexcept;

    /**
     * @brief Aktywuje ten program shaderow do uzycia w biezacym kontekscie renderowania.
     */
    void use() const;

    /**
     * @brief Ustawia wartosc uniformu typu boolean.
     * @param uniformName Nazwa uniformu w kodzie shadera.
     * @param value Wartosc do ustawienia.
     */
    void setBool(const std::string& uniformName, bool value) const;

    /**
     * @brief Ustawia wartosc uniformu typu integer.
     * @param uniformName Nazwa uniformu w kodzie shadera.
     * @param value Wartosc do ustawienia.
     */
    void setInt(const std::string& uniformName, int value) const;

    /**
     * @brief Ustawia wartosc uniformu typu float.
     * @param uniformName Nazwa uniformu w kodzie shadera.
     * @param value Wartosc do ustawienia.
     */
    void setFloat(const std::string& uniformName, float value) const;

    /**
     * @brief Ustawia wartosc uniformu typu glm::vec3.
     * @param uniformName Nazwa uniformu w kodzie shadera.
     * @param value Wartosc wektora 3-elementowego do ustawienia.
     */
    void setVec3(const std::string& uniformName, const glm::vec3& value) const;

    /**
     * @brief Ustawia wartosc uniformu typu glm::mat4 (macierz 4x4).
     * @param uniformName Nazwa uniformu w kodzie shadera.
     * @param value Wartosc macierzy do ustawienia.
     */
    void setMat4(const std::string& uniformName, const glm::mat4& value) const;

    /**
     * @brief Zwraca nazwe identyfikujaca shader.
     * @return Stala referencja do nazwy shadera.
     */
    const std::string& getName() const;

    /**
     * @brief Zwraca ID programu shaderow OpenGL.
     * @return ID programu lub 0 jesli shader jest nieprawidlowy/niezainicjalizowany.
     */
    unsigned int getID() const;

private:
    unsigned int m_id;        ///< ID programu shaderow OpenGL.
    std::string m_name;       ///< Nazwa shadera.

    /**
     * @brief Wczytuje zawartosc pliku tekstowego do stringa.
     * @param filePath Sciezka do pliku.
     * @return Zawartosc pliku jako string lub pusty string w przypadku bledu.
     */
    std::string readFile(const std::string& filePath) const;

    /**
     * @brief Sprawdza bledy kompilacji shadera lub linkowania programu.
     * Loguje informacje o bledach.
     * @param shaderOrProgram ID shadera lub programu do sprawdzenia.
     * @param type Typ operacji ("VERTEX", "FRAGMENT" lub "PROGRAM").
     * @return true jesli operacja zakonczyla sie sukcesem, false w przeciwnym razie.
     */
    bool checkCompileErrors(unsigned int shaderOrProgram, const std::string& type) const;
};

#endif // SHADER_H