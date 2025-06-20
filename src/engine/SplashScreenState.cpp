#include "SplashScreenState.h" 

#include <glad/glad.h> 
#include <GLFW/glfw3.h> // Dla kodow klawiszy w handleEvents

#include "Engine.h"
#include "InputManager.h"
#include "Renderer.h"         // Dla parametru Renderer w render()
#include "ResourceManager.h"  // Dla dostepu do ResourceManager
#include "Texture.h"          // Dla obiektu Texture
#include "Shader.h"           // Dla obiektu Shader
#include "Logger.h"

#include <algorithm> // Dla std::min/max
#include <vector>    // Dla std::vector do przechowywania danych wierzcholkow

// --- Stale dla efektow przenikania ---
const float FADE_IN_DURATION = 0.5f;  // Czas pojawiania sie
const float FADE_OUT_DURATION = 0.5f; // Czas znikania

SplashScreenState::SplashScreenState(const std::string& texturePath, float displayDuration)
    : m_splashTexturePtr(nullptr),
    m_splashShader(nullptr),
    m_texturePath(texturePath),
    m_textureKey("splashScreenTexture_uniqueKey_" + texturePath), // Klucz bardziej unikalny, jesli mozliwe sa wielokrotne ekrany powitalne
    m_displayDuration(displayDuration),
    m_elapsedTime(0.0f),
    m_isFinished(false),
    m_alpha(0.0f),
    m_quadVAO(0),
    m_quadVBO(0) {
    Logger::getInstance().info("Konstruktor SplashScreenState dla: " + m_texturePath);
}

SplashScreenState::~SplashScreenState() {
    // Zasoby sa czyszczone przez cleanup(), ktora powinna byc wywolana przez GameStateManager
    // lub jawnie, jesli to konieczne. Destruktor zapewnia zwolnienie zasobow renderowania, jesli jeszcze nie zostaly zwolnione.
    cleanupRenderResources();
    Logger::getInstance().info("Destruktor SplashScreenState dla: " + m_texturePath);
}

void SplashScreenState::init() {
	m_engine = Engine::getInstance(); // Pobierz instancje silnika
    Logger::getInstance().info("SplashScreenState: Rozpoczecie Init dla " + m_texturePath);

    if (!m_engine) {
        Logger::getInstance().error("SplashScreenState: Wskaznik Engine jest null w init! Nie mozna kontynuowac.");
        m_isFinished = true; // Oznacz jako zakonczony, aby umozliwic zmiane stanu
        return;
    }

    m_engine->setMouseCapture(false); // Ekrany powitalne zazwyczaj nie potrzebuja przechwytywania myszy

    // Najpierw zaladuj teksture ekranu powitalnego, aby uzyskac jej wymiary do korekcji proporcji obrazu
    try {
        ResourceManager& resManager = m_engine->getResourceManager();
        std::string textureTypeForSplash = "ui_splash"; // Opisowy typ dla tekstury

        // loadTexture zwraca std::shared_ptr<Texture>
        m_splashTexturePtr = resManager.loadTexture(m_textureKey, m_texturePath, textureTypeForSplash);

        if (m_splashTexturePtr && m_splashTexturePtr->ID != 0) {
            Logger::getInstance().info("SplashScreenState: Tekstura '" + m_texturePath + "' zaladowana pomyslnie. Typ: " + m_splashTexturePtr->type);
            // Teraz, gdy tekstura jest zaladowana i jej wymiary sa znane, zainicjalizuj pozostale zasoby renderowania
            initRenderResources();
        }
        else {
            Logger::getInstance().error("SplashScreenState: Nie udalo sie zaladowac tekstury '" + m_texturePath + "' lub ID tekstury to 0. Nie mozna wyswietlic ekranu powitalnego.");
            m_isFinished = true; // Nie mozna kontynuowac bez tekstury
            return;
        }
    }
    catch (const std::exception& e) {
        Logger::getInstance().error("SplashScreenState: Wyjatek podczas dostepu do menedzera zasobow lub ladowania tekstury: " + std::string(e.what()));
        m_isFinished = true; // Krytyczny blad, nie mozna kontynuowac
        return;
    }

    // Zresetuj zmienne stanu
    m_elapsedTime = 0.0f;
    m_isFinished = false;
    m_alpha = 0.0f; // Zacznij w pelni przezroczysty dla efektu pojawiania sie
    Logger::getInstance().info("SplashScreenState: Zakonczono Init dla " + m_texturePath);
}

