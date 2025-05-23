#include "ResourceManager.h"
#include "Logger.h" // Dla logowania
#include "Shader.h"   // Wczesniej juz bylo, ale upewniamy sie
#include "Texture.h"  // Wczesniej juz bylo

// STB Image - implementacja powinna byc tylko w jednym pliku .cpp
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
// Usunieto #include "ModelData.h", poniewaz jest juz w ResourceManager.h

ResourceManager& ResourceManager::getInstance() {
    static ResourceManager instance; // Inicjalizacja statyczna, bezpieczna watkowo od C++11
    return instance;
}

bool ResourceManager::initialize() {
    if (m_initialized) {
        Logger::getInstance().warning("ResourceManager jest juz zainicjalizowany.");
        return true;
    }
    Logger::getInstance().info("Inicjalizacja ResourceManager...");

    // Inicjalizacja biblioteki FreeType
    if (FT_Init_FreeType(&m_ftLibrary)) {
        Logger::getInstance().error("ResourceManager: Nie mozna zainicjalizowac biblioteki FreeType.");
        m_freeTypeInitialized = false;
        // Mozna rozwazyc zwrocenie false tutaj, jesli FreeType jest krytyczny
    }
    else {
        Logger::getInstance().info("ResourceManager: Biblioteka FreeType zainicjalizowana pomyslnie.");
        m_freeTypeInitialized = true;
    }

    m_initialized = true;
    Logger::getInstance().info("ResourceManager zainicjalizowany pomyslnie.");
    return true;
}

void ResourceManager::shutdown() {
    if (!m_initialized) {
        Logger::getInstance().warning("ResourceManager nie jest zainicjalizowany lub zostal juz zamkniety.");
        return;
    }
    Logger::getInstance().info("Zamykanie ResourceManager...");
    clearAllResources(); // Zwolni wszystkie zasoby, w tym tekstury i czcionki

    // Czyszczenie biblioteki FreeType
    if (m_freeTypeInitialized) {
        FT_Done_FreeType(m_ftLibrary);
        m_ftLibrary = nullptr; // Dobra praktyka, aby wyzerowac wskaznik po zwolnieniu
        m_freeTypeInitialized = false;
        Logger::getInstance().info("ResourceManager: Zasoby biblioteki FreeType zwolnione.");
    }

    m_initialized = false;
    Logger::getInstance().info("ResourceManager zamkniety.");
}

std::shared_ptr<Shader> ResourceManager::loadShader(const std::string& name, const std::string& vShaderFile, const std::string& fShaderFile) {
    if (!m_initialized) {
        Logger::getInstance().error("ResourceManager: Nie zainicjalizowany. Nie mozna zaladowac shadera: " + name);
        return nullptr;
    }

    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        // Logger::getInstance().info("ResourceManager: Shader '" + name + "' juz zaladowany. Zwracam instancje z cache.");
        return it->second;
    }

    // Logger::getInstance().info("ResourceManager: Ladowanie shadera '" + name + "' z plikow: " + vShaderFile + ", " + fShaderFile);
    try {
        // Tworzenie shadera; jego konstruktor zajmuje sie kompilacja i linkowaniem
        auto shader = std::make_shared<Shader>(name, vShaderFile, fShaderFile);
        if (shader && shader->getID() != 0) { // Sprawdzenie czy shader zostal poprawnie utworzony (ma ID)
            m_shaders[name] = shader;
            // Logger::getInstance().info("ResourceManager: Shader '" + name + "' zaladowany pomyslnie.");
            return shader;
        }
        else {
            // Blad jest logowany w konstruktorze Shader lub przez checkCompileErrors
            Logger::getInstance().error("ResourceManager: Nie udalo sie utworzyc lub zlinkowac shadera '" + name + "'. ID programu shadera moze byc 0.");
            return nullptr;
        }
    }
    catch (const std::exception& e) { // Ogolny wyjatek, jesli konstruktor Shader by rzucal
        Logger::getInstance().error("ResourceManager: Nie udalo sie zaladowac shadera '" + name + "'. Wyjatek: " + std::string(e.what()));
        return nullptr;
    }
    // Dodatkowe catch dla bardziej specyficznych wyjatkow, jesli sa potrzebne
}

