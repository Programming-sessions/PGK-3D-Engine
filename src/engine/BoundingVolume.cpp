#include "BoundingVolume.h"
#include "Primitives.h" // Potrzebne dla definicji struktury Vertex (dla AABB::updateFromVertices)
// Upewnij sie, ze Primitives.h jest "lekki" lub rozwaz alternatywne przekazywanie danych wierzcholkow.
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // Dla glm::inverse, glm::transpose
#include <vector>
#include <limits>                       // Dla std::numeric_limits
#include <algorithm>                    // Dla std::min, std::max
#include <array>                        // Dla std::array

// --- Implementacje metod dla klasy AABB ---

/**
 * @brief Aktualizuje wymiary AABB na podstawie zbioru lokalnych wierzcholkow obiektu i jego macierzy modelu.
 * Transformuje kazdy lokalny wierzcholek do przestrzeni swiata i rozszerza AABB tak,
 * aby obejmowalo wszystkie przetransformowane wierzcholki. Jesli lista wierzcholkow jest pusta,
 * AABB jest ustawiane na punkt (0,0,0).
 * @param localVertices Wektor lokalnych wierzcholkow obiektu (typu Vertex).
 * @param modelMatrix Macierz modelu transformujaca obiekt z przestrzeni lokalnej do swiata.
 */
void AABB::updateFromVertices(const std::vector<Vertex>& localVertices, const glm::mat4& modelMatrix) {
    // Jesli brak wierzcholkow, ustaw AABB jako punkt w origo lub pozostaw "odwrocone".
    // Ustawienie na punkt (0,0,0) moze byc bardziej przewidywalne niz pozostawienie max/lowest.
    if (localVertices.empty()) {
        minPoint = glm::vec3(0.0f);
        maxPoint = glm::vec3(0.0f);
        return;
    }

    // Zainicjalizuj minPoint na maksymalne mozliwe wartosci, a maxPoint na minimalne.
    // Pierwszy przetworzony wierzcholek ustawi poprawne poczatkowe granice.
    minPoint = glm::vec3(std::numeric_limits<float>::max());
    maxPoint = glm::vec3(std::numeric_limits<float>::lowest());

    // Iteruj po wszystkich lokalnych wierzcholkach.
    for (const auto& vertex : localVertices) {
        // Transformuj pozycje wierzcholka z przestrzeni lokalnej do przestrzeni swiata.
        glm::vec3 worldPos = glm::vec3(modelMatrix * glm::vec4(vertex.position, 1.0f));

        // Aktualizuj minimalne i maksymalne wspolrzedne AABB.
        minPoint.x = std::min(minPoint.x, worldPos.x);
        minPoint.y = std::min(minPoint.y, worldPos.y);
        minPoint.z = std::min(minPoint.z, worldPos.z);

        maxPoint.x = std::max(maxPoint.x, worldPos.x);
        maxPoint.y = std::max(maxPoint.y, worldPos.y);
        maxPoint.z = std::max(maxPoint.z, worldPos.z);
    }
}

// --- Implementacje metod dla klasy PlaneBV ---

/**
 * @brief Konstruktor plaszczyzny kolizyjnej.
 * @param worldNormal Wektor normalny plaszczyzny w przestrzeni swiata. Zostanie znormalizowany.
 * @param worldDistance Odleglosc plaszczyzny od origo wzdluz jej normalnej.
 */
PlaneBV::PlaneBV(const glm::vec3& worldNormal, float worldDistance)
    : normal(glm::normalize(worldNormal)), distance(worldDistance) {
    // Normalna jest zawsze przechowywana jako znormalizowana.
}

/**
 * @brief Aktualizuje parametry plaszczyzny (normalna i odleglosc) w przestrzeni swiata.
 * Transformuje lokalna definicje plaszczyzny (normalna i punkt na plaszczyznie)
 * za pomoca podanej macierzy modelu.
 * @param localNormal Wektor normalny plaszczyzny w przestrzeni lokalnej obiektu. Powinien byc znormalizowany.
 * @param localPointOnPlane Dowolny punkt lezacy na plaszczyznie w przestrzeni lokalnej obiektu.
 * @param modelMatrix Macierz transformacji modelu (z lokalnej do swiata).
 */
