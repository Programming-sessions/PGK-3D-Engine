#include "TextRenderer.h" // Powinien byc pierwszy, jesli to odpowiadajacy naglowek
#include <glad/glad.h>
#include "ResourceManager.h" // Zakladamy, ze zarzadza inicjalizacja FT_Library i ladowaniem/zarzadzaniem FT_Face
#include "Logger.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Statyczna instancja dla wzorca Singleton
TextRenderer* TextRenderer::instance = nullptr;

// Stale definiujace uklad danych wierzcholkow
const unsigned int VERTICES_PER_QUAD = 6;
const unsigned int COMPONENTS_PER_VERTEX = 4; // Kazdy wierzcholek: vec2 pozycja, vec2 wspolrzedne tekstury

// Zakres znakow ASCII do wstepnego zaladowania
const unsigned int PRELOADED_GLYPH_COUNT = 128;

TextRenderer* TextRenderer::getInstance() {
    if (instance == nullptr) {
        instance = new TextRenderer();
    }
    return instance;
}

TextRenderer::~TextRenderer() {
    cleanup(); // Upewnij sie, ze wszystkie zasoby sa zwolnione
}

bool TextRenderer::initialize(GLFWwindow* windowPtr, const std::string& fontPath, const std::string& fontNameInManager, unsigned int fontSize, int winWidth, int winHeight) {
    if (initialized) {
        Logger::getInstance().warning("TextRenderer juz zainicjalizowany. Pomijanie reinicjalizacji.");
        return true;
    }
    Logger::getInstance().info("Inicjalizacja TextRenderer...");

    this->window = windowPtr;
    this->windowWidth = winWidth;
    this->windowHeight = winHeight;
    this->currentFontName = fontNameInManager;

    // Zaladuj czcionke przez ResourceManager
    this->face = ResourceManager::getInstance().loadFont(fontNameInManager, fontPath);
    if (!this->face) {
        Logger::getInstance().error("TextRenderer: Nie udalo sie zaladowac czcionki '" + fontNameInManager + "' ze sciezki: " + fontPath);
        return false;
    }
    Logger::getInstance().info("TextRenderer: Czcionka '" + fontNameInManager + "' zaladowana przez ResourceManager.");

    // Ustaw rozmiar czcionki
    if (FT_Set_Pixel_Sizes(this->face, 0, fontSize)) {
        Logger::getInstance().error("TextRenderer: Nie udalo sie ustawic rozmiaru pikseli dla czcionki '" + fontNameInManager + "'.");
        return false;
    }
    Logger::getInstance().info("TextRenderer: Rozmiar pikseli ustawiony dla czcionki '" + fontNameInManager + "'.");

    // Inicjalizuj shadery
    Logger::getInstance().info("TextRenderer: Inicjalizacja shaderow...");
    if (!initializeShaders()) {
        Logger::getInstance().error("TextRenderer: Nie udalo sie zainicjalizowac shaderow tekstu.");
        return false;
    }

    // Konfiguruj VAO i VBO
    Logger::getInstance().info("TextRenderer: Konfiguracja VAO/VBO...");
    glGenVertexArrays(1, &this->VAO);
    glGenBuffers(1, &this->VBO);

    glBindVertexArray(this->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
    // Alokuj pamiec dla VBO (dynamiczny rysunek, zawartosc zmienia sie na znak/klatke)
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * VERTICES_PER_QUAD * COMPONENTS_PER_VERTEX, NULL, GL_DYNAMIC_DRAW);

    // Ustaw wskazniki atrybutow wierzcholkow
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, COMPONENTS_PER_VERTEX, GL_FLOAT, GL_FALSE, COMPONENTS_PER_VERTEX * sizeof(float), (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, 0); // Odwiaz VBO
    glBindVertexArray(0);             // Odwiaz VAO

    // Zaladuj glify
    Logger::getInstance().info("TextRenderer: Ladowanie glifow...");
    if (!loadGlyphs()) {
        Logger::getInstance().error("TextRenderer: Nie udalo sie zaladowac glifow.");
        cleanup(); // Krytyczny blad: wyczysc shadery, VAO, VBO zaalokowane do tej pory
        return false;
    }

    this->initialized = true;
    Logger::getInstance().info("TextRenderer zainicjalizowany pomyslnie.");
    return true;
}

