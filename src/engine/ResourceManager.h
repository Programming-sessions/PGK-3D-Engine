/**
* @file ResourceManager.h
* @brief Definicja klasy ResourceManager.
*
* Plik ten zawiera deklarację klasy ResourceManager, która zarządza
* ładowaniem i zwalnianiem zasobów, takich jak modele, tekstury i shadery.
*/
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <map>
#include <string>
#include <memory> // Dla std::shared_ptr
// #include <stdexcept> // Usuniete, jesli nie jest uzywane bezposrednio (np. do rzucania std::runtime_error)

#include <glad/glad.h> // Potrzebne dla OpenGL API (np. GLuint, glDeleteTextures)
#include "ModelData.h" // Struktury danych dla modeli (ModelAsset, MeshData, Vertex, MaterialData)
#include "Shader.h"    // Pelna definicja klasy Shader
#include "Texture.h"   // Pelna definicja struktury/klasy Texture

// Biblioteki zewnetrzne
#include <ft2build.h> // FreeType
#include FT_FREETYPE_H // FreeType

#include <assimp/Importer.hpp> // Assimp
#include <assimp/scene.h>      // Assimp
#include <assimp/postprocess.h>// Assimp

// Logger nie jest potrzebny w .h, jesli nie ma tu logowania,
// ale moze byc, jesli np. destruktor mialby logowac.
// Zakladamy, ze Logger.h jest includowany w .cpp tam, gdzie jest uzywany.

/**
 * @brief Menedzer zasobow (Singleton) odpowiedzialny za ladowanie,
 * * przechowywanie i udostepnianie roznych typow zasobow gry,
 * * takich jak shadery, tekstury, modele i czcionki.
 * * Zapobiega wielokrotnemu ladowaniu tych samych zasobow.
 */
class ResourceManager {
public:
    /**
     * @brief Zwraca instancje singletonu ResourceManager.
     * @return Referencja do instancji ResourceManager.
     */
    static ResourceManager& getInstance();

    // Usuniecie konstruktora kopiujacego i operatora przypisania
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    /**
     * @brief Inicjalizuje menedzera zasobow.
     * * Powinno byc wywolane raz na poczatku dzialania aplikacji.
     * * Inicjalizuje m.in. biblioteke FreeType.
     * @return true jesli inicjalizacja przebiegla pomyslnie, false w przeciwnym wypadku.
     */
    bool initialize();

    /**
     * @brief Zamyka menedzera zasobow i zwalnia wszystkie zaladowane zasoby.
     * * Powinno byc wywolane raz na koncu dzialania aplikacji.
     */
    void shutdown();

    /**
     * @brief Laduje (lub pobiera z cache) program shaderow.
     * @param name Unikalna nazwa identyfikujaca shader.
     * @param vShaderFile Sciezka do pliku vertex shadera.
     * @param fShaderFile Sciezka do pliku fragment shadera.
     * @return Wspoldzielony wskaznik do obiektu Shader lub nullptr w przypadku bledu.
     */
    std::shared_ptr<Shader> loadShader(const std::string& name, const std::string& vShaderFile, const std::string& fShaderFile);

    /**
     * @brief Pobiera wczesniej zaladowany shader.
     * @param name Nazwa shadera.
     * @return Wspoldzielony wskaznik do obiektu Shader lub nullptr, jesli nie znaleziono.
     */
    std::shared_ptr<Shader> getShader(const std::string& name);

    /**
     * @brief Laduje (lub pobiera z cache) teksture 2D.
     * @param name Unikalna nazwa identyfikujaca teksture.
     * @param filePath Sciezka do pliku tekstury.
     * @param typeName Nazwa typu tekstury (np. "texture_diffuse", "texture_specular").
     * @param flipVertically Czy obrazek ma byc odwrocony wertykalnie podczas ladowania (typowe dla OpenGL).
     * @return Wspoldzielony wskaznik do obiektu Texture lub nullptr w przypadku bledu.
     */
    std::shared_ptr<Texture> loadTexture(const std::string& name, const std::string& filePath, const std::string& typeName, bool flipVertically = true);

    /**
     * @brief Pobiera wczesniej zaladowana teksture.
     * @param name Nazwa tekstury.
     * @return Wspoldzielony wskaznik do obiektu Texture lub nullptr, jesli nie znaleziono.
     */
    std::shared_ptr<Texture> getTexture(const std::string& name);

