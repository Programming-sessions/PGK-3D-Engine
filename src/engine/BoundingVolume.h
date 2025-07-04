/**
* @file BoundingVolume.h
* @brief Definicja klasy BoundingVolume.
*
* Plik ten zawiera deklarację klasy BoundingVolume, która jest
* używana do reprezentowania otoczek w systemie detekcji kolizji.
*/
#ifndef BOUNDING_VOLUME_H
#define BOUNDING_VOLUME_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // Dla transformacji, np. w OBB lub przy aktualizacji
#include <vector>                       // Uzywane w AABB::updateFromVertices
#include <limits>                       // Dla std::numeric_limits
#include <algorithm>                    // Dla std::min/max
#include <array>                        // Dla OBB::getCorners

// Deklaracja wyprzedzajaca dla struktury Vertex.
// Pelna definicja (z Primitives.h) bedzie potrzebna w BoundingVolume.cpp
// dla metody AABB::updateFromVertices.
struct Vertex;

/**
 * @enum ColliderType
 * @brief Definiuje ogolny typ zachowania obiektu kolidujacego w systemie fizyki/kolizji.
 */
enum class ColliderType {
    STATIC,    ///< Obiekt nieruchomy, nie reaguje na kolizje, ale inne obiekty moga z nim kolidowac (np. sciana, podloga).
    DYNAMIC,   ///< Obiekt ruchomy, reaguje na kolizje i moze byc przez nie przesuwany (np. gracz, pudelko).
    TRIGGER    ///< Obszar specjalny, ktory nie powoduje fizycznej reakcji kolizyjnej, ale wyzwala zdarzenia przy wejsciu/wyjsciu (np. pulapka, obszar dialogu).
};

/**
 * @enum BoundingShapeType
 * @brief Definiuje konkretny ksztalt geometryczny uzywany do reprezentowania granic obiektu kolidujacego.
 */
enum class BoundingShapeType {
    AABB,       ///< Axis-Aligned Bounding Box (Prostopadloscian osioworownolegly).
    SPHERE,     ///< Sfera (obecnie niezaimplementowana w dostarczonym kodzie, ale typ jest zdefiniowany).
    PLANE,      ///< Nieskonczona plaszczyzna.
    OBB,        ///< Oriented Bounding Box (Prostopadloscian zorientowany).
    CYLINDER,   ///< Walec.
    UNKNOWN     ///< Typ nieznany lub nieokreslony.
};

/**
 * @class BoundingVolume
 * @brief Abstrakcyjna klasa bazowa dla wszystkich ksztaltow kolizyjnych (Bounding Volumes).
 *
 * Definiuje wspolny interfejs, w tym metode do uzyskania konkretnego typu ksztaltu.
 * Kazdy obiekt w scenie, ktory ma uczestniczyc w detekcji kolizji,
 * powinien posiadac obiekt BoundingVolume okreslajacy jego granice.
 */
class BoundingVolume {
public:
    /** @brief Wirtualny destruktor domyslny. */
    virtual ~BoundingVolume() = default;

    /**
     * @brief Czysto wirtualna metoda zwracajaca konkretny typ ksztaltu kolizyjnego.
     * Musi byc zaimplementowana przez wszystkie klasy pochodne.
     * @return Wartosc BoundingShapeType identyfikujaca typ ksztaltu.
     */
    virtual BoundingShapeType getType() const = 0;

    // Potencjalna wirtualna metoda do sprawdzania przeciecia z innym BoundingVolume.
    // W obecnym systemie kolizji, testy sa wykonywane przez CollisionSystem
    // na podstawie typow, wiec ta metoda nie jest tutaj scisle wymagana.
    // virtual bool intersects(const BoundingVolume& other) const = 0;
};

/**
 * @class AABB
 * @brief Reprezentuje Axis-Aligned Bounding Box (prostopadloscian osioworownolegly).
 * Definiowany przez dwa punkty: minimalny (minPoint) i maksymalny (maxPoint).
 * Jego krawedzie sa zawsze rownolegle do osi ukladu wspolrzednych.
 */
class AABB : public BoundingVolume {
public:
    glm::vec3 minPoint; ///< Punkt minimalny AABB (najmniejsze wspolrzedne X, Y, Z).
    glm::vec3 maxPoint; ///< Punkt maksymalny AABB (najwieksze wspolrzedne X, Y, Z).

    /**
     * @brief Konstruktor.
     * Inicjalizuje AABB domyslnie "odwroconymi" wartosciami, co jest przydatne
     * przy obliczaniu AABB na podstawie zbioru punktow (pierwszy punkt ustawi poprawne granice).
     * @param min Minimalny punkt AABB.
     * @param max Maksymalny punkt AABB.
     */
    AABB(const glm::vec3& min = glm::vec3(std::numeric_limits<float>::max()),
        const glm::vec3& max = glm::vec3(std::numeric_limits<float>::lowest()))
        : minPoint(min), maxPoint(max) {
    }