void PlaneBV::updateWorldVolume(const glm::vec3& localNormal, const glm::vec3& localPointOnPlane, const glm::mat4& modelMatrix) {
    // Transformacja wektora normalnego:
    // Aby poprawnie transformowac normalne (zwlaszcza przy niejednorodnym skalowaniu w macierzy modelu),
    // nalezy uzyc transponowanej odwrotnosci macierzy transformacji (tylko jej czesci rotacyjno-skalujacej 3x3).
    // Jesli macierz modelu zawiera tylko translacje, rotacje i jednorodne skalowanie,
    // mozna by uzyc prostszej transformacji glm::mat3(modelMatrix) * localNormal.
    // Dla ogolnosci, uzywamy pelnej transformacji normalnych.
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
    this->normal = glm::normalize(normalMatrix * localNormal); // Wynikowa normalna musi byc znormalizowana.

    // Transformacja punktu lezacego na plaszczyznie z przestrzeni lokalnej do swiata.
    glm::vec3 worldPointOnPlane = glm::vec3(modelMatrix * glm::vec4(localPointOnPlane, 1.0f));

    // Obliczenie nowej odleglosci 'd' dla rownania plaszczyzny w przestrzeni swiata:
    // Rownanie plaszczyzny: dot(N, P) - d = 0, gdzie N to normalna, P to punkt na plaszczyznie.
    // Stad d = dot(N, P_world), gdzie P_world to przetransformowany punkt na plaszczyznie.
    this->distance = glm::dot(this->normal, worldPointOnPlane);
}


// --- Implementacje metod dla klasy OBB ---

/**
 * @brief Konstruktor zorientowanego prostopadloscianu (OBB).
 * @param worldCenter Srodek OBB w przestrzeni swiata.
 * @param worldHalfExtents Polowy wymiarow OBB wzdluz jego lokalnych osi (przed transformacja orientacji).
 * @param worldOrientation Macierz orientacji 3x3, ktorej kolumny sa osiami OBB w przestrzeni swiata.
 */
OBB::OBB(const glm::vec3& worldCenter, const glm::vec3& worldHalfExtents, const glm::mat3& worldOrientation)
    : center(worldCenter), halfExtents(worldHalfExtents), orientation(worldOrientation) {
}

/**
 * @brief Aktualizuje parametry OBB (srodek, polowy wymiarow, orientacja) w przestrzeni swiata.
 * Transformuje lokalna definicje OBB za pomoca podanej macierzy modelu.
 * @param localCenterOffset Przesuniecie srodka OBB wzgledem punktu (0,0,0) obiektu w przestrzeni lokalnej.
 * @param localHalfExtents Polowy wymiarow OBB w jego przestrzeni lokalnej (przed jakimkolwiek skalowaniem z macierzy modelu).
 * @param modelMatrix Macierz transformacji modelu (z lokalnej do swiata), moze zawierac translacje, rotacje i skalowanie.
 */
void OBB::updateWorldVolume(const glm::vec3& localCenterOffset, const glm::vec3& localHalfExtents, const glm::mat4& modelMatrix) {
    // Transformacja srodka OBB z przestrzeni lokalnej (z uwzglednieniem offsetu) do przestrzeni swiata.
    this->center = glm::vec3(modelMatrix * glm::vec4(localCenterOffset, 1.0f));

    // Wyodrebnienie macierzy rotacji i skalowania (czesci 3x3) z macierzy modelu.
    // Ta macierz bedzie stanowic podstawe dla orientacji OBB w przestrzeni swiata.
    glm::mat3 rotationAndScaleMatrix = glm::mat3(modelMatrix);

    // Obliczenie wspolczynnikow skalowania na podstawie dlugosci wektorow bazowych
    // transformowanej macierzy (kolumn macierzy rotacyjno-skalujacej).
    glm::vec3 scale;
    scale.x = glm::length(rotationAndScaleMatrix[0]); // Dlugosc pierwszej kolumny (transformowana os X)
    scale.y = glm::length(rotationAndScaleMatrix[1]); // Dlugosc drugiej kolumny (transformowana os Y)
    scale.z = glm::length(rotationAndScaleMatrix[2]); // Dlugosc trzeciej kolumny (transformowana os Z)

    // Zastosowanie obliczonego skalowania do polowy wymiarow OBB.
    // localHalfExtents sa w przestrzeni lokalnej i nie sa jeszcze przeskalowane.
    this->halfExtents = localHalfExtents * scale;

    // Ustawienie macierzy orientacji OBB.
    // Kolumny macierzy orientacji musza byc znormalizowanymi osiami OBB.
    // Dzielimy kolumny macierzy rotacyjno-skalujacej przez odpowiednie wspolczynniki skalowania.
    this->orientation = rotationAndScaleMatrix;
    if (scale.x > 1e-6f) this->orientation[0] /= scale.x; else this->orientation[0] = glm::vec3(1.f, 0.f, 0.f); // Zabezpieczenie przed dzieleniem przez zero
    if (scale.y > 1e-6f) this->orientation[1] /= scale.y; else this->orientation[1] = glm::vec3(0.f, 1.f, 0.f);
    if (scale.z > 1e-6f) this->orientation[2] /= scale.z; else this->orientation[2] = glm::vec3(0.f, 0.f, 1.f);
}

