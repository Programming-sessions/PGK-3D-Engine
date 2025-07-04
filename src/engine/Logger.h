/**
* @file Logger.h
* @brief Definicja klasy Logger.
*
* Plik ten zawiera deklarację klasy Logger, która zapewnia
* funkcjonalność logowania komunikatów o różnym poziomie ważności.
*/
#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream> // Do obslugi strumienia plikowego.
#include <mutex>   // Do zapewnienia bezpieczenstwa watkowego.

/**
 * @enum LogLevel
 * @brief Poziomy logowania sluza do kategoryzacji komunikatow logu.
 * Umozliwiaja filtrowanie i rozroznianie wagi poszczegolnych informacji.
 */
enum class LogLevel {
    DEBUG,   ///< Komunikaty diagnostyczne, przydatne podczas tworzenia i testowania.
    INFO,    ///< Komunikaty informacyjne o ogolnym przebiegu dzialania aplikacji.
    WARNING, ///< Ostrzezenia o potencjalnych problemach lub nietypowych sytuacjach.
    ERR,     ///< Bledy, ktore wystapily, ale niekoniecznie przerywaja dzialanie aplikacji. (ERR zamiast ERROR dla zwiezlosci)
    FATAL    ///< Bledy krytyczne, ktore prawdopodobnie uniemozliwiaja dalsze poprawne dzialanie.
};

/**
 * @class Logger
 * @brief System zarzadzania logami (Singleton).
 *
 * Logger zapewnia nastepujace funkcjonalnosci:
 * - Logowanie do pliku.
 * - Opcjonalne wypisywanie komunikatow na konsole z kolorowaniem.
 * - Kategoryzacje komunikatow wedlug poziomu waznosci.
 * - Automatyczne dodawanie znacznikow czasu.
 * - Operacje bezpieczne watkowo dzieki uzyciu mutexu.
 */
class Logger {
private:
    std::ofstream m_logFile;         ///< Strumien pliku logu.
    bool m_consoleOutput;            ///< Flaga wlaczajaca/wylaczajaca wypisywanie na konsole.
    std::string m_logFilePath;       ///< Sciezka do pliku logu.
    std::mutex m_logMutex;           ///< Mutex zapewniajacy bezpieczenstwo operacji logowania w srodowisku wielowatkowym.

    // Instancja Singletona.
    static Logger* s_instance;      ///< Wskaznik na jedyna instancje klasy Logger.

    /**
     * @brief Prywatny konstruktor dla wzorca Singleton.
     * Inicjalizuje domyslne wartosci.
     */
    Logger();

public:
    /**
     * @brief Destruktor.
     * Zamyka plik logu, jesli byl otwarty.
     */
    ~Logger();

    // Usuniecie operacji kopiowania i przenoszenia, aby zapewnic unikalnosc instancji Singletona.
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    /**
     * @brief Zwraca referencje do jedynej instancji Loggera (Singleton).
     * Jesli instancja nie istnieje, tworzy ja.
     * @return Referencja do instancji Loggera.
     */
    static Logger& getInstance();

    /**
     * @brief Inicjalizuje system logowania.
     * Otwiera plik logu i ustawia opcje wypisywania na konsole.
     * @param filePath Sciezka do pliku, w ktorym beda zapisywane logi.
     * @param outputToConsole Czy logi maja byc rowniez wypisywane na konsole (domyslnie true).
     * @return True, jesli inicjalizacja przebiegla pomyslnie, false w przeciwnym razie.
     */
    bool init(const std::string& filePath, bool outputToConsole = true);

    /**
     * @brief Loguje komunikat z okreslonym poziomem waznosci.
     * Jest to glowna metoda logujaca, wywolywana przez metody pomocnicze (debug, info, etc.).
     * @param level Poziom waznosci komunikatu (DEBUG, INFO, WARNING, ERR, FATAL).
     * @param message Tresc komunikatu do zalogowania.
     */
    void log(LogLevel level, const std::string& message);

    /**
     * @brief Loguje komunikat na poziomie DEBUG.
     * @param message Tresc komunikatu diagnostycznego.
     */
    void debug(const std::string& message);

    /**
     * @brief Loguje komunikat na poziomie INFO.
     * @param message Tresc komunikatu informacyjnego.
     */
    void info(const std::string& message);

    /**
     * @brief Loguje komunikat na poziomie WARNING.
     * @param message Tresc ostrzezenia.
     */
    void warning(const std::string& message);

    /**
     * @brief Loguje komunikat na poziomie ERR (Error).
     * @param message Tresc komunikatu o bledzie.
     */
    void error(const std::string& message); // Nazwa metody spójna z 'error' zamiast 'err' dla lepszej czytelności, poziom to LogLevel::ERR

    /**
     * @brief Loguje komunikat na poziomie FATAL.
     * @param message Tresc komunikatu o bledzie krytycznym.
     */
    void fatal(const std::string& message);

    /**
     * @brief Zamyka plik logu.
     * Wazne, aby wywolac te metode przed zakonczeniem programu, aby upewnic sie,
     * ze wszystkie dane zostaly zapisane do pliku.
     */
    void close();
};

#endif // LOGGER_H