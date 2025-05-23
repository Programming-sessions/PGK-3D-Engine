#include "Logger.h" // Pierwszeństwo dla nagłówka klasy implementowanej.

#include <iostream>  // Dla std::cout, std::cerr.
#include <chrono>    // Dla std::chrono::system_clock.
#include <ctime>     // Dla std::time_t, std::tm, ctime_s.
#include <iomanip>   // Dla std::put_time (alternatywa dla ctime_s, bardziej przenośna).
#include <sstream>   // Dla formatowania czasu za pomocą std::put_time.

#ifdef _WIN32
#include <Windows.h> // Dla funkcji konsoli Windows (kolory).
#endif

// Inicjalizacja statycznej instancji Singletona.
Logger* Logger::s_instance = nullptr;

Logger::Logger() : m_consoleOutput(true) {
    // Konstruktor jest prywatny, uzywany tylko przez getInstance().
}

Logger::~Logger() {
    close(); // Automatyczne zamkniecie pliku logu przy niszczeniu obiektu Loggera.
}

Logger& Logger::getInstance() {
    // Prosta implementacja getInstance. Dla srodowisk wielowatkowych, inicjalizacja
    // s_instance moze wymagac dodatkowej synchronizacji (np. std::call_once),
    // chociaz same operacje logowania sa chronione mutexem.
    if (s_instance == nullptr) {
        s_instance = new Logger();
    }
    return *s_instance;
}

bool Logger::init(const std::string& filePath, bool outputToConsole) {
    std::lock_guard<std::mutex> lock(m_logMutex); // Zapewnienie bezpieczenstwa watkowego podczas inicjalizacji.

    // Jesli plik logu byl juz otwarty, zamknij go przed ponowna inicjalizacja.
    if (m_logFile.is_open()) {
        m_logFile.close();
    }

    m_logFilePath = filePath;
    m_consoleOutput = outputToConsole;

    m_logFile.open(m_logFilePath, std::ios::out | std::ios::app); // Otwarcie w trybie dopisywania.
    if (!m_logFile.is_open()) {
        if (m_consoleOutput) {
            // Uzycie std::cerr dla bledow, nawet jesli to tylko blad otwarcia pliku logu.
            std::cerr << "BLAD KRYTYCZNY: Nie udalo sie otworzyc pliku logu: " << m_logFilePath << std::endl;
        }
        return false; // Inicjalizacja nie powiodla sie.
    }

    info("System logowania zainicjalizowany. Plik logu: " + m_logFilePath);
    return true; // Inicjalizacja pomyslna.
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_logMutex); // Blokada mutexu na czas operacji logowania.

    // Pobranie aktualnego czasu.
    auto now = std::chrono::system_clock::now();
    auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);

    // Konwersja czasu na czytelny format. Uzycie std::put_time dla wiekszej kontroli i przenosnosci.
    std::stringstream timeStream;
    std::tm timeinfo;
    // localtime_s jest bezpieczniejsza wersja localtime (dostepna w MSVC i C11).
    // Dla innych kompilatorow mozna uzyc localtime lub odpowiednika.
#ifdef _MSC_VER
    localtime_s(&timeinfo, &nowAsTimeT);
#else
    // Wersja dla kompilatorow nie-MSVC (moze wymagac dodatkowej uwagi dla bezpieczenstwa watkowego,
    // jesli localtime nie zwraca wskaznika do statycznego bufora w sposob bezpieczny watkowo,
    // ale jest to mniej prawdopodobne z std::chrono).
    timeinfo = *std::localtime(&nowAsTimeT);
#endif
    timeStream << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
    std::string timeString = timeStream.str();

    std::string levelString; // Tekstowa reprezentacja poziomu logu.
#ifdef _WIN32
    WORD consoleColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Domyslny bialy (Windows).
#endif

    switch (level) {
    case LogLevel::DEBUG:
        levelString = "DEBUG";
#ifdef _WIN32
        consoleColor = FOREGROUND_BLUE | FOREGROUND_INTENSITY; // Jasnoniebieski.
#endif
        break;
    case LogLevel::INFO:
        levelString = "INFO "; // Dodatkowa spacja dla wyrownania.
#ifdef _WIN32
        consoleColor = FOREGROUND_GREEN | FOREGROUND_INTENSITY; // Jasnozielony.
#endif
        break;
    case LogLevel::WARNING:
        levelString = "WARN "; // Skrocona forma dla wyrownania.
#ifdef _WIN32
        consoleColor = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; // Jasnozolty.
#endif
        break;
    case LogLevel::ERR:
        levelString = "ERROR";
#ifdef _WIN32
        consoleColor = FOREGROUND_RED | FOREGROUND_INTENSITY; // Jasnoczerwony.
#endif
        break;
    case LogLevel::FATAL:
        levelString = "FATAL";
#ifdef _WIN32
        consoleColor = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY; // Jasnofioletowy/Magenta.
#endif
        break;
    default: // Zabezpieczenie przed nieznanym poziomem logu.
        levelString = "UNKWN";
        break;
    }

    // Formatowanie finalnego komunikatu logu.
    std::stringstream logMessageStream;
    logMessageStream << "[" << levelString << "] " << timeString << " - " << message;
    std::string logMessage = logMessageStream.str();

    // Zapis do pliku.
    if (m_logFile.is_open()) {
        m_logFile << logMessage << std::endl; // std::endl zapewnia flush.
        // m_logFile.flush(); // Dodatkowy flush, jesli std::endl nie wystarcza (zalezy od implementacji).
    }

    // Wypisanie na konsole.
    if (m_consoleOutput) {
#ifdef _WIN32
        // Ustawienie koloru konsoli dla Windows.
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO consoleScreenBufferInfo; // Przechowanie informacji o buforze konsoli.
        GetConsoleScreenBufferInfo(hConsole, &consoleScreenBufferInfo);
        WORD defaultConsoleAttributes = consoleScreenBufferInfo.wAttributes; // Zapisanie domyslnych atrybutow.

        SetConsoleTextAttribute(hConsole, consoleColor);
#endif

        // Uzycie std::cerr dla bledow i fatalnych, std::cout dla pozostalych.
        if (level == LogLevel::ERR || level == LogLevel::FATAL) {
            std::cerr << logMessage << std::endl;
        }
        else {
            std::cout << logMessage << std::endl;
        }

#ifdef _WIN32
        // Przywrocenie domyslnego koloru konsoli.
        SetConsoleTextAttribute(hConsole, defaultConsoleAttributes);
#endif
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERR, message);
}

void Logger::fatal(const std::string& message) {
    log(LogLevel::FATAL, message);
}

void Logger::close() {
    std::lock_guard<std::mutex> lock(m_logMutex); // Zapewnienie bezpieczenstwa watkowego.

    if (m_logFile.is_open()) {
        info("Zamykanie systemu logowania."); // Logowanie informacji o zamknieciu.
        m_logFile.close();
    }
}