void TextRenderer::renderText(const std::string& text, float x, float y, float scale, glm::vec3 color) {
    if (!initialized) {
        Logger::getInstance().warning("TextRenderer::renderText wywolane, ale renderer nie jest zainicjalizowany.");
        return;
    }
    if (!shaderProgram) {
        Logger::getInstance().error("TextRenderer::renderText: Program shaderow jest nieprawidlowy.");
        return;
    }
    if (Characters.empty()) {
        Logger::getInstance().warning("TextRenderer::renderText wywolane, ale nie zaladowano znakow dla biezacej czcionki.");
        return;
    }

    glUseProgram(this->shaderProgram); // Aktywuj program shaderow

    // Ustaw macierz projekcji
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->windowWidth), 0.0f, static_cast<float>(this->windowHeight));
    glUniformMatrix4fv(this->locProjection, 1, GL_FALSE, glm::value_ptr(projection));

    // Ustaw kolor tekstu
    glUniform3f(this->locTextColor, color.x, color.y, color.z);

    // Aktywuj jednostke teksturujaca i ustaw sampler
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(this->locTextSampler, 0);

    glBindVertexArray(this->VAO); // Powiaz VAO

    float currentX = x; // Pozycja X kursora

    // Iteruj po wszystkich znakach w tekscie
    for (char c : text) {
        auto it = Characters.find(c);
        if (it == Characters.end()) {
            // Znak nie znaleziony we wstepnie zaladowanych glifach
            Logger::getInstance().warning("TextRenderer: Znak '" + std::string(1, c) + "' nie znaleziony w zaladowanych glifach. Pomijanie.");
            continue;
        }
        Character ch = it->second;

        // Oblicz pozycje i wymiary czworokata dla biezacego znaku
        float xpos = currentX + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        // Dane wierzcholkow dla czworokata znaku
        float vertices[VERTICES_PER_QUAD][COMPONENTS_PER_VERTEX] = {
            { xpos,     ypos + h,   0.0f, 0.0f }, // Gorny-lewy
            { xpos,     ypos,       0.0f, 1.0f }, // Dolny-lewy
            { xpos + w, ypos,       1.0f, 1.0f }, // Dolny-prawy

            { xpos,     ypos + h,   0.0f, 0.0f }, // Gorny-lewy
            { xpos + w, ypos,       1.0f, 1.0f }, // Dolny-prawy
            { xpos + w, ypos + h,   1.0f, 0.0f }  // Gorny-prawy
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID); // Powiaz teksture glifu znaku

        // Zaktualizuj zawartosc VBO
        glBindBuffer(GL_ARRAY_BUFFER, this->VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        glDrawArrays(GL_TRIANGLES, 0, VERTICES_PER_QUAD); // Renderuj czworokat

        // Przesun kursor do nastepnego znaku
        currentX += (ch.Advance >> 6) * scale; // Przesuniecie bitowe o 6 to to samo co dzielenie przez 64
    }
    glBindVertexArray(0);       // Odwiaz VAO
    glBindTexture(GL_TEXTURE_2D, 0); // Odwiaz teksture
    glUseProgram(0); // Odwiaz program shaderow (dobra praktyka)
}

void TextRenderer::updateProjectionMatrix(int newWindowWidth, int newWindowHeight) {
    this->windowWidth = newWindowWidth;
    this->windowHeight = newWindowHeight;
    Logger::getInstance().debug("TextRenderer wymiary okna zaktualizowane: " + std::to_string(windowWidth) + "x" + std::to_string(windowHeight));
}

void TextRenderer::cleanup() {
    if (!initialized && VAO == 0 && VBO == 0 && shaderProgram == 0 && Characters.empty()) {
        return; // Unikaj logowania, jesli juz wyczyszczono lub nie zainicjalizowano
    }
    Logger::getInstance().info("Czyszczenie TextRenderer...");

    // Usun tekstury OpenGL dla wszystkich zaladowanych glifow
    for (auto& pair : Characters) {
        if (pair.second.TextureID != 0) {
            glDeleteTextures(1, &pair.second.TextureID);
        }
    }
    Characters.clear();

    // Usun obiekty OpenGL
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO != 0) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (shaderProgram != 0) {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }

    // FT_Face jest zarzadzany przez ResourceManager
    this->face = nullptr;

    // Zresetuj zbuforowane lokalizacje uniformow
    this->locProjection = -1;
    this->locTextColor = -1;
    this->locTextSampler = -1;

    this->initialized = false;
    Logger::getInstance().info("TextRenderer wyczyszczony pomyslnie.");
}

