/**
* @file ModelData.h
* @brief Definicje struktur danych dla modeli 3D.
*
* Plik ten zawiera definicje struktur używanych do przechowywania
* danych o wierzchołkach, teksturach i materiałach modelu.
*/
#ifndef MODELDATA_H
#define MODELDATA_H

#include <vector>
#include <string>
#include <memory> // Dla std::shared_ptr
#include <map>    // Dla std::map

#include "Primitives.h" // Zawiera definicje struktury Vertex
#include "Texture.h"    // Dla std::shared_ptr<Texture>
#include "Lighting.h"   // Zawiera definicje struktury Material

/**
 * @struct MeshData
 * @brief Struktura reprezentujaca pojedyncza siatke (mesh) w modelu 3D.
 * Przechowuje wierzcholki, indeksy oraz material przypisany do tej siatki.
 */
struct MeshData {
    /**
     * @var vertices
     * @brief Wektor wierzcholkow tworzacych siatke.
     */
    std::vector<Vertex> vertices;

    /**
     * @var indices
     * @brief Wektor indeksow definiujacych trojkaty siatki na podstawie wierzcholkow.
     */
    std::vector<unsigned int> indices;

    /**
     * @var material
     * @brief Material przypisany do tej siatki.
     * Kazda siatka moze miec odrebny material definiujacy jej wyglad.
     */
    Material material;

    // Potencjalne rozszerzenie:
    // /**
    //  * @var assimpMaterialIndex
    //  * @brief Indeks materialu w oryginalnym pliku modelu (np. z biblioteki Assimp).
    //  * Moze byc uzyteczny do bardziej zaawansowanego mapowania lub debugowania materialow. 
    //  */
    // unsigned int assimpMaterialIndex; 
};

/**
 * @struct ModelAsset
 * @brief Struktura reprezentujaca caly zasob modelu 3D, skladajacy sie z wielu siatek. 
 * Jest to forma, w jakiej model jest przechowywany np. w ResourceManager.
 */
struct ModelAsset {
    /**
     * @var meshes
     * @brief Wektor siatek (MeshData) skladajacych sie na caly model.
     */
    std::vector<MeshData> meshes;

    /**
     * @var directory
     * @brief Sciezka do katalogu, z ktorego model zostal zaladowany.
     * Przydatne do rozwiazywania wzglednych sciezek do plikow tekstur.
     */
    std::string directory;

    /**
     * @var loadedTexturesCache
     * @brief Podreczna pamiec (cache) dla tekstur juz zaladowanych dla tego modelu.
     * Kluczem mapy jest oryginalna sciezka do pliku tekstury (np. z pliku modelu),
     * a wartoscia jest wskaznik shared_ptr do obiektu tekstury.
     * Pomaga uniknac wielokrotnego ladowania tych samych tekstur.
     */
    std::map<std::string, std::shared_ptr<Texture>> loadedTexturesCache;
};

#endif // MODELDATA_H