/**
 * @brief Oblicza i zwraca 8 naroznikow OBB w przestrzeni swiata.
 * @return Tablica (std::array) 8 wektorow glm::vec3 reprezentujacych narozniki.
 */
std::array<glm::vec3, 8> OBB::getCorners() const {
    std::array<glm::vec3, 8> corners;
    // Wektory reprezentujace polowy wymiarow OBB wzdluz jego zorientowanych osi.
    glm::vec3 u = orientation[0] * halfExtents.x; // Wektor wzdluz osi X OBB
    glm::vec3 v = orientation[1] * halfExtents.y; // Wektor wzdluz osi Y OBB
    glm::vec3 w = orientation[2] * halfExtents.z; // Wektor wzdluz osi Z OBB

    // Obliczenie pozycji kazdego z 8 naroznikow przez kombinacje
    // srodka OBB oraz wektorow u, v, w.
    corners[0] = center - u - v - w;
    corners[1] = center + u - v - w;
    corners[2] = center + u + v - w;
    corners[3] = center - u + v - w;
    corners[4] = center - u - v + w;
    corners[5] = center + u - v + w;
    corners[6] = center + u + v + w;
    corners[7] = center - u + v + w;

    return corners;
}


// --- Implementacje metod dla klasy CylinderBV ---

/**
 * @brief Konstruktor walca kolizyjnego.
 * @param worldBaseCenter1 Srodek pierwszej podstawy walca w przestrzeni swiata.
 * @param worldBaseCenter2 Srodek drugiej podstawy walca w przestrzeni swiata.
 * @param worldRadius Promien walca.
 */
CylinderBV::CylinderBV(const glm::vec3& worldBaseCenter1, const glm::vec3& worldBaseCenter2, float worldRadius)
    : p1(worldBaseCenter1), p2(worldBaseCenter2), radius(worldRadius) {
}

/**
 * @brief Aktualizuje parametry walca (srodki podstaw, promien) w przestrzeni swiata.
 * Transformuje lokalna definicje walca za pomoca podanej macierzy modelu.
 * @param localP1 Srodek pierwszej podstawy walca w przestrzeni lokalnej.
 * @param localP2 Srodek drugiej podstawy walca w przestrzeni lokalnej.
 * @param localRadius Promien walca w przestrzeni lokalnej (przed skalowaniem).
 * @param modelMatrix Macierz transformacji modelu (z lokalnej do swiata).
 */
void CylinderBV::updateWorldVolume(const glm::vec3& localP1, const glm::vec3& localP2, float localRadius, const glm::mat4& modelMatrix) {
    // Transformacja srodkow podstaw walca z przestrzeni lokalnej do swiata.
    this->p1 = glm::vec3(modelMatrix * glm::vec4(localP1, 1.0f));
    this->p2 = glm::vec3(modelMatrix * glm::vec4(localP2, 1.0f));

    // Skalowanie promienia. Jest to uproszczenie, ktore dziala najlepiej dla jednorodnego skalowania.
    // Przy niejednorodnym skalowaniu, walec moze stac sie walcem eliptycznym,
    // co znacznie komplikuje detekcje kolizji i reprezentacje promienia.
    // Tutaj przyjmujemy maksymalny wspolczynnik skalowania z osi macierzy modelu,
    // co daje "najgrubszy" mozliwy walec otaczajacy, co jest bezpieczniejsze (moze prowadzic do
    // wiekszej liczby falszywych pozytywow w fazie szerokiej, ale nie pominie kolizji).
    glm::vec3 scaleFactors;
    scaleFactors.x = glm::length(glm::vec3(modelMatrix[0])); // Skalowanie wzdluz osi X transformacji
    scaleFactors.y = glm::length(glm::vec3(modelMatrix[1])); // Skalowanie wzdluz osi Y transformacji
    scaleFactors.z = glm::length(glm::vec3(modelMatrix[2])); // Skalowanie wzdluz osi Z transformacji

    // Uzycie maksymalnego ze wspolczynnikow skalowania do przeskalowania promienia.
    // To zapewnia, ze promien bedzie wystarczajaco duzy, aby pokryc walec nawet po anizotropowym skalowaniu.
    // Alternatywnie, mozna by probowac obliczyc skalowanie w plaszczyznie prostopadlej do osi walca,
    // ale jest to bardziej skomplikowane.
    this->radius = localRadius * std::max({ scaleFactors.x, scaleFactors.y, scaleFactors.z });
    // Jesli wiemy, ze skalowanie jest jednorodne, mozna uzyc np. scaleFactors.x.
    // this->radius = localRadius * scaleFactors.x;
}