    /**
     * @brief Laduje (lub pobiera z cache) czcionke przy uzyciu FreeType.
     * @param name Unikalna nazwa identyfikujaca czcionke.
     * @param fontFile Sciezka do pliku czcionki (np. .ttf).
     * @return Uchwyt FT_Face do zaladowanej czcionki lub nullptr w przypadku bledu.
     */
    FT_Face loadFont(const std::string& name, const std::string& fontFile);

    /**
     * @brief Pobiera wczesniej zaladowana czcionke.
     * @param name Nazwa czcionki.
     * @return Uchwyt FT_Face do czcionki lub nullptr, jesli nie znaleziono.
     */
    FT_Face getFont(const std::string& name);

    /**
     * @brief Zwraca uchwyt do zainicjalizowanej biblioteki FreeType.
     * @return Uchwyt FT_Library.
     */
    FT_Library getFreeTypeLibrary();

    /**
     * @brief Laduje (lub pobiera z cache) model 3D z pliku.
     * @param name Unikalna nazwa identyfikujaca model.
     * @param filePath Sciezka do pliku modelu (np. .obj, .fbx).
     * @return Wspoldzielony wskaznik do obiektu ModelAsset lub nullptr w przypadku bledu.
     */
    std::shared_ptr<ModelAsset> loadModel(const std::string& name, const std::string& filePath);

    /**
     * @brief Pobiera wczesniej zaladowany model.
     * @param name Nazwa modelu.
     * @return Wspoldzielony wskaznik do obiektu ModelAsset lub nullptr, jesli nie znaleziono.
     */
    std::shared_ptr<ModelAsset> getModel(const std::string& name);

private:
    /**
     * @brief Prywatny konstruktor (Singleton).
     */
    ResourceManager() : m_ftLibrary(nullptr), m_initialized(false), m_freeTypeInitialized(false) {}

    /**
     * @brief Prywatny destruktor (Singleton). Sprzataniem zajmuje sie `shutdown()`.
     */
    ~ResourceManager() = default; // shutdown() jest odpowiedzialny za zwolnienie zasobow

    // Mapy przechowujace zaladowane zasoby
    std::map<std::string, std::shared_ptr<Shader>> m_shaders;
    std::map<std::string, std::shared_ptr<Texture>> m_textures;
    std::map<std::string, std::shared_ptr<ModelAsset>> m_models;
    std::map<std::string, FT_Face> m_fonts;
    std::map<std::string, std::string> m_fontPaths; // Do sledzenia sciezek czcionek, jesli potrzebne

    // Uchwyt do biblioteki FreeType
    FT_Library m_ftLibrary;

    // Flagi stanu
    bool m_initialized;
    bool m_freeTypeInitialized;

    // Prywatne metody pomocnicze do ladowania modeli (Assimp)
    /**
     * @brief Rekurencyjnie przetwarza wezly sceny Assimp.
     * @param node Aktualnie przetwarzany wezel Assimp.
     * @param scene Wskaznik do obiektu sceny Assimp.
     * @param modelAsset Wskaznik do obiektu ModelAsset, do ktorego dodawane sa siatki.
     */
    void processNode(aiNode* node, const aiScene* scene, std::shared_ptr<ModelAsset>& modelAsset);

    /**
     * @brief Przetwarza pojedyncza siatke (aiMesh) ze sceny Assimp.
     * @param mesh Wskaznik do siatki Assimp.
     * @param scene Wskaznik do obiektu sceny Assimp.
     * @param modelAsset Wskaznik do obiektu ModelAsset (uzywany do dostepu do katalogu i cache tekstur).
     * @return Obiekt MeshData zawierajacy dane wierzcholkow, indeksow i materialu.
     */
    MeshData processMesh(aiMesh* mesh, const aiScene* scene, std::shared_ptr<ModelAsset>& modelAsset);

    /**
     * @brief Laduje tekstury materialu dla danej siatki.
     * @param mat Wskaznik do materialu Assimp.
     * @param type Typ tekstury Assimp (np. aiTextureType_DIFFUSE).
     * @param typeName Nazwa typu tekstury (do uzycia w strukturze Texture).
     * @param modelAsset Wskaznik do obiektu ModelAsset (do cache'owania tekstur na poziomie modelu).
     * @return Wektor wspoldzielonych wskaznikow do zaladowanych tekstur.
     */
    std::vector<std::shared_ptr<Texture>> loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName, std::shared_ptr<ModelAsset>& modelAsset);

    // Prywatne metody do czyszczenia zasobow
    void clearAllResources();
    void clearShaders();
    void clearTextures();
    void clearModels();
    void clearFonts();
};

#endif // RESOURCE_MANAGER_H