    /**
     * @brief Zwraca typ ksztaltu jako BoundingShapeType::AABB.
     * @return BoundingShapeType::AABB.
     */
    BoundingShapeType getType() const override { return BoundingShapeType::AABB; }

    /**
     * @brief Sprawdza, czy ten AABB przecina sie z innym AABB.
     * @param other Drugi AABB do testu przeciecia.
     * @return True, jesli AABB sie przecinaja, false w przeciwnym wypadku.
     */
    bool intersects(const AABB& other) const {
        // Przeciecie wystepuje, jesli nie ma separacji na zadnej z osi.
        if (maxPoint.x < other.minPoint.x || minPoint.x > other.maxPoint.x) return false;
        if (maxPoint.y < other.minPoint.y || minPoint.y > other.maxPoint.y) return false;
        if (maxPoint.z < other.minPoint.z || minPoint.z > other.maxPoint.z) return false;
        return true; // Brak separacji na wszystkich osiach oznacza przeciecie.
    }

    /**
     * @brief Aktualizuje wymiary AABB na podstawie zbioru lokalnych wierzcholkow obiektu i jego macierzy modelu.
     * Transformuje kazdy lokalny wierzcholek do przestrzeni swiata i rozszerza AABB tak,
     * aby obejmowalo wszystkie przetransformowane wierzcholki.
     * @param localVertices Wektor lokalnych wierzcholkow obiektu (typu Vertex).
     * @param modelMatrix Macierz modelu transformujaca obiekt z przestrzeni lokalnej do swiata.
     */
    void updateFromVertices(const std::vector<struct Vertex>& localVertices, const glm::mat4& modelMatrix);

    /**
     * @brief Resetuje AABB do stanu poczatkowego (nieskonczenie maly, "odwrocony").
     * Ustawia minPoint na maksymalne mozliwe wartosci, a maxPoint na minimalne.
     */
    void reset() {
        minPoint = glm::vec3(std::numeric_limits<float>::max());
        maxPoint = glm::vec3(std::numeric_limits<float>::lowest());
    }
};

/**
 * @class PlaneBV
 * @brief Reprezentuje nieskonczona plaszczyzne w przestrzeni 3D.
 * Definiowana przez wektor normalny i odleglosc od poczatku ukladu wspolrzednych.
 * Rownanie plaszczyzny: dot(normal, P) - distance = 0.
 */
class PlaneBV : public BoundingVolume {
public:
    glm::vec3 normal;   ///< Wektor normalny plaszczyzny (znormalizowany) w przestrzeni swiata.
    float distance;     ///< Odleglosc plaszczyzny od poczatku ukladu wspolrzednych wzdluz jej normalnej.

    /**
     * @brief Konstruktor.
     * @param worldNormal Wektor normalny plaszczyzny w przestrzeni swiata. Domyslnie (0,1,0) (plaszczyzna XZ).
     * @param worldDistance Odleglosc plaszczyzny od origo. Domyslnie 0.
     */
    PlaneBV(const glm::vec3& worldNormal = glm::vec3(0.0f, 1.0f, 0.0f), float worldDistance = 0.0f);
    ~PlaneBV() override = default;

    /**
     * @brief Zwraca typ ksztaltu jako BoundingShapeType::PLANE.
     * @return BoundingShapeType::PLANE.
     */
    BoundingShapeType getType() const override { return BoundingShapeType::PLANE; }

    /**
     * @brief Aktualizuje parametry plaszczyzny (normalna i odleglosc) w przestrzeni swiata.
     * Transformuje lokalna definicje plaszczyzny (normalna i punkt na plaszczyznie)
     * za pomoca podanej macierzy modelu.
     * @param localNormal Wektor normalny plaszczyzny w przestrzeni lokalnej obiektu.
     * @param localPointOnPlane Dowolny punkt lezacy na plaszczyznie w przestrzeni lokalnej obiektu.
     * @param modelMatrix Macierz transformacji modelu (z lokalnej do swiata).
     */
    void updateWorldVolume(const glm::vec3& localNormal, const glm::vec3& localPointOnPlane, const glm::mat4& modelMatrix);
};

/**
 * @class OBB
 * @brief Reprezentuje Oriented Bounding Box (prostopadloscian zorientowany).
 * Definiowany przez srodek, polowy wymiarow wzdluz jego lokalnych osi oraz macierz orientacji.
 * Moze byc dowolnie obrocony w przestrzeni.
 */
class OBB : public BoundingVolume {
public:
    glm::vec3 center;        ///< Srodek OBB w przestrzeni swiata.
    glm::vec3 halfExtents;   ///< Polowy wymiarow OBB (szerokosc, wysokosc, glebokosc) wzdluz jego lokalnych osi.
    glm::mat3 orientation;   ///< Macierz orientacji 3x3, ktorej kolumny sa znormalizowanymi osiami X, Y, Z OBB w przestrzeni swiata.