void SplashScreenState::initRenderResources() {
    // Ta funkcja jest wywolywana po pomyslnym zaladowaniu m_splashTexturePtr
    if (!m_engine || !m_splashTexturePtr || m_splashTexturePtr->ID == 0) {
        Logger::getInstance().error("SplashScreenState: Nie mozna zainicjalizowac zasobow renderowania - silnik lub tekstura niegotowe.");
        return;
    }

    // 1. Oblicz wierzcholki czworokata, aby zachowac proporcje tekstury
    float texWidth = static_cast<float>(m_splashTexturePtr->width);
    float texHeight = static_cast<float>(m_splashTexturePtr->height);

    if (texHeight <= 0.0f || texWidth <= 0.0f) { // Sprawdz poprawnosc wymiarow tekstury
        Logger::getInstance().error("SplashScreenState: Nieprawidlowe wymiary tekstury (" + std::to_string(texWidth) + "x" + std::to_string(texHeight) + "). Nie mozna obliczyc proporcji.");
        m_isFinished = true; // Nie mozna poprawnie renderowac
        return;
    }

    float screenWidth = static_cast<float>(m_engine->getWindowWidth());
    float screenHeight = static_cast<float>(m_engine->getWindowHeight());

    if (screenHeight <= 0.0f || screenWidth <= 0.0f) { // Sprawdz poprawnosc wymiarow ekranu
        Logger::getInstance().error("SplashScreenState: Nieprawidlowe wymiary ekranu (" + std::to_string(screenWidth) + "x" + std::to_string(screenHeight) + "). Nie mozna obliczyc proporcji.");
        m_isFinished = true; // Nie mozna poprawnie renderowac
        return;
    }

    float texAspect = texWidth / texHeight;
    float screenAspect = screenWidth / screenHeight;

    float quadNdcWidth = 1.0f;  // Domyslnie pelna szerokosc ekranu w NDC
    float quadNdcHeight = 1.0f; // Domyslnie pelna wysokosc ekranu w NDC

    if (texAspect > screenAspect) {
        // Tekstura jest szersza niz ekran (wzgledem proporcji) -> dopasuj do szerokosci ekranu, letterbox gora/dol
        quadNdcHeight = screenAspect / texAspect;
    }
    else {
        // Tekstura jest wyzsza niz ekran (wzgledem proporcji) -> dopasuj do wysokosci ekranu, letterbox lewo/prawo
        quadNdcWidth = texAspect / screenAspect;
    }

    // Zdefiniuj wierzcholki dla wysrodkowanego czworokata uzywajac obliczonych wymiarow NDC
    // Format: pozycjaX, pozycjaY, wspTeksturyX, wspTeksturyY
    std::vector<float> vertices = {
        -quadNdcWidth,  quadNdcHeight, 0.0f, 1.0f, // Gorny-lewy
        -quadNdcWidth, -quadNdcHeight, 0.0f, 0.0f, // Dolny-lewy
         quadNdcWidth, -quadNdcHeight, 1.0f, 0.0f, // Dolny-prawy

        -quadNdcWidth,  quadNdcHeight, 0.0f, 1.0f, // Gorny-lewy
         quadNdcWidth, -quadNdcHeight, 1.0f, 0.0f, // Dolny-prawy
         quadNdcWidth,  quadNdcHeight, 1.0f, 1.0f  // Gorny-prawy
    };

    // 2. Zainicjalizuj Shader
    try {
        // Rozwaz uczynienie sciezek do shaderow konfigurowalnymi lub czescia definicji zasobu
        std::string shaderName = "SplashShader_" + m_textureKey; // Unikalna nazwa dla shadera, jesli istnieje wiele ekranow powitalnych
        m_splashShader = std::make_unique<Shader>(shaderName,
            "assets/shaders/splash_shader.vert",
            "assets/shaders/splash_shader.frag");
        Logger::getInstance().info("Shader ekranu powitalnego '" + shaderName + "' utworzony i zaladowany pomyslnie.");
    }
    catch (const std::exception& e) {
        Logger::getInstance().error("SplashScreenState: Nie udalo sie utworzyc/zaladowac shadera ekranu powitalnego: " + std::string(e.what()));
        m_isFinished = true; // Nie mozna renderowac bez shadera
        return;
    }

    // 3. Zainicjalizuj VAO i VBO dla czworokata
    if (m_quadVAO == 0) glGenVertexArrays(1, &m_quadVAO); // Generuj tylko, jesli jeszcze nie utworzono
    if (m_quadVBO == 0) glGenBuffers(1, &m_quadVBO);   // Generuj tylko, jesli jeszcze nie utworzono

    glBindVertexArray(m_quadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Wskazniki atrybutow wierzcholkow:
    // Atrybut pozycji (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    // Atrybut wspolrzednych tekstury (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0); // Odwiaz VBO
    glBindVertexArray(0);             // Odwiaz VAO
    Logger::getInstance().info("VAO/VBO czworokata ekranu powitalnego zainicjalizowane z korekcja proporcji.");
}

void SplashScreenState::cleanup() {
    Logger::getInstance().info("SplashScreenState: Wywolano Cleanup dla " + m_texturePath);
    cleanupRenderResources();
    // m_splashTexturePtr (shared_ptr) bedzie zarzadzany przez ResourceManager lub automatycznie, jesli nie ma innych referencji.
    // Nie ma potrzeby jawnego zwalniania z ResourceManager tutaj, chyba ze taka jest pozadana polityka.
    m_splashTexturePtr.reset();
}

void SplashScreenState::cleanupRenderResources() {
    if (m_quadVAO != 0) {
        glDeleteVertexArrays(1, &m_quadVAO);
        m_quadVAO = 0;
    }
    if (m_quadVBO != 0) {
        glDeleteBuffers(1, &m_quadVBO);
        m_quadVBO = 0;
    }
    // m_splashShader (unique_ptr) jest zwalniany automatycznie, gdy wychodzi poza zakres lub jest resetowany.
    m_splashShader.reset();
    Logger::getInstance().info("SplashScreenState: Zasoby renderowania wyczyszczone dla " + m_texturePath);
}

void SplashScreenState::pause() {
    // Logger::getInstance().info("SplashScreenState: Pauza dla " + m_texturePath);
    // Nic konkretnego do zrobienia przy pauzie dla prostego ekranu powitalnego
}

void SplashScreenState::resume() {
    // Logger::getInstance().info("SplashScreenState: Wznowienie dla " + m_texturePath);
    // Jesli byly jakies animacje lub timery wstrzymane, wznow je tutaj
}

void SplashScreenState::handleEvents(InputManager* inputManager, EventManager* eventManager /* nieuzywany */) {
    if (!inputManager) return;

    // Pozwol na pominiecie ekranu powitalnego za pomoca popularnych klawiszy
    if (inputManager->isKeyPressed(GLFW_KEY_SPACE) ||
        inputManager->isKeyPressed(GLFW_KEY_ENTER) ||
        inputManager->isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {

        if (!m_isFinished) { // Zapobiegaj wielokrotnym logom "pominieto", jesli klawisz jest przytrzymany
            Logger::getInstance().info("SplashScreenState: Pominieto przez uzytkownika (" + m_texturePath + ").");
            m_isFinished = true; // Wyzwol efekt znikania lub natychmiastowe zakonczenie
        }
    }
}

void SplashScreenState::update(float deltaTime) {
    if (!m_engine) {
        m_isFinished = true; // Nie powinno sie zdarzyc, jesli init zakonczyl sie sukcesem
        return;
    }

    if (m_isFinished && m_alpha <= 0.0f) {
        // Jesli juz oznaczono do zakonczenia i calkowicie zniknal, nie rob nic wiecej.
        // GameStateManager powinien obsluzyc zmiane stanu.
        return;
    }

    m_elapsedTime += deltaTime;

    if (m_isFinished) {
        // Jesli oznaczono jako zakonczony (przez czas trwania lub pominiecie), rozpocznij/kontynuuj znikanie
        if (FADE_OUT_DURATION > 0.0f) {
            m_alpha -= (deltaTime / FADE_OUT_DURATION);
        }
        else {
            m_alpha = 0.0f; // Natychmiastowe znikniecie, jesli czas trwania wynosi zero
        }
    }
    else {
        // Normalne dzialanie: pojawianie sie, wyswietlanie, przygotowanie do znikania
        if (m_elapsedTime < FADE_IN_DURATION) {
            // Faza pojawiania sie
            if (FADE_IN_DURATION > 0.0f) {
                m_alpha = m_elapsedTime / FADE_IN_DURATION;
            }
            else {
                m_alpha = 1.0f; // Natychmiastowe pojawienie sie
            }
        }
        else if (m_elapsedTime >= (m_displayDuration - FADE_OUT_DURATION)) {
            // Rozpocznij faze znikania (zanim zakonczy sie calkowity czas wyswietlania)
            if (m_displayDuration > FADE_OUT_DURATION && FADE_OUT_DURATION > 0.0f) {
                m_alpha = (m_displayDuration - m_elapsedTime) / FADE_OUT_DURATION;
            }
            else {
                m_alpha = 0.0f; // Obsluguje przypadki, gdy czas wyswietlania jest zbyt krotki lub znikanie jest natychmiastowe
                m_isFinished = true;
            }
        }
        else {
            // Faza pelnej widocznosci
            m_alpha = 1.0f;
        }

        // Sprawdz, czy osiagnieto calkowity czas wyswietlania
        if (m_elapsedTime >= m_displayDuration) {
            m_isFinished = true;
        }
    }

    // Ogranicz alfa do zakresu [0, 1]
    m_alpha = std::max(0.0f, std::min(1.0f, m_alpha));
}

void SplashScreenState::render(Renderer* renderer /* nieuzywany */) {
    // Upewnij sie, ze wszystkie niezbedne zasoby sa dostepne przed proba renderowania
    if (!m_splashShader || !m_splashTexturePtr || m_splashTexturePtr->ID == 0 || m_quadVAO == 0) {
        // Zaloguj ten problem raz lub kontroluj czestotliwosc, jesli spamuje
        // Logger::getInstance().warning("SplashScreenState: Wywolano Render, ale zasoby nie sa gotowe dla " + m_texturePath);
        return;
    }

    if (m_alpha <= 0.0f && m_isFinished) {
        // Optymalizacja: jesli w pelni przezroczysty i zakonczony, nie zawracaj sobie glowy wywolaniami GL
        return;
    }

    // --- Zapisz i ustaw stan OpenGL dla mieszania alfa 2D ---
    GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLint prevBlendSrcRGB, prevBlendDstRGB, prevBlendSrcAlpha, prevBlendDstAlpha;
    GLint prevBlendEquationRGB, prevBlendEquationAlpha;

    // Dla ekranu powitalnego test glebokosci jest zazwyczaj wylaczony, aby rysowac na wszystkim
    if (depthTestWasEnabled) glDisable(GL_DEPTH_TEST);

    // Wlacz mieszanie, jesli jeszcze nie jest wlaczone
    if (!blendWasEnabled) glEnable(GL_BLEND);

    // Przechowaj poprzednie ustawienia mieszania
    glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &prevBlendEquationRGB);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &prevBlendEquationAlpha);

    // Ustaw standardowe mieszanie alfa
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    // --- Renderuj czworokat ekranu powitalnego ---
    m_splashShader->use();
    m_splashShader->setInt("splashTextureSampler", 0); // Jednostka teksturujaca 0
    m_splashShader->setFloat("alphaValue", m_alpha);   // Przekaz biezaca alfa dla efektu przenikania

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_splashTexturePtr->ID);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6); // 6 wierzcholkow dla 2 trojkatow tworzacych czworokat

    // --- Przywroc stan OpenGL ---
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    if (depthTestWasEnabled) glEnable(GL_DEPTH_TEST);
    if (!blendWasEnabled) {
        glDisable(GL_BLEND);
    }
    else {
        // Przywroc poprzednia funkcje i rownanie mieszania, jesli mieszanie bylo juz wlaczone
        glBlendFuncSeparate(prevBlendSrcRGB, prevBlendDstRGB, prevBlendSrcAlpha, prevBlendDstAlpha);
        glBlendEquationSeparate(prevBlendEquationRGB, prevBlendEquationAlpha);
    }
}