std::shared_ptr<Shader> ResourceManager::getShader(const std::string& name) {
    if (!m_initialized) {
        // Logger::getInstance().error("ResourceManager: Nie zainicjalizowany. Nie mozna pobrac shadera: " + name);
        return nullptr;
    }
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        return it->second;
    }
    // Logger::getInstance().warning("ResourceManager: Shader '" + name + "' nie znaleziony w cache.");
    return nullptr;
}

std::shared_ptr<Texture> ResourceManager::loadTexture(const std::string& name, const std::string& filePath, const std::string& typeName, bool flipVertically) {
    if (!m_initialized) {
        Logger::getInstance().error("ResourceManager: Nie zainicjalizowany. Nie mozna zaladowac tekstury: " + name);
        return nullptr;
    }
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        // Logger::getInstance().info("ResourceManager: Tekstura '" + name + "' juz zaladowana. Zwracam instancje z cache.");
        return it->second;
    }

    // Logger::getInstance().info("ResourceManager: Ladowanie tekstury '" + name + "' typu '" + typeName + "' z pliku: " + filePath);

    // Tworzenie obiektu tekstury
    auto texture = std::make_shared<Texture>();
    texture->path = filePath;
    texture->type = typeName; // np. "texture_diffuse"

    // Ustawienie flagi odwracania obrazu dla stb_image
    stbi_set_flip_vertically_on_load(flipVertically);

    // Ladowanie danych obrazu z pliku
    unsigned char* data = stbi_load(filePath.c_str(), &texture->width, &texture->height, &texture->nrChannels, 0);
    if (data) {
        GLenum internalFormat = 0;
        GLenum dataFormat = 0;

        if (texture->nrChannels == 1) {
            internalFormat = GL_RED; // Format wewnetrzny dla tekstury jednokanalowej
            dataFormat = GL_RED;     // Format danych wejsciowych
        }
        else if (texture->nrChannels == 3) {
            internalFormat = GL_RGB; // Mozna uzyc GL_RGB8 dla jawnego rozmiaru
            dataFormat = GL_RGB;
        }
        else if (texture->nrChannels == 4) {
            internalFormat = GL_RGBA; // Mozna uzyc GL_RGBA8 dla jawnego rozmiaru
            dataFormat = GL_RGBA;
        }
        else {
            Logger::getInstance().error("ResourceManager: Nieznana liczba kanalow (" + std::to_string(texture->nrChannels) + ") dla tekstury '" + name + "'. Sciezka: " + filePath);
            stbi_image_free(data); // Wazne: zwolnij pamiec po danych obrazu
            return nullptr;
        }

        glGenTextures(1, &texture->ID);
        glBindTexture(GL_TEXTURE_2D, texture->ID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, texture->width, texture->height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D); // Automatyczne generowanie mipmap

        // Ustawienie parametrow tekstury (zawijanie, filtrowanie)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Filtrowanie trojliniowe
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);               // Filtrowanie dwuliniowe

        stbi_image_free(data); // Zwolnienie danych obrazu po utworzeniu tekstury OpenGL

        m_textures[name] = texture;
        // Logger::getInstance().info("ResourceManager: Tekstura '" + name + "' zaladowana pomyslnie (ID: " + std::to_string(texture->ID) + ", " + std::to_string(texture->width) + "x" + std::to_string(texture->height) + ", Ch: " + std::to_string(texture->nrChannels) + ").");
        return texture;
    }
    else {
        Logger::getInstance().error("ResourceManager: Nie udalo sie zaladowac tekstury '" + name + "'. Sciezka: " + filePath + ". Powod: " + stbi_failure_reason());
        return nullptr;
    }
}

std::shared_ptr<Texture> ResourceManager::getTexture(const std::string& name) {
    if (!m_initialized) {
        // Logger::getInstance().error("ResourceManager: Nie zainicjalizowany. Nie mozna pobrac tekstury: " + name);
        return nullptr;
    }
    auto it = m_textures.find(name);
    if (it != m_textures.end()) {
        return it->second;
    }
    // Logger::getInstance().warning("ResourceManager: Tekstura '" + name + "' nie znaleziona w cache.");
    return nullptr;
}

FT_Face ResourceManager::loadFont(const std::string& name, const std::string& fontFile) {
    if (!m_initialized || !m_freeTypeInitialized) {
        Logger::getInstance().error("ResourceManager lub FreeType nie zainicjalizowany. Nie mozna zaladowac czcionki: " + name);
        return nullptr;
    }

    // Logger::getInstance().info("ResourceManager: Ladowanie czcionki '" + name + "' z pliku: " + fontFile);
    FT_Face face;
    if (FT_New_Face(m_ftLibrary, fontFile.c_str(), 0, &face)) {
        Logger::getInstance().error("ResourceManager: Nie udalo sie zaladowac czcionki '" + name + "' z pliku: " + fontFile);
        return nullptr;
    }

    // Jesli czcionka o tej nazwie juz istnieje, zwolnij stara
    auto it = m_fonts.find(name);
    if (it != m_fonts.end()) {
        FT_Done_Face(it->second);
        // Logger::getInstance().info("ResourceManager: Nadpisywanie wczesniej zaladowanej czcionki: " + name);
    }
    m_fonts[name] = face;
    m_fontPaths[name] = fontFile; // Zapisz sciezke, jesli potrzebne do ponownego ladowania
    // Logger::getInstance().info("ResourceManager: Czcionka '" + name + "' zaladowana pomyslnie.");
    return face;
}

FT_Face ResourceManager::getFont(const std::string& name) {
    if (!m_initialized) {
        // Logger::getInstance().error("ResourceManager: Nie zainicjalizowany. Nie mozna pobrac czcionki: " + name);
        return nullptr;
    }
    auto it = m_fonts.find(name);
    if (it != m_fonts.end()) {
        return it->second;
    }
    // Logger::getInstance().warning("ResourceManager: Czcionka '" + name + "' nie znaleziona.");
    return nullptr;
}

FT_Library ResourceManager::getFreeTypeLibrary() {
    if (!m_freeTypeInitialized) {
        // Logger::getInstance().warning("ResourceManager: Proba pobrania biblioteki FreeType, ktora nie jest zainicjalizowana.");
    }
    return m_ftLibrary;
}

std::shared_ptr<ModelAsset> ResourceManager::loadModel(const std::string& name, const std::string& filePath) {
    if (!m_initialized) {
        Logger::getInstance().error("ResourceManager: Nie zainicjalizowany. Nie mozna zaladowac modelu: " + name);
        return nullptr;
    }
    auto it = m_models.find(name);
    if (it != m_models.end()) {
        // Logger::getInstance().info("ResourceManager: Model '" + name + "' juz zaladowany. Zwracam instancje z cache.");
        return it->second;
    }

    // Logger::getInstance().info("ResourceManager: Ladowanie modelu '" + name + "' z pliku: " + filePath);
    Assimp::Importer importer;
    // Flagi przetwarzania dla Assimp
    const unsigned int assimpFlags = aiProcess_Triangulate           // Zawsze trianguluj siatki
        | aiProcess_FlipUVs               // Odwroc wspolrzedne UV wertykalnie (czesto potrzebne dla OpenGL)
        | aiProcess_GenSmoothNormals      // Generuj gladkie normale, jesli ich nie ma (GenNormals generuje plaskie)
        | aiProcess_CalcTangentSpace;     // Oblicz przestrzen stycznych (dla normal mappingu)
    // | aiProcess_JoinIdenticalVertices // Optymalizacja, laczy identyczne wierzcholki
    // | aiProcess_OptimizeMeshes        // Probuje zredukowac liczbe siatek
    // | aiProcess_OptimizeGraph         // Probuje zoptymalizowac graf sceny

    const aiScene* scene = importer.ReadFile(filePath, assimpFlags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        Logger::getInstance().error("ResourceManager: Blad Assimp podczas ladowania modelu '" + name + "'. Sciezka: " + filePath + ". Blad: " + importer.GetErrorString());
        return nullptr;
    }

    auto modelAsset = std::make_shared<ModelAsset>();
    modelAsset->directory = filePath.substr(0, filePath.find_last_of('/')); // Pobranie katalogu z pelnej sciezki

    processNode(scene->mRootNode, scene, modelAsset); // Rozpocznij przetwarzanie od wezla glownego

    if (modelAsset->meshes.empty()) {
        Logger::getInstance().warning("ResourceManager: Model '" + name + "' zaladowany pomyslnie, ale nie zawiera siatek. Sciezka: " + filePath);
    }
    else {
        // Logger::getInstance().info("ResourceManager: Model '" + name + "' zaladowany pomyslnie z " + std::to_string(modelAsset->meshes.size()) + " siatkami.");
    }
    m_models[name] = modelAsset;
    return modelAsset;
}