bool TextRenderer::initializeShaders() {
    // Kod zrodlowy Vertex Shadera
    const char* vertexSource = R"glsl(
        #version 330 core
        layout (location = 0) in vec4 vertex; 
        out vec2 TexCoords;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * vec4(vertex.xy, 0.0, 1.0); 
            TexCoords = vertex.zw;
        }
    )glsl";

    // Kod zrodlowy Fragment Shadera
    const char* fragmentSource = R"glsl(
        #version 330 core
        in vec2 TexCoords;
        out vec4 FragColor; 
        uniform sampler2D text;     
        uniform vec3 textColor;  
        void main() {
            vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
            FragColor = vec4(textColor, 1.0) * sampled; 
        }
    )glsl";

    GLuint vertexShader = 0, fragmentShader = 0;
    GLint success;
    char infoLog[512];

    // Kompiluj Vertex Shader
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        Logger::getInstance().error("TextRenderer: Blad kompilacji Vertex Shadera: " + std::string(infoLog));
        glDeleteShader(vertexShader);
        return false;
    }

    // Kompiluj Fragment Shader
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        Logger::getInstance().error("TextRenderer: Blad kompilacji Fragment Shadera: " + std::string(infoLog));
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    // Linkuj Program Shaderow
    this->shaderProgram = glCreateProgram();
    glAttachShader(this->shaderProgram, vertexShader);
    glAttachShader(this->shaderProgram, fragmentShader);
    glLinkProgram(this->shaderProgram);
    glGetProgramiv(this->shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(this->shaderProgram, 512, NULL, infoLog);
        Logger::getInstance().error("TextRenderer: Blad linkowania Programu Shaderow: " + std::string(infoLog));
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(this->shaderProgram);
        this->shaderProgram = 0;
        return false;
    }

    // Shadery nie sa juz potrzebne po zlinkowaniu
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Pobierz i zbuforuj lokalizacje uniformow
    this->locProjection = glGetUniformLocation(this->shaderProgram, "projection");
    this->locTextColor = glGetUniformLocation(this->shaderProgram, "textColor");
    this->locTextSampler = glGetUniformLocation(this->shaderProgram, "text");

    // Sprawdz, czy wszystkie lokalizacje uniformow zostaly znalezione
    if (this->locProjection == -1 || this->locTextColor == -1 || this->locTextSampler == -1) {
        Logger::getInstance().error("TextRenderer: Nie udalo sie pobrac jednej lub wiecej lokalizacji uniformow z programu shaderow.");
        if (this->locProjection == -1) Logger::getInstance().error(" - Uniform 'projection' nie znaleziony lub nieaktywny.");
        if (this->locTextColor == -1) Logger::getInstance().error(" - Uniform 'textColor' nie znaleziony lub nieaktywny.");
        if (this->locTextSampler == -1) Logger::getInstance().error(" - Sampler uniform 'text' nie znaleziony lub nieaktywny.");

        glDeleteProgram(this->shaderProgram);
        this->shaderProgram = 0;
        return false;
    }

    Logger::getInstance().info("TextRenderer: Shadery zainicjalizowane i lokalizacje uniformow zbuforowane pomyslnie.");
    return true;
}

bool TextRenderer::loadGlyphs() {
    if (!this->face) {
        Logger::getInstance().error("TextRenderer::loadGlyphs - Nie zaladowano czcionki. Nie mozna zaladowac glifow.");
        return false;
    }

    // Wyczysc poprzednio zaladowane glify i ich tekstury
    for (auto& pair : Characters) {
        if (pair.second.TextureID != 0) {
            glDeleteTextures(1, &pair.second.TextureID);
        }
    }
    Characters.clear();

    // FreeType laduje glify z wyrownaniem 1-bajtowym
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Zaladuj glify dla pierwszych PRELOADED_GLYPH_COUNT znakow (zazwyczaj ASCII)
    for (unsigned char c = 0; c < PRELOADED_GLYPH_COUNT; c++) {
        // Zaladuj glif znaku
        if (FT_Load_Char(this->face, c, FT_LOAD_RENDER)) {
            Logger::getInstance().warning("TextRenderer: Nie udalo sie zaladowac glifu: " + std::string(1, c) + " (kod znaku " + std::to_string(static_cast<int>(c)) + ")");
            continue; // Pomin ten znak w przypadku bledu
        }

        GLuint textureId = 0;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        // Utworz teksture z bitmapy glifu
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED, // Format wewnetrzny: przechowuj jako pojedynczy czerwony kanal
            this->face->glyph->bitmap.width,
            this->face->glyph->bitmap.rows,
            0,
            GL_RED, // Format zrodlowy: dane sa rowniez pojedynczym czerwonym kanalem
            GL_UNSIGNED_BYTE,
            this->face->glyph->bitmap.buffer
        );

        // Ustaw opcje tekstury
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Przechowaj informacje o znaku
        Character character = {
            textureId,
            glm::ivec2(this->face->glyph->bitmap.width, this->face->glyph->bitmap.rows),
            glm::ivec2(this->face->glyph->bitmap_left, this->face->glyph->bitmap_top),
            static_cast<unsigned int>(this->face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }
    glBindTexture(GL_TEXTURE_2D, 0); // Odwiaz ostatnia teksture

    // Przywroc domyslne wyrownanie pikseli
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    Logger::getInstance().info("TextRenderer: Glify zaladowane dla czcionki '" + this->currentFontName + "'.");
    return true;
}

