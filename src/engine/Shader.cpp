#include "Shader.h"
#include "Logger.h" // Dla logowania

#include <fstream>
#include <sstream>
#include <glad/glad.h> // Dla funkcji OpenGL

Shader::Shader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath)
    : m_id(0), m_name(name) { // Inicjalizacja m_id na 0 (nieprawidlowy shader)
    std::string vertexCode = readFile(vertexPath);
    std::string fragmentCode = readFile(fragmentPath);

    if (vertexCode.empty()) {
        Logger::getInstance().error("Shader: Pusty kod zrodlowy vertex shadera dla '" + m_name + "' (sciezka: " + vertexPath + ")");
        return; // m_id pozostaje 0
    }
    if (fragmentCode.empty()) {
        Logger::getInstance().error("Shader: Pusty kod zrodlowy fragment shadera dla '" + m_name + "' (sciezka: " + fragmentPath + ")");
        return; // m_id pozostaje 0
    }

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vShaderCode, nullptr);
    glCompileShader(vertexShader);
    if (!checkCompileErrors(vertexShader, "VERTEX")) {
        glDeleteShader(vertexShader); // Sprzatamy po nieudanym shaderze
        return; // m_id pozostaje 0
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fShaderCode, nullptr);
    glCompileShader(fragmentShader);
    if (!checkCompileErrors(fragmentShader, "FRAGMENT")) {
        glDeleteShader(vertexShader);   // Sprzatamy tez poprzedni, jesli ten sie nie udal
        glDeleteShader(fragmentShader);
        return; // m_id pozostaje 0
    }

    m_id = glCreateProgram();
    if (m_id == 0) { // Bardzo rzadki blad, ale mozliwy
        Logger::getInstance().error("Shader: Nie udalo sie utworzyc programu shaderow (glCreateProgram zwrocil 0) dla '" + m_name + "'.");
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return; // m_id jest juz 0
    }

    glAttachShader(m_id, vertexShader);
    glAttachShader(m_id, fragmentShader);
    glLinkProgram(m_id);

    // Po zlinkowaniu, indywidualne obiekty shaderow nie sa juz potrzebne
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!checkCompileErrors(m_id, "PROGRAM")) {
        glDeleteProgram(m_id); // Sprzatamy program, jesli linkowanie sie nie powiodlo
        m_id = 0;              // Ustawiamy ID na 0, aby zasygnalizowac nieprawidlowy shader
        // Logger::getInstance().error("Shader '" + m_name + "' nie powiodlo sie linkowanie."); // checkCompileErrors juz loguje
        return;
    }

    Logger::getInstance().info("Shader '" + m_name + "' skompilowany i zlinkowany pomyslnie. ID: " + std::to_string(m_id));
}

Shader::~Shader() {
    if (m_id != 0) {
        glDeleteProgram(m_id);
        // Logger::getInstance().info("Shader '" + m_name + "' (ID: " + std::to_string(m_id) + ") zniszczony.");
        m_id = 0; // Dobra praktyka, choc obiekt i tak jest niszczony
    }
}

Shader::Shader(Shader&& other) noexcept
    : m_id(other.m_id), m_name(std::move(other.m_name)) {
    other.m_id = 0; // Zapobiega podwojnemu zwolnieniu zasobu przez destruktor 'other'
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        // Najpierw zwolnij wlasne zasoby, jesli istnieja
        if (m_id != 0) {
            glDeleteProgram(m_id);
        }

        // Przenies dane z 'other'
        m_id = other.m_id;
        m_name = std::move(other.m_name);

        // Wyzeruj zasob w 'other', aby zapobiec podwojnemu zwolnieniu
        other.m_id = 0;
    }
    return *this;
}

void Shader::use() const {
    if (m_id != 0) {
        glUseProgram(m_id);
    }
    else {
        // Logger::getInstance().warning("Proba uzycia nieprawidlowego/niezainicjalizowanego shadera (ID=0), nazwa: " + m_name);
    }
}

// Metody ustawiajace uniformy - pozostaja const, sprawdzaja m_id != 0
void Shader::setBool(const std::string& uniformName, bool value) const {
    if (m_id != 0) {
        // TODO: Rozwazyc cachowanie lokalizacji uniformow dla wydajnosci
        glUniform1i(glGetUniformLocation(m_id, uniformName.c_str()), static_cast<int>(value));
    }
}

void Shader::setInt(const std::string& uniformName, int value) const {
    if (m_id != 0) {
        glUniform1i(glGetUniformLocation(m_id, uniformName.c_str()), value);
    }
}

void Shader::setFloat(const std::string& uniformName, float value) const {
    if (m_id != 0) {
        glUniform1f(glGetUniformLocation(m_id, uniformName.c_str()), value);
    }
}

void Shader::setVec3(const std::string& uniformName, const glm::vec3& value) const {
    if (m_id != 0) {
        glUniform3fv(glGetUniformLocation(m_id, uniformName.c_str()), 1, &value[0]);
    }
}

void Shader::setMat4(const std::string& uniformName, const glm::mat4& value) const {
    if (m_id != 0) {
        glUniformMatrix4fv(glGetUniformLocation(m_id, uniformName.c_str()), 1, GL_FALSE, &value[0][0]);
    }
}

const std::string& Shader::getName() const {
    return m_name;
}

unsigned int Shader::getID() const {
    return m_id;
}

std::string Shader::readFile(const std::string& filePath) const {
    std::ifstream fileStream(filePath, std::ios::in); // Pliki shaderow sa tekstowe
    std::stringstream stringBuffer;

    if (!fileStream.is_open()) {
        Logger::getInstance().error("Shader: Nie mozna otworzyc pliku: " + filePath);
        return "";
    }

    stringBuffer << fileStream.rdbuf();
    fileStream.close();
    return stringBuffer.str();
}

bool Shader::checkCompileErrors(unsigned int shaderOrProgram, const std::string& type) const {
    GLint success = 0;
    GLchar infoLog[1024]; // Bufor na log bledow

    if (type != "PROGRAM") { // Sprawdzanie kompilacji shadera (vertex/fragment)
        glGetShaderiv(shaderOrProgram, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shaderOrProgram, sizeof(infoLog), nullptr, infoLog);
            Logger::getInstance().error("Shader: Blad kompilacji (" + type + ") dla shadera '" + m_name + "':\n" + infoLog);
            return false;
        }
    }
    else { // Sprawdzanie linkowania programu
        glGetProgramiv(shaderOrProgram, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shaderOrProgram, sizeof(infoLog), nullptr, infoLog);
            Logger::getInstance().error("Shader: Blad linkowania programu dla shadera '" + m_name + "':\n" + infoLog);
            return false;
        }
    }
    return true; // Sukces
}