std::shared_ptr<ModelAsset> ResourceManager::getModel(const std::string& name) {
    if (!m_initialized) {
        // Logger::getInstance().error("ResourceManager: Nie zainicjalizowany. Nie mozna pobrac modelu: " + name);
        return nullptr;
    }
    auto it = m_models.find(name);
    if (it != m_models.end()) {
        return it->second;
    }
    return nullptr;
}

void ResourceManager::processNode(aiNode* node, const aiScene* scene, std::shared_ptr<ModelAsset>& modelAsset) {
    // Przetworz wszystkie siatki w biezacym wezle
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]]; // Pobranie siatki ze sceny na podstawie indeksu
        if (mesh) { // Dodatkowe sprawdzenie, chociaz rzadko potrzebne
            modelAsset->meshes.push_back(processMesh(mesh, scene, modelAsset));
        }
    }
    // Rekurencyjnie przetworz wszystkie dzieci biezacego wezla
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        if (node->mChildren[i]) { // Dodatkowe sprawdzenie
            processNode(node->mChildren[i], scene, modelAsset);
        }
    }
}

MeshData ResourceManager::processMesh(aiMesh* mesh, const aiScene* scene, std::shared_ptr<ModelAsset>& modelAsset) {
    MeshData meshData; // Obiekt do przechowywania danych przetwarzanej siatki

    // Przetwarzanie wierzcholkow
    meshData.vertices.reserve(mesh->mNumVertices); // Prealokacja pamieci dla wektora wierzcholkow
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex vertex;
        vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

        if (mesh->HasNormals()) {
            vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        }
        else {
            vertex.normal = glm::vec3(0.0f, 0.0f, 0.0f); // Lub wygeneruj/ostrzez
        }

        if (mesh->mTextureCoords[0]) { // Assimp moze miec do 8 zestawow wspolrzednych tekstur
            vertex.texCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            // Opcjonalnie: wczytaj tangenty i bitangenty, jesli sa i potrzebne
            // if (mesh->HasTangentsAndBitangents()) {
            //     vertex.Tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
            //     vertex.Bitangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
            // }
        }
        else {
            vertex.texCoords = glm::vec2(0.0f, 0.0f);
        }
        meshData.vertices.push_back(vertex);
    }

    // Przetwarzanie indeksow
    // Prealokacja: kazda sciana to zwykle trojkat (po aiProcess_Triangulate), wiec 3 indeksy na sciane
    meshData.indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) { // Przejdz przez wszystkie indeksy na scianie
            meshData.indices.push_back(face.mIndices[j]);
        }
    }

    // Przetwarzanie materialow
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        // Wczytywanie tekstur dla materialu
        std::vector<std::shared_ptr<Texture>> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", modelAsset);
        if (!diffuseMaps.empty()) {
            meshData.material.diffuseTexture = diffuseMaps[0]; // Uzyj pierwszej tekstury diffuse
        }

        std::vector<std::shared_ptr<Texture>> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular", modelAsset);
        if (!specularMaps.empty()) {
            meshData.material.specularTexture = specularMaps[0]; // Uzyj pierwszej tekstury specular
        }

        // Wczytywanie kolorow materialu i innych wlasciwosci
        aiColor3D color(0.f, 0.f, 0.f);
        if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
            meshData.material.diffuse = glm::vec3(color.r, color.g, color.b);
        if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
            meshData.material.ambient = glm::vec3(color.r, color.g, color.b);
        if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
            meshData.material.specular = glm::vec3(color.r, color.g, color.b);

        float shininess = 32.0f; // Domyslna wartosc
        if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
            if (shininess == 0.0f) shininess = 1.0f; // Unikaj polysku 0, ktory moze powodowac problemy w niektorych shaderach
            meshData.material.shininess = shininess;
        }
    }
    return meshData;
}