    /**
     * @brief Konstruktor.
     * @param worldCenter Srodek OBB w przestrzeni swiata. Domyslnie (0,0,0).
     * @param worldHalfExtents Polowy wymiarow OBB. Domyslnie (0.5, 0.5, 0.5).
     * @param worldOrientation Macierz orientacji OBB. Domyslnie macierz jednostkowa (brak obrotu).
     */
    OBB(const glm::vec3& worldCenter = glm::vec3(0.0f),
        const glm::vec3& worldHalfExtents = glm::vec3(0.5f),
        const glm::mat3& worldOrientation = glm::mat3(1.0f));
    ~OBB() override = default;

    /**
     * @brief Zwraca typ ksztaltu jako BoundingShapeType::OBB.
     * @return BoundingShapeType::OBB.
     */
    BoundingShapeType getType() const override { return BoundingShapeType::OBB; }

    /**
     * @brief Aktualizuje parametry OBB (srodek, polowy wymiarow, orientacja) w przestrzeni swiata.
     * Transformuje lokalna definicje OBB za pomoca podanej macierzy modelu.
     * @param localCenterOffset Przesuniecie srodka OBB wzgledem punktu (0,0,0) obiektu w przestrzeni lokalnej.
     * @param localHalfExtents Polowy wymiarow OBB w jego przestrzeni lokalnej (przed skalowaniem).
     * @param modelMatrix Macierz transformacji modelu (z lokalnej do swiata), moze zawierac translacje, rotacje i skalowanie.
     */
    void updateWorldVolume(const glm::vec3& localCenterOffset, const glm::vec3& localHalfExtents, const glm::mat4& modelMatrix);

    /**
     * @brief Oblicza i zwraca 8 naroznikow OBB w przestrzeni swiata.
     * Przydatne do wizualizacji, debugowania lub niektorych algorytmow kolizji.
     * @return Tablica (std::array) 8 wektorow glm::vec3 reprezentujacych narozniki.
     */
    std::array<glm::vec3, 8> getCorners() const;
};

/**
 * @class CylinderBV
 * @brief Reprezentuje walec kolizyjny.
 * Definiowany przez srodki dwoch podstaw (p1, p2) oraz promien.
 * Os walca jest segmentem laczacym p1 i p2.
 */
class CylinderBV : public BoundingVolume {
public:
    glm::vec3 p1;      ///< Srodek pierwszej podstawy walca w przestrzeni swiata.
    glm::vec3 p2;      ///< Srodek drugiej podstawy walca w przestrzeni swiata.
    float radius;      ///< Promien walca.

    /**
     * @brief Konstruktor.
     * @param worldBaseCenter1 Srodek pierwszej podstawy walca w przestrzeni swiata. Domyslnie (0,-0.5,0).
     * @param worldBaseCenter2 Srodek drugiej podstawy walca w przestrzeni swiata. Domyslnie (0,0.5,0).
     * @param worldRadius Promien walca. Domyslnie 0.5.
     */
    CylinderBV(const glm::vec3& worldBaseCenter1 = glm::vec3(0.0f, -0.5f, 0.0f),
        const glm::vec3& worldBaseCenter2 = glm::vec3(0.0f, 0.5f, 0.0f),
        float worldRadius = 0.5f);
    ~CylinderBV() override = default;

    /**
     * @brief Zwraca typ ksztaltu jako BoundingShapeType::CYLINDER.
     * @return BoundingShapeType::CYLINDER.
     */
    BoundingShapeType getType() const override { return BoundingShapeType::CYLINDER; }

    /**
     * @brief Aktualizuje parametry walca (srodki podstaw, promien) w przestrzeni swiata.
     * Transformuje lokalna definicje walca za pomoca podanej macierzy modelu.
     * @param localP1 Srodek pierwszej podstawy walca w przestrzeni lokalnej.
     * @param localP2 Srodek drugiej podstawy walca w przestrzeni lokalnej.
     * @param localRadius Promien walca w przestrzeni lokalnej (przed skalowaniem).
     * @param modelMatrix Macierz transformacji modelu (z lokalnej do swiata).
     * @note Skalowanie promienia jest uproszczone i zaklada jednorodne skalowanie lub bierze pod uwage maksymalny wspolczynnik skalowania.
     */
    void updateWorldVolume(const glm::vec3& localP1, const glm::vec3& localP2, float localRadius, const glm::mat4& modelMatrix);

    /**
     * @brief Oblicza i zwraca znormalizowany wektor osi walca.
     * Wektor ten jest skierowany od p1 do p2.
     * @return Znormalizowany wektor glm::vec3 reprezentujacy os walca. Zwraca (0,1,0) dla zdegenerowanego walca (p1=p2).
     */
    glm::vec3 getAxis() const {
        if (p1 == p2) return glm::vec3(0.0f, 1.0f, 0.0f); // Domyslna os Y dla punktu
        return glm::normalize(p2 - p1);
    }

    /**
     * @brief Oblicza i zwraca wysokosc walca.
     * Jest to odleglosc miedzy srodkami podstaw p1 i p2.
     * @return Wysokosc walca jako float.
     */
    float getHeight() const {
        return glm::distance(p1, p2);
    }
};

#endif // BOUNDING_VOLUME_H