std::vector<std::shared_ptr<Texture>> ResourceManager::loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName, std::shared_ptr<ModelAsset>& modelAsset) {
    std::vector<std::shared_ptr<Texture>> textures;
    unsigned int textureCount = mat->GetTextureCount(type);
    textures.reserve(textureCount);

    for (unsigned int i = 0; i < textureCount; ++i) {
        aiString str;
        if (mat->GetTexture(type, i, &str) != AI_SUCCESS) { // Sprawdzenie bledu GetTexture
            Logger::getInstance().warning("ResourceManager: Nie udalo sie pobrac sciezki tekstury typu " + typeName + " z materialu.");
            continue;
        }
        std::string relativeTexturePath = str.C_Str();

        // Sprawdzenie, czy tekstura nie zostala juz zaladowana dla tego modelu (w ramach tego wywolania loadModel)
        auto cachedTexIt = modelAsset->loadedTexturesCache.find(relativeTexturePath);
        if (cachedTexIt != modelAsset->loadedTexturesCache.end()) {
            textures.push_back(cachedTexIt->second);
        }
        else {
            // Budowanie pelnej sciezki. Zakladamy, ze sciezki sa wzgledne do katalogu modelu.
            // Nalezy obsluzyc przypadki, gdy sciezka jest absolutna lub zawiera '..'
            // Proste polaczenie stringow moze nie byc wystarczajaco odporne.
            std::string fullTexturePath = modelAsset->directory + "/" + relativeTexturePath;
            // Normalizacja sciezki (np. zamiana '\' na '/', obsluga '../') moze byc potrzebna.

            // Uzyj unikalnej nazwy dla globalnego cache ResourceManager, np. pelnej sciezki.
            // Jesli rozne modele moga miec tekstury o tej samej wzglednej sciezce ale w roznych katalogach,
            // globalny klucz musi byc unikalny, np. przez dodanie sciezki modelu.
            std::string globalTextureStoreName = fullTexturePath;

            std::shared_ptr<Texture> newTexture = loadTexture(globalTextureStoreName, fullTexturePath, typeName);
            if (newTexture) {
                textures.push_back(newTexture);
                modelAsset->loadedTexturesCache[relativeTexturePath] = newTexture; // Dodaj do lokalnego cache modelu
            }
            else {
                Logger::getInstance().warning("ResourceManager: Nie udalo sie zaladowac tekstury materialu: " + fullTexturePath);
            }
        }
    }
    return textures;
}

void ResourceManager::clearAllResources() {
    Logger::getInstance().info("ResourceManager: Czyszczenie wszystkich zaladowanych zasobow...");
    clearShaders();
    clearTextures();
    clearModels();
    clearFonts();
}

void ResourceManager::clearShaders() {
    m_shaders.clear(); // shared_ptr automatycznie zarzadza pamiecia Shaderow
    Logger::getInstance().info("ResourceManager: Wszystkie shadery wyczyszczone.");
}

void ResourceManager::clearTextures() {
    // Zakladamy, ze struktura Texture nie ma wlasnego destruktora zwalniajacego ID OpenGL.
    // Jesli ma, to ponizsze glDeleteTextures jest bledem (podwojne zwolnienie).
    // Jesli Texture jest tylko kontenerem danych, to jest to poprawne.
    for (auto const& [name, texturePtr] : m_textures) {
        if (texturePtr && texturePtr->ID != 0) {
            glDeleteTextures(1, &texturePtr->ID);
            // texturePtr->ID = 0; // Opcjonalnie, dla pewnosci, chociaz obiekt jest usuwany z mapy
        }
    }
    m_textures.clear();
    Logger::getInstance().info("ResourceManager: Wszystkie tekstury wyczyszczone.");
}

void ResourceManager::clearModels() {
    // shared_ptr automatycznie zarzadza pamiecia ModelAsset i jego skladowych (jesli sa rowniez shared_ptr)
    // Tekstury zaladowane przez modele sa zarzadzane przez m_Textures i loadedTexturesCache w ModelAsset.
    // Upewnij sie, ze nie ma cyklicznych zaleznosci shared_ptr.
    m_models.clear();
    Logger::getInstance().info("ResourceManager: Wszystkie modele wyczyszczone.");
}

void ResourceManager::clearFonts() {
    for (auto& pair : m_fonts) {
        if (pair.second) { // pair.second to FT_Face
            FT_Done_Face(pair.second);
        }
    }
    m_fonts.clear();
    m_fontPaths.clear(); // Czyscimy tez mape sciezek
    Logger::getInstance().info("ResourceManager: Wszystkie czcionki wyczyszczone.");
}