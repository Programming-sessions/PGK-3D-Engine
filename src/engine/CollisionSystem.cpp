#include "CollisionSystem.h"
#include "ICollidable.h"      // Dla interfejsu ICollidable i dostepu do jego metod
#include "BoundingVolume.h"   // Dla definicji BoundingShapeType oraz konkretnych klas pochodnych (AABB, PlaneBV, OBB, CylinderBV)
#include "EventManager.h"     // Do rozglaszania zdarzen kolizji
#include "Event.h"            // Dla definicji CollisionEvent
#include "Logger.h"           // Do logowania informacji, ostrzezen

#include <string>             
#include <glm/glm.hpp>        
#include <glm/gtc/matrix_transform.hpp> 
#include <algorithm>          
#include <array>              
#include <vector>             
#include <limits>             

// Funkcja pomocnicza do konwersji typu ksztaltu BoundingShapeType na czytelny string.
// Przydatna do logowania i debugowania.
std::string shapeTypeToStringSAP(BoundingShapeType type) {
    switch (type) {
    case BoundingShapeType::AABB:     return "AABB";
    case BoundingShapeType::PLANE:    return "PLANE";
    case BoundingShapeType::OBB:      return "OBB";
    case BoundingShapeType::CYLINDER: return "CYLINDER";
    case BoundingShapeType::SPHERE:   return "SPHERE"; // Jesli typ SPHERE zostanie dodany
    default:                          return "UNKNOWN_SHAPE_TYPE";
    }
}


CollisionSystem::CollisionSystem() {
    // Konstruktor systemu kolizji.
    // Inicjalizuje wewnetrzne struktury danych (np. wektory sa tworzone jako puste).
    // Obecnie system jest skonfigurowany do uzywania algorytmu Sweep and Prune.
    Logger::getInstance().info("CollisionSystem: System kolizji utworzony (domyslnie z algorytmem Sweep and Prune).");
}

CollisionSystem::~CollisionSystem() {
    // Destruktor systemu kolizji.
    // Czysci liste obiektow kolidujacych oraz dane specyficzne dla algorytmu Sweep and Prune.
    m_collidables.clear();   // Wektor wskaznikow, nie zarzadza czasem zycia obiektow ICollidable
    m_endpointsX.clear();    // Lista punktow koncowych dla SAP
    m_cachedAABBs.clear();   // Cache obliczonych AABB
    Logger::getInstance().info("CollisionSystem: System kolizji zniszczony. Listy obiektow i dane S&P wyczyszczone.");
}

void CollisionSystem::addCollidable(ICollidable* collidable) {
    // Dodaje obiekt do systemu kolizji.
    if (!collidable) {
        Logger::getInstance().warning("CollisionSystem: Proba dodania pustego wskaznika (nullptr) jako obiektu kolidujacego.");
        return;
    }
    // Sprawdzenie, czy obiekt nie zostal juz wczesniej dodany, aby uniknac duplikatow.
    if (std::find(m_collidables.begin(), m_collidables.end(), collidable) == m_collidables.end()) {
        m_collidables.push_back(collidable);
        // Logger::getInstance().debug("CollisionSystem: Obiekt kolidujacy dodany do systemu.");
        // Lista endpointow dla Sweep and Prune zostanie przebudowana w nastepnej metodzie update().
    }
    else {
        // Logger::getInstance().debug("CollisionSystem: Proba dodania obiektu, ktory juz istnieje w systemie.");
    }
}

void CollisionSystem::removeCollidable(ICollidable* collidable) {
    // Usuwa obiekt z systemu kolizji.
    if (!collidable) {
        // Logger::getInstance().debug("CollisionSystem: Proba usuniecia pustego wskaznika (nullptr) z systemu kolizji.");
        return;
    }
    // Usuniecie obiektu z glownej listy obiektow kolidujacych.
    // Idiom erase-remove jest efektywnym sposobem usuwania elementow z wektora.
    m_collidables.erase(std::remove(m_collidables.begin(), m_collidables.end(), collidable), m_collidables.end());

    // Usuniecie obiektu rowniez z mapy przechowujacej cache'owane AABB.
    m_cachedAABBs.erase(collidable);
    // Logger::getInstance().debug("CollisionSystem: Obiekt kolidujacy usuniety z systemu oraz z cache AABB.");
    // Lista endpointow dla Sweep and Prune zostanie przebudowana w nastepnej metodzie update().
    // Dla optymalizacji moznaby implementowac bardziej selektywne usuwanie z m_endpointsX,
    // ale pelne przebudowanie jest prostsze i mniej podatne na bledy przy dynamicznych zmianach.
}

AABB CollisionSystem::getWorldAABB(const BoundingVolume* bv) {
    // Oblicza i zwraca globalne AABB (Axis-Aligned Bounding Box) dla danego BoundingVolume.
    // AABB jest zawsze w przestrzeni swiata.
    if (!bv) {
        Logger::getInstance().warning("CollisionSystem::getWorldAABB - Otrzymano pusty wskaznik (nullptr) do BoundingVolume. Zwracanie pustego AABB.");
        return AABB(glm::vec3(0.0f), glm::vec3(0.0f)); // Zwroc "zerowy" AABB
    }

    switch (bv->getType()) {
    case BoundingShapeType::AABB:
        // Jesli BoundingVolume jest juz typu AABB, po prostu zwracamy jego kopie.
        // Zakladamy, ze AABB jest juz w przestrzeni swiata lub jego transformacja jest uwzgledniona.
        return *static_cast<const AABB*>(bv);

    case BoundingShapeType::OBB:
    {
        // Dla OBB (Oriented Bounding Box), obliczamy AABB otaczajace jego 8 naroznikow.
        const auto* obb = static_cast<const OBB*>(bv);
        std::array<glm::vec3, 8> corners = obb->getCorners(); // Metoda getCorners() powinna zwracac narozniki w przestrzeni swiata.
        if (corners.empty()) { // Zabezpieczenie na wypadek, gdyby getCorners() zwrocilo pusty kontener
            Logger::getInstance().warning("CollisionSystem::getWorldAABB (OBB) - getCorners() zwrocilo pusta liste. Zwracanie pustego AABB.");
            return AABB(glm::vec3(0.0f), glm::vec3(0.0f));
        }

        // Inicjalizacja minPoint i maxPoint AABB pierwszym naroznikiem.
        AABB world_aabb(corners[0], corners[0]);
        // Iteracja po pozostalych naroznikach, aby znalezc minimalne i maksymalne wspolrzedne.
        for (size_t i = 1; i < corners.size(); ++i) {
            world_aabb.minPoint = glm::min(world_aabb.minPoint, corners[i]);
            world_aabb.maxPoint = glm::max(world_aabb.maxPoint, corners[i]);
        }
        return world_aabb;
    }

    case BoundingShapeType::CYLINDER:
    {
        // Dla walca (CylinderBV), obliczamy AABB na podstawie jego dwoch podstaw i promienia.
        const auto* cylinder = static_cast<const CylinderBV*>(bv);
        if (cylinder->radius <= 0.0f) { // Walec o niepoprawnym promieniu
            Logger::getInstance().warning("CollisionSystem::getWorldAABB (CYLINDER) - Niepoprawny promien walca (" + std::to_string(cylinder->radius) + "). Zwracanie pustego AABB.");
            return AABB(glm::vec3(0.0f), glm::vec3(0.0f));
        }

        glm::vec3 cylP1 = cylinder->p1; // Punkt srodkowy pierwszej podstawy
        glm::vec3 cylP2 = cylinder->p2; // Punkt srodkowy drugiej podstawy
        float cylRadius = cylinder->radius;
        glm::vec3 cylAxis = cylinder->getAxis(); // Znormalizowana os walca (od p1 do p2)

        // Wektory ortogonalne do osi walca, potrzebne do znalezienia punktow na obwodzie podstaw.
        glm::vec3 orth1, orth2;

        // Bardziej robustna metoda generowania wektorow ortogonalnych,
        // unika problemow, gdy os walca jest rownolegla do jednej z osi glownych.
        if (std::abs(cylAxis.x) > 0.9f) { // Os jest bliska osi X
            orth1 = glm::normalize(glm::cross(cylAxis, glm::vec3(0.0f, 0.0f, 1.0f))); // Uzyj osi Z jako pomocniczej
        }
        else { // Os nie jest bliska osi X (mozna uzyc osi X jako pomocniczej)
            orth1 = glm::normalize(glm::cross(cylAxis, glm::vec3(1.0f, 0.0f, 0.0f)));
        }
        // Jesli pierwszy wektor ortogonalny jest zerowy (co nie powinno sie zdarzyc przy powyzszej logice,
        // jesli cylAxis nie jest zerowy), sprobuj z inna osia pomocnicza.
        if (glm::length(orth1) < 0.001f) {
            orth1 = glm::normalize(glm::cross(cylAxis, glm::vec3(0.0f, 1.0f, 0.0f))); // Uzyj osi Y
        }

        orth2 = glm::normalize(glm::cross(cylAxis, orth1)); // Drugi wektor ortogonalny

        // Punkty skrajne na obwodach obu podstaw walca.
        std::vector<glm::vec3> extremePoints;
        extremePoints.reserve(8); // 4 punkty na kazdej podstawie
        for (int i = 0; i < 2; ++i) { // Iteracja po dwoch podstawach
            glm::vec3 baseCenter = (i == 0) ? cylP1 : cylP2;
            extremePoints.push_back(baseCenter + orth1 * cylRadius);
            extremePoints.push_back(baseCenter - orth1 * cylRadius);
            extremePoints.push_back(baseCenter + orth2 * cylRadius);
            extremePoints.push_back(baseCenter - orth2 * cylRadius);
        }

        if (extremePoints.empty()) { // Nie powinno sie zdarzyc przy poprawnej logice
            Logger::getInstance().warning("CollisionSystem::getWorldAABB (CYLINDER) - Brak punktow skrajnych do obliczenia AABB. Zwracanie pustego AABB.");
            return AABB(glm::vec3(0.0f), glm::vec3(0.0f));
        }

        // Utworzenie AABB na podstawie punktow skrajnych.
        AABB world_aabb(extremePoints[0], extremePoints[0]);
        for (const auto& p : extremePoints) {
            world_aabb.minPoint = glm::min(world_aabb.minPoint, p);
            world_aabb.maxPoint = glm::max(world_aabb.maxPoint, p);
        }
        return world_aabb;
    }

    case BoundingShapeType::PLANE:
        // Plaszczyzny sa nieskonczone i nie maja sensownego AABB dla algorytmu Sweep and Prune.
        // Zwracamy AABB, ktore jest "odwrocone" (min > max), aby zostalo ono zignorowane
        // podczas tworzenia listy punktow koncowych dla SAP.
        // Kolizje z plaszczyznami sa obslugiwane osobno (np. przez testy z kazdym innym obiektem).
        return AABB(glm::vec3(std::numeric_limits<float>::max()),      // minPoint ustawiony na maksymalne wartosci
            glm::vec3(std::numeric_limits<float>::lowest())); // maxPoint ustawiony na minimalne wartosci

    default:
        Logger::getInstance().warning("CollisionSystem::getWorldAABB - Nieznany lub nieobslugiwany BoundingShapeType: " + shapeTypeToStringSAP(bv->getType()) + ". Zwracanie pustego AABB.");
        return AABB(glm::vec3(0.0f), glm::vec3(0.0f));
    }
}


void CollisionSystem::update(EventManager* eventManager) {
    // Glowna metoda aktualizacji systemu kolizji, wywolywana w kazdej klatce.
    if (!eventManager) {
        Logger::getInstance().error("CollisionSystem::update - EventManager jest pusty (nullptr). Nie mozna rozglaszac zdarzen kolizji.");
        return;
    }

    // Czyszczenie danych z poprzedniej klatki.
    m_endpointsX.clear();    // Lista punktow koncowych dla SAP
    m_cachedAABBs.clear();   // Cache obliczonych AABB

    std::vector<ICollidable*> planesToTestSeparately; // Lista do przechowywania plaszczyzn

    // --- Faza 1: Przygotowanie danych dla Sweep and Prune (SAP) ---
    // Dla kazdego obiektu kolidujacego:
    // 1. Sprawdz, czy kolizje sa wlaczone.
    // 2. Pobierz jego BoundingVolume.
    // 3. Jesli to plaszczyzna, odloz ja do osobnego testowania.
    // 4. Oblicz globalne AABB.
    // 5. Dodaj punkty koncowe (min i max) tego AABB na osi X do listy m_endpointsX.
    // 6. Zapisz obliczone AABB w cache'u (m_cachedAABBs).
    // Logger::getInstance().debug("CollisionSystem: Faza 1 SAP - Przygotowywanie danych...");
    for (ICollidable* collidable : m_collidables) {
        if (!collidable) continue; // Pomin, jesli wskaznik jest pusty

        if (!collidable->collisionsEnabled()) {
            // Logger::getInstance().debug("CollisionSystem: Obiekt pominiety - kolizje wylaczone.");
            continue; // Pomin ten obiekt, jesli ma wylaczone kolizje
        }

        BoundingVolume* bv = collidable->getBoundingVolume(); // Zakladamy, ze ta metoda aktualizuje BV, jesli jest to potrzebne
        if (!bv) {
            // Logger::getInstance().debug("CollisionSystem: Obiekt pominiety - brak BoundingVolume.");
            continue;
        }

        if (bv->getType() == BoundingShapeType::PLANE) {
            planesToTestSeparately.push_back(collidable); // Plaszczyzny beda sprawdzane osobno z innymi obiektami
            continue; // Nie dodajemy plaszczyzn do listy endpointow SAP
        }

        AABB worldAABB = getWorldAABB(bv); // Oblicz AABB w przestrzeni swiata

        // Sprawdzenie, czy AABB jest prawidlowe (min <= max na wszystkich osiach).
        // Nieprawidlowe AABB (np. zwrocone dla plaszczyzn) sa ignorowane przez SAP.
        if (worldAABB.minPoint.x > worldAABB.maxPoint.x ||
            worldAABB.minPoint.y > worldAABB.maxPoint.y ||
            worldAABB.minPoint.z > worldAABB.maxPoint.z) {
            // Logger::getInstance().warning("CollisionSystem::update - Obliczono nieprawidlowe AABB dla obiektu. Pomijanie w SAP.");
            continue;
        }

        m_cachedAABBs[collidable] = worldAABB; // Zapisz obliczone AABB w cache'u

        // Dodaj punkty koncowe (min i max) AABB na osi X do listy endpointow.
        m_endpointsX.push_back({ collidable, worldAABB.minPoint.x, true });  // true dla punktu minimalnego (początek)
        m_endpointsX.push_back({ collidable, worldAABB.maxPoint.x, false }); // false dla punktu maksymalnego (koniec)
    }

    // --- Faza 2: Sortowanie punktow koncowych na osi X ---
    // Logger::getInstance().debug("CollisionSystem: Faza 2 SAP - Sortowanie punktow koncowych...");
    std::sort(m_endpointsX.begin(), m_endpointsX.end()); // Uzywa operatora< zdefiniowanego w Endpoint

    // --- Faza 3: Przemiatanie (Sweep) i identyfikacja potencjalnych kolizji (faza szeroka) ---
    // Logger::getInstance().debug("CollisionSystem: Faza 3 SAP - Przemiatanie...");
    std::vector<ICollidable*> activeList; // Lista obiektow, ktorych AABB aktualnie sie pokrywaja na osi X
    std::vector<std::pair<ICollidable*, ICollidable*>> potentialCollisionPairs; // Pary kandydatow do fazy waskiej

    for (const auto& endpoint : m_endpointsX) {
        ICollidable* currentCollidable = endpoint.collidable;

        // Pobranie AABB z cache'u. Jesli nie ma (co nie powinno sie zdarzyc tutaj), pomin.
        auto it_current_aabb = m_cachedAABBs.find(currentCollidable);
        if (it_current_aabb == m_cachedAABBs.end()) continue;
        const AABB& currentAABB = it_current_aabb->second;

        if (endpoint.isMin) { // Jesli to jest poczatek (minPoint) AABB obiektu:
            // Sprawdz ten obiekt z wszystkimi obiektami aktualnie znajdujacymi sie na liscie aktywnych.
            for (ICollidable* otherCollidableInActiveList : activeList) {
                auto it_other_aabb = m_cachedAABBs.find(otherCollidableInActiveList);
                if (it_other_aabb == m_cachedAABBs.end()) continue;
                const AABB& otherAABBInActiveList = it_other_aabb->second;

                // Sprawdz, czy AABB tych dwoch obiektow pokrywaja sie na osiach Y i Z.
                // Pokrycie na osi X jest juz gwarantowane przez algorytm SAP w tym momencie.
                bool overlapY = currentAABB.maxPoint.y >= otherAABBInActiveList.minPoint.y &&
                    currentAABB.minPoint.y <= otherAABBInActiveList.maxPoint.y;
                bool overlapZ = currentAABB.maxPoint.z >= otherAABBInActiveList.minPoint.z &&
                    currentAABB.minPoint.z <= otherAABBInActiveList.maxPoint.z;

                if (overlapY && overlapZ) {
                    // Jesli AABB pokrywaja sie na wszystkich trzech osiach, mamy potencjalna kolizje.
                    // Dodajemy te pare do listy kandydatow do fazy waskiej.
                    // Aby uniknac duplikatow (np. para (A,B) i (B,A)) i testowania obiektu z samym soba,
                    // zapewniamy spójna kolejnosc w parze (np. mniejszy adres wskaznika jako pierwszy).
                    ICollidable* c1 = currentCollidable;
                    ICollidable* c2 = otherCollidableInActiveList;
                    if (c1 > c2) std::swap(c1, c2); // Zapewnienie spójnej kolejności
                    potentialCollisionPairs.push_back({ c1, c2 });
                }
            }
            // Dodaj biezacy obiekt do listy aktywnych.
            activeList.push_back(currentCollidable);
        }
        else { // Jesli to jest koniec (maxPoint) AABB obiektu:
            // Usun ten obiekt z listy aktywnych.
            activeList.erase(std::remove(activeList.begin(), activeList.end(), currentCollidable), activeList.end());
        }
    }

    // Opcjonalne usuniecie duplikatow z listy par potencjalnych kolizji.
    // Jesli sortowanie wskaznikow w parze (c1,c2) jest stosowane, to unique powinno dzialac poprawnie.
    std::sort(potentialCollisionPairs.begin(), potentialCollisionPairs.end());
    potentialCollisionPairs.erase(std::unique(potentialCollisionPairs.begin(), potentialCollisionPairs.end()), potentialCollisionPairs.end());
    // Logger::getInstance().debug("CollisionSystem: Faza szeroka SAP zidentyfikowala " + std::to_string(potentialCollisionPairs.size()) + " potencjalnych par kolizji.");

    // --- Faza 4: Testy fazy waskiej dla par zidentyfikowanych przez SAP ---
    // Logger::getInstance().debug("CollisionSystem: Faza 4 - Testy waskie dla par z SAP...");
    for (const auto& pair : potentialCollisionPairs) {
        // Sprawdz ponownie, czy kolizje sa wlaczone, na wypadek gdyby stan sie zmienil
        // od czasu budowania listy endpointow (chociaz tutaj to malo prawdopodobne).
        if (!pair.first->collisionsEnabled() || !pair.second->collisionsEnabled()) {
            continue;
        }
        // Wykonaj dokladny test kolizji (faza waska).
        if (performNarrowPhaseCheck(pair.first, pair.second)) {
            // Jesli wykryto kolizje, zaloguj ja i rozglos zdarzenie CollisionEvent.
            Logger::getInstance().info(
                "CollisionSystem: Kolizja (SAP) wykryta miedzy obiektami typu: " +
                shapeTypeToStringSAP(pair.first->getBoundingVolume()->getType()) + " oraz " +
                shapeTypeToStringSAP(pair.second->getBoundingVolume()->getType())
            );
            CollisionEvent collision(pair.first, pair.second); // Uzyj oryginalnych wskaznikow z pary
            eventManager->dispatch(collision);
        }
    }

    // --- Faza 5: Testy kolizji z plaszczyznami ---
    // Plaszczyzny sa traktowane osobno, poniewaz nie pasuja dobrze do algorytmu SAP opartego na AABB.
    // Kazda plaszczyzna jest testowana z kazdym innym obiektem (oprocz innych plaszczyzn, jesli niepotrzebne).
    // Logger::getInstance().debug("CollisionSystem: Faza 5 - Testy kolizji z plaszczyznami...");
    for (ICollidable* planeCollidable : planesToTestSeparately) {
        if (!planeCollidable->collisionsEnabled()) continue; // Sprawdz, czy kolizje dla plaszczyzny sa wlaczone

        for (ICollidable* otherCollidable : m_collidables) {
            if (planeCollidable == otherCollidable) continue; // Nie testuj plaszczyzny z sama soba

            if (!otherCollidable->collisionsEnabled()) continue; // Sprawdz, czy kolizje dla drugiego obiektu sa wlaczone

            // Nie testuj plaszczyzny z inna plaszczyzna tutaj, jesli funkcja performNarrowPhaseCheck
            // obsluguje to poprawnie (PlaneBV vs PlaneBV) i nie chcemy duplikatow zdarzen.
            // Obecna implementacja performNarrowPhaseCheck obsluguje PlaneBV vs PlaneBV.
            // Jesli `otherCollidable` jest plaszczyzna, para (planeCollidable, otherCollidable) zostanie przetestowana.
            // Nalezy jednak uwazac na podwojne testowanie, jesli obie plaszczyzny sa w `planesToTestSeparately`.
            // Prostsze jest testowanie kazdej plaszczyzny z WSZYSTKIMI innymi obiektami (w tym innymi plaszczyznami),
            // a `performNarrowPhaseCheck` znormalizuje kolejnosc.

            if (performNarrowPhaseCheck(planeCollidable, otherCollidable)) {
                Logger::getInstance().info(
                    "CollisionSystem: Kolizja (z plaszczyzna) wykryta miedzy obiektami typu: " +
                    shapeTypeToStringSAP(planeCollidable->getBoundingVolume()->getType()) + " oraz " +
                    shapeTypeToStringSAP(otherCollidable->getBoundingVolume()->getType())
                );
                CollisionEvent collision(planeCollidable, otherCollidable);
                eventManager->dispatch(collision);
            }
        }
    }
    // Logger::getInstance().debug("CollisionSystem: Zakonczono aktualizacje systemu kolizji.");
}


bool CollisionSystem::performNarrowPhaseCheck(ICollidable* collidableA_ptr, ICollidable* collidableB_ptr) {
    // Przeprowadza dokladny test kolizji (faza waska) miedzy dwoma obiektami.
    if (!collidableA_ptr || !collidableB_ptr) {
        // Logger::getInstance().warning("CollisionSystem::performNarrowPhaseCheck - Jeden lub oba wskazniki do ICollidable sa puste.");
        return false;
    }

    // Sprawdzenie, czy kolizje sa wlaczone dla obu obiektow.
    if (!collidableA_ptr->collisionsEnabled() || !collidableB_ptr->collisionsEnabled()) {
        return false; // Jesli ktorykolwiek obiekt ma wylaczone kolizje, nie ma kolizji.
    }

    BoundingVolume* bvA_base_orig = collidableA_ptr->getBoundingVolume();
    BoundingVolume* bvB_base_orig = collidableB_ptr->getBoundingVolume();

    if (!bvA_base_orig || !bvB_base_orig) {
        // Logger::getInstance().warning("CollisionSystem::performNarrowPhaseCheck - Jeden lub oba BoundingVolume sa puste.");
        return false;
    }

    // Tworzymy kopie wskaznikow, ktore beda mogly byc zamienione miejscami
    // w celu normalizacji (np. aby typ A byl zawsze "mniejszy" od typu B).
    BoundingVolume* bvA_base = bvA_base_orig;
    BoundingVolume* bvB_base = bvB_base_orig;

    BoundingShapeType typeA = bvA_base->getType();
    BoundingShapeType typeB = bvB_base->getType();
    bool collisionDetected = false;

    // Normalizacja kolejnosci: ksztalt o "mniejszym" typie (wartosc enum) jako pierwszy.
    // Redukuje to liczbe kombinacji funkcji checkCollision, ktore trzeba zaimplementowac
    // (np. wystarczy AABBvsOBB, nie trzeba OBBvsAABB).
    if (static_cast<int>(typeA) > static_cast<int>(typeB)) {
        std::swap(bvA_base, bvB_base); // Zamien wskazniki do BoundingVolume
        std::swap(typeA, typeB);       // Zamien typy
    }

    // Wybor odpowiedniej funkcji checkCollision na podstawie typow ksztaltow.
    // Uzywamy dynamic_cast lub (lepiej) static_cast po sprawdzeniu typu,
    // aby uzyskac wskazniki do konkretnych typow ksztaltow.
    if (typeA == BoundingShapeType::AABB) {
        auto shapeA = static_cast<const AABB*>(bvA_base);
        if (typeB == BoundingShapeType::AABB)     collisionDetected = checkCollision(shapeA, static_cast<const AABB*>(bvB_base));
        else if (typeB == BoundingShapeType::PLANE)   collisionDetected = checkCollision(shapeA, static_cast<const PlaneBV*>(bvB_base));
        else if (typeB == BoundingShapeType::OBB)     collisionDetected = checkCollision(shapeA, static_cast<const OBB*>(bvB_base));
        else if (typeB == BoundingShapeType::CYLINDER)collisionDetected = checkCollision(shapeA, static_cast<const CylinderBV*>(bvB_base));
        // else if (typeB == BoundingShapeType::SPHERE) collisionDetected = checkCollision(shapeA, static_cast<const SphereBV*>(bvB_base));
    }
    else if (typeA == BoundingShapeType::PLANE) {
        auto shapeA = static_cast<const PlaneBV*>(bvA_base);
        // Nie ma potrzeby testowania Plane vs AABB, bo zostaloby to obsluzone przez normalizacje (AABB vs Plane)
        if (typeB == BoundingShapeType::PLANE)    collisionDetected = checkCollision(shapeA, static_cast<const PlaneBV*>(bvB_base));
        else if (typeB == BoundingShapeType::OBB)     collisionDetected = checkCollision(shapeA, static_cast<const OBB*>(bvB_base));
        else if (typeB == BoundingShapeType::CYLINDER)collisionDetected = checkCollision(shapeA, static_cast<const CylinderBV*>(bvB_base));
        // else if (typeB == BoundingShapeType::SPHERE) collisionDetected = checkCollision(shapeA, static_cast<const SphereBV*>(bvB_base));
    }
    else if (typeA == BoundingShapeType::OBB) {
        auto shapeA = static_cast<const OBB*>(bvA_base);
        // Nie ma potrzeby testowania OBB vs AABB, OBB vs Plane
        if (typeB == BoundingShapeType::OBB)      collisionDetected = checkCollision(shapeA, static_cast<const OBB*>(bvB_base));
        else if (typeB == BoundingShapeType::CYLINDER)collisionDetected = checkCollision(shapeA, static_cast<const CylinderBV*>(bvB_base));
        // else if (typeB == BoundingShapeType::SPHERE) collisionDetected = checkCollision(shapeA, static_cast<const SphereBV*>(bvB_base));
    }
    else if (typeA == BoundingShapeType::CYLINDER) {
        auto shapeA = static_cast<const CylinderBV*>(bvA_base);
        // Nie ma potrzeby testowania Cylinder vs AABB, Cylinder vs Plane, Cylinder vs OBB
        if (typeB == BoundingShapeType::CYLINDER) collisionDetected = checkCollision(shapeA, static_cast<const CylinderBV*>(bvB_base));
        // else if (typeB == BoundingShapeType::SPHERE) collisionDetected = checkCollision(shapeA, static_cast<const SphereBV*>(bvB_base));
    }
    // else if (typeA == BoundingShapeType::SPHERE) {
    //     auto shapeA = static_cast<const SphereBV*>(bvA_base);
    //     if (typeB == BoundingShapeType::SPHERE) collisionDetected = checkCollision(shapeA, static_cast<const SphereBV*>(bvB_base));
    // }
    // ... dodac inne kombinacje, jesli pojawia sie nowe typy ksztaltow

    return collisionDetected;
}


// --- Implementacje funkcji checkCollision (faza wąska) ---
// Te funkcje pozostaja takie same jak w oryginalnej implementacji dostarczonej przez uzytkownika,
// z dodanymi komentarzami i drobnymi poprawkami (np. zabezpieczenia przed nullptr).

// AABB vs AABB
bool CollisionSystem::checkCollision(const AABB* aabb1, const AABB* aabb2) {
    if (!aabb1 || !aabb2) return false; // Zabezpieczenie
    // Kolizja wystepuje, jesli AABB przecinaja sie na wszystkich trzech osiach.
    return aabb1->intersects(*aabb2); // Zakladamy, ze AABB::intersects() implementuje te logike.
}

// AABB vs PlaneBV
bool CollisionSystem::checkCollision(const AABB* aabb, const PlaneBV* plane) {
    if (!aabb || !plane) return false; // Zabezpieczenie

    // Oblicz srodek i polowy wymiar (extents) AABB.
    glm::vec3 center = (aabb->minPoint + aabb->maxPoint) * 0.5f;
    glm::vec3 extents = (aabb->maxPoint - aabb->minPoint) * 0.5f;

    // Oblicz rzutowany promien AABB na normalna plaszczyzny.
    // Jest to maksymalna odleglosc od srodka AABB do jego naroznika
    // w kierunku prostopadlym do plaszczyzny.
    float r_projection = extents.x * std::abs(plane->normal.x) +
        extents.y * std::abs(plane->normal.y) +
        extents.z * std::abs(plane->normal.z);

    // Oblicz odleglosc srodka AABB od plaszczyzny.
    // s > 0 oznacza, ze srodek jest po stronie "pozytywnej" normalnej.
    // s < 0 oznacza, ze srodek jest po stronie "negatywnej" normalnej.
    // s = 0 oznacza, ze srodek lezy na plaszczyznie.
    float s_center_dist = glm::dot(center, plane->normal) - plane->distance;

    // Kolizja wystepuje, jesli absolutna odleglosc srodka od plaszczyzny
    // jest mniejsza lub rowna rzutowanemu promieniowi AABB.
    return std::abs(s_center_dist) <= r_projection;
}

// Funkcja pomocnicza dla Separating Axis Theorem (SAT): projekcja OBB na os.
// Oblicza minimalna i maksymalna wartosc projekcji 8 naroznikow OBB na dana os.
void getInterval(const OBB* obb, const glm::vec3& axis, float& min_val, float& max_val) {
    std::array<glm::vec3, 8> corners = obb->getCorners(); // Pobierz narozniki OBB w przestrzeni swiata
    if (corners.empty()) { // Zabezpieczenie na wypadek bledu
        min_val = 0.0f; max_val = 0.0f;
        Logger::getInstance().warning("CollisionSystem::getInterval (OBB) - getCorners() zwrocilo pusta liste.");
        return;
    }
    // Inicjalizuj min/max pierwszym naroznikiem.
    min_val = max_val = glm::dot(corners[0], axis);
    // Przetworz pozostale narozniki.
    for (int i = 1; i < 8; ++i) {
        float projection = glm::dot(corners[i], axis);
        if (projection < min_val) min_val = projection;
        if (projection > max_val) max_val = projection;
    }
}

// Funkcja pomocnicza dla SAT: projekcja AABB na os.
// Oblicza minimalna i maksymalna wartosc projekcji AABB na dana os.
void getIntervalAABB(const AABB* aabb, const glm::vec3& axis, float& min_val, float& max_val) {
    // Oblicz srodek i polowy wymiar (extents) AABB.
    glm::vec3 center = (aabb->minPoint + aabb->maxPoint) * 0.5f;
    glm::vec3 extents = (aabb->maxPoint - aabb->minPoint) * 0.5f;

    // Oblicz rzutowany promien AABB na os (analogicznie do AABB vs Plane).
    float r_projection = extents.x * std::abs(axis.x) +
        extents.y * std::abs(axis.y) +
        extents.z * std::abs(axis.z);
    // Oblicz projekcje srodka AABB na os.
    float c_projection = glm::dot(center, axis);

    // Przedzial projekcji AABB to [c - r, c + r].
    min_val = c_projection - r_projection;
    max_val = c_projection + r_projection;
}

// AABB vs OBB (przy uzyciu Separating Axis Theorem - SAT)
bool CollisionSystem::checkCollision(const AABB* aabb, const OBB* obb) {
    if (!aabb || !obb) return false; // Zabezpieczenie

    std::vector<glm::vec3> testAxes; // Lista osi do przetestowania
    testAxes.reserve(15); // Maksymalnie 3 (AABB) + 3 (OBB) + 3*3 (iloczyny wektorowe) = 15 osi.

    // 1. Osie normalne scian AABB (osie swiata X, Y, Z)
    testAxes.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
    testAxes.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
    testAxes.push_back(glm::vec3(0.0f, 0.0f, 1.0f));

    // 2. Osie normalne scian OBB (osie orientacji OBB)
    testAxes.push_back(obb->orientation[0]); // Lokalna os X OBB
    testAxes.push_back(obb->orientation[1]); // Lokalna os Y OBB
    testAxes.push_back(obb->orientation[2]); // Lokalna os Z OBB

    // 3. Iloczyny wektorowe miedzy osiami AABB i OBB
    // Dla kazdej osi AABB (a_i) i kazdej osi OBB (b_j), testujemy os cross(a_i, b_j).
    for (int i = 0; i < 3; ++i) { // Iteracja po osiach AABB (pierwsze 3 w testAxes)
        for (int j = 0; j < 3; ++j) { // Iteracja po osiach OBB (kolejne 3 w testAxes, tj. testAxes[3] do testAxes[5])
            glm::vec3 crossProduct = glm::cross(testAxes[i], testAxes[j + 3]);
            // Dodaj os do testow tylko jesli nie jest wektorem zerowym (co oznacza, ze oryginalne osie nie byly rownolegle).
            if (glm::length(crossProduct) > 0.0001f) { // Prawie zerowa dlugosc oznacza rownoleglosc
                testAxes.push_back(glm::normalize(crossProduct)); // Normalizuj os testowa
            }
        }
    }

    // Testowanie kazdej osi z listy.
    for (const auto& axis : testAxes) {
        if (glm::length(axis) < 0.0001f) continue; // Pomin zdegenerowane osie (malo prawdopodobne po normalizacji)

        float minA, maxA, minB, maxB; // Przedzialy projekcji dla AABB (A) i OBB (B)
        getIntervalAABB(aabb, axis, minA, maxA); // Oblicz projekcje AABB
        getInterval(obb, axis, minB, maxB);     // Oblicz projekcje OBB

        // Jesli przedzialy projekcji nie pokrywaja sie, znalezlismy os separujaca.
        // Oznacza to, ze obiekty nie koliduja.
        if (maxA < minB || maxB < minA) {
            return false; // Znaleziono os separujaca, brak kolizji.
        }
    }
    // Jesli nie znaleziono zadnej osi separujacej po przetestowaniu wszystkich 15 (lub mniej) osi,
    // obiekty koliduja.
    return true;
}

// AABB vs CylinderBV (uproszczony test)
bool CollisionSystem::checkCollision(const AABB* aabb, const CylinderBV* cylinder) {
    if (!aabb || !cylinder) return false;

    // Obecna implementacja jest uproszczona i opiera sie na tescie AABB vs AABB,
    // gdzie AABB dla walca jest najpierw obliczane.
    // To jest test fazy szerokiej dla tej pary, a nie dokladny test fazy waskiej.
    AABB cylinderAABB = getWorldAABB(cylinder); // Uzyj funkcji pomocniczej do obliczenia AABB walca

    // Sprawdzenie, czy obliczone AABB dla walca jest prawidlowe.
    if (cylinderAABB.minPoint.x > cylinderAABB.maxPoint.x ||
        cylinderAABB.minPoint.y > cylinderAABB.maxPoint.y ||
        cylinderAABB.minPoint.z > cylinderAABB.maxPoint.z) {
        // To moze sie zdarzyc, jesli np. cylinder ma zerowy promien.
        // Logger::getInstance().warning("CollisionSystem::checkCollision (AABB vs CylinderBV) - Nieprawidlowe AABB dla walca.");
        return false;
    }

    // Jesli AABB obu obiektow sie nie przecinaja, na pewno nie ma kolizji.
    if (!aabb->intersects(cylinderAABB)) {
        return false;
    }

    // TODO: Zaimplementowac dokladniejszy test kolizji AABB vs Cylinder.
    // Ten test powinien uwzgledniac:
    // 1. Odleglosc wierzcholkow AABB od osi walca.
    // 2. Odleglosc wierzcholkow AABB od podstaw walca.
    // 3. Przeciecia krawedzi AABB z powierzchnia boczna i podstawami walca.
    // Obecnie, jesli AABB sie przecinaja, zakladamy kolizje, co moze prowadzic
    // do falszywych pozytywow (false positives).
    // Logger::getInstance().warning("CollisionSystem: Test kolizji AABB vs CylinderBV jest uproszczony (AABB vs AABB walca). Wymaga dokladniejszej implementacji.");
    return true; // Tymczasowo, jesli AABB sie pokrywaja, uznajemy to za kolizje.
}

// PlaneBV vs PlaneBV
bool CollisionSystem::checkCollision(const PlaneBV* plane1, const PlaneBV* plane2) {
    if (!plane1 || !plane2) return false;

    // Dwie plaszczyzny koliduja (przecinaja sie), jesli nie sa rownolegle.
    // Jesli sa rownolegle, koliduja tylko wtedy, gdy sa wspolplaszczyznowe (ta sama plaszczyzna).
    glm::vec3 crossProduct = glm::cross(plane1->normal, plane2->normal);

    // Jesli dlugosc iloczynu wektorowego normalnych jest wieksza od malego epsilon,
    // plaszczyzny nie sa rownolegle i na pewno sie przecinaja.
    if (glm::length(crossProduct) > 0.0001f) {
        return true; // Plaszczyzny nie sa rownolegle, wiec sie przecinaja.
    }

    // Jesli sa rownolegle (iloczyn wektorowy bliski zeru), sprawdz, czy sa wspolplaszczyznowe.
    // Normalne musza byc rownolegle (co juz sprawdzilismy) ORAZ odleglosci od poczatku ukladu
    // musza byc takie same (jesli normalne sa w tym samym kierunku)
    // LUB suma odleglosci musi byc bliska zeru (jesli normalne sa w przeciwnych kierunkach,
    // a plaszczyzny sa zdefiniowane jako N*P = d, gdzie d jest odlegloscia).
    // Zakladajac, ze 'distance' w PlaneBV jest odlegloscia wzdluz normalnej.
    if (std::abs(glm::dot(plane1->normal, plane2->normal)) > 0.999f) { // Potwierdzenie rownoleglosci normalnych
        // Jesli normalne sa w tym samym kierunku (dot > 0) i odleglosci sa takie same
        if (glm::dot(plane1->normal, plane2->normal) > 0 && std::abs(plane1->distance - plane2->distance) < 0.001f) {
            return true; // Wspolplaszczyznowe, ta sama orientacja normalnej
        }
        // Jesli normalne sa w przeciwnych kierunkach (dot < 0) i suma odleglosci (z odpowiednim znakiem) jest bliska zeru
        // Przyklad: N1=(0,1,0), d1=5; N2=(0,-1,0), d2=-5. Wtedy sa wspolplaszczyznowe.
        // |d1 - (-d2)| < eps  => |d1+d2| < eps
        if (glm::dot(plane1->normal, plane2->normal) < 0 && std::abs(plane1->distance + plane2->distance) < 0.001f) {
            return true; // Wspolplaszczyznowe, przeciwna orientacja normalnej
        }
    }
    return false; // Rownolegle, ale nie wspolplaszczyznowe.
}

// PlaneBV vs OBB
bool CollisionSystem::checkCollision(const PlaneBV* plane, const OBB* obb) {
    if (!plane || !obb) return false;

    // Oblicz rzutowany promien OBB na normalna plaszczyzny.
    // Jest to suma projekcji polowy wymiarow OBB (extents) na normalna plaszczyzny.
    float r_projection = obb->halfExtents.x * std::abs(glm::dot(plane->normal, obb->orientation[0])) +
        obb->halfExtents.y * std::abs(glm::dot(plane->normal, obb->orientation[1])) +
        obb->halfExtents.z * std::abs(glm::dot(plane->normal, obb->orientation[2]));

    // Oblicz odleglosc srodka OBB od plaszczyzny.
    float s_center_dist = glm::dot(plane->normal, obb->center) - plane->distance;

    // Kolizja wystepuje, jesli absolutna odleglosc srodka OBB od plaszczyzny
    // jest mniejsza lub rowna rzutowanemu promieniowi OBB.
    return std::abs(s_center_dist) <= r_projection;
}

// PlaneBV vs CylinderBV
bool CollisionSystem::checkCollision(const PlaneBV* plane, const CylinderBV* cylinder) {
    if (!plane || !cylinder) return false;

    // Oblicz odleglosci (z znakiem) srodkow podstaw walca od plaszczyzny.
    float distP1 = glm::dot(plane->normal, cylinder->p1) - plane->distance;
    float distP2 = glm::dot(plane->normal, cylinder->p2) - plane->distance;

    // 1. Jesli srodki podstaw leza po przeciwnych stronach plaszczyzny (lub jeden na plaszczyznie),
    //    segment osi walca przecina plaszczyzne, wiec walec na pewno koliduje z plaszczyzna.
    if (distP1 * distP2 <= 0.0f) { // Iloczyn <= 0 oznacza rozne znaki lub jeden zero.
        return true;
    }

    // 2. Jesli obie podstawy sa po tej samej stronie plaszczyzny.
    //    Znajdz punkt na osi walca, ktory jest najblizej plaszczyzny.
    //    To bedzie jeden z koncow osi (p1 lub p2).
    //    Odleglosc tego najblizszego punktu osi od plaszczyzny to min(|distP1|, |distP2|).
    float closestAxisPointDist = std::min(std::abs(distP1), std::abs(distP2));

    // 3. Oblicz rzutowany promien walca na normalna plaszczyzny.
    //    Maksymalna odleglosc punktu na obwodzie walca od jego osi, mierzona wzdluz normalnej plaszczyzny.
    //    r_proj = R * sqrt(1 - (dot(N_plaszczyzny, N_osi_walca))^2)
    //    gdzie N_osi_walca to znormalizowany wektor osi walca.
    glm::vec3 cylinderLocalAxis = cylinder->getAxis(); // Zakladamy, ze getAxis() zwraca znormalizowana os.
    float cosAngle = glm::dot(plane->normal, cylinderLocalAxis); // Kosinus kata miedzy normalna plaszczyzny a osia walca
    float sinAngleSq = 1.0f - cosAngle * cosAngle; // sin^2(theta)
    // Zapobieganie ujemnym wartosciom pod pierwiastkiem z powodu bledow numerycznych
    if (sinAngleSq < 0.0f) sinAngleSq = 0.0f;
    float projectedRadius = cylinder->radius * std::sqrt(sinAngleSq);

    // Kolizja wystepuje, jesli najblizszy punkt osi walca jest od plaszczyzny
    // w odleglosci mniejszej lub rownej rzutowanemu promieniowi.
    if (closestAxisPointDist <= projectedRadius) {
        return true;
    }

    return false;
}

// OBB vs OBB (Separating Axis Theorem - SAT)
bool CollisionSystem::checkCollision(const OBB* obb1, const OBB* obb2) {
    if (!obb1 || !obb2) return false; // Zabezpieczenie

    std::vector<glm::vec3> testAxes; // Lista osi do przetestowania
    testAxes.reserve(15); // Maksymalnie 3 (OBB1) + 3 (OBB2) + 3*3 (iloczyny wektorowe) = 15 osi.

    // 1. Osie normalne scian OBB1
    testAxes.push_back(obb1->orientation[0]);
    testAxes.push_back(obb1->orientation[1]);
    testAxes.push_back(obb1->orientation[2]);

    // 2. Osie normalne scian OBB2
    testAxes.push_back(obb2->orientation[0]);
    testAxes.push_back(obb2->orientation[1]);
    testAxes.push_back(obb2->orientation[2]);

    // 3. Iloczyny wektorowe miedzy osiami OBB1 i OBB2
    for (int i = 0; i < 3; ++i) { // Iteracja po osiach OBB1 (pierwsze 3 w testAxes)
        for (int j = 0; j < 3; ++j) { // Iteracja po osiach OBB2 (kolejne 3 w testAxes, tj. testAxes[3] do testAxes[5])
            glm::vec3 crossProduct = glm::cross(testAxes[i], testAxes[j + 3]);
            if (glm::length(crossProduct) > 0.0001f) { // Dodaj tylko jesli nie jest wektorem zerowym
                testAxes.push_back(glm::normalize(crossProduct)); // Normalizuj os testowa
            }
        }
    }

    // Testowanie kazdej osi z listy.
    for (const auto& axis : testAxes) {
        if (glm::length(axis) < 0.0001f) continue; // Pomin zdegenerowane osie

        float min1, max1, min2, max2; // Przedzialy projekcji dla OBB1 i OBB2
        getInterval(obb1, axis, min1, max1); // Oblicz projekcje OBB1
        getInterval(obb2, axis, min2, max2); // Oblicz projekcje OBB2

        // Jesli przedzialy projekcji nie pokrywaja sie, znalezlismy os separujaca.
        if (max1 < min2 || max2 < min1) {
            return false; // Znaleziono os separujaca, brak kolizji.
        }
    }
    // Jesli nie znaleziono zadnej osi separujacej, obiekty koliduja.
    return true;
}

// OBB vs CylinderBV (uproszczony test)
bool CollisionSystem::checkCollision(const OBB* obb, const CylinderBV* cylinder) {
    if (!obb || !cylinder) return false;

    // TODO: Zaimplementowac dokladny test kolizji OBB vs Cylinder. Jest to bardzo zlozone.
    // Obecnie uzywane jest uproszczenie: test AABB otaczajacego OBB z AABB otaczajacym walec.
    AABB obb_aabb = getWorldAABB(obb);             // Oblicz AABB dla OBB
    AABB cylinder_aabb = getWorldAABB(cylinder);   // Oblicz AABB dla walca

    // Sprawdzenie, czy obliczone AABB sa prawidlowe.
    if (obb_aabb.minPoint.x > obb_aabb.maxPoint.x || cylinder_aabb.minPoint.x > cylinder_aabb.maxPoint.x) {
        // Logger::getInstance().warning("CollisionSystem::checkCollision (OBB vs CylinderBV) - Nieprawidlowe AABB dla OBB lub walca.");
        return false; // Jeden lub oba AABB sa nieprawidlowe (np. "odwrocone").
    }

    // Logger::getInstance().warning("CollisionSystem: Test kolizji OBB vs CylinderBV jest wysoce uproszczony (AABB OBB vs AABB Walca). Wymagana dokladniejsza implementacja.");
    // Jesli AABB sie przecinaja, zakladamy potencjalna kolizje. Moze to prowadzic do falszywych pozytywow.
    return obb_aabb.intersects(cylinder_aabb);
}

// CylinderBV vs CylinderBV (uproszczony test)
bool CollisionSystem::checkCollision(const CylinderBV* cyl1, const CylinderBV* cyl2) {
    if (!cyl1 || !cyl2) return false;

    // TODO: Zaimplementowac dokladny test kolizji Cylinder vs Cylinder. Jest to bardzo zlozone.
    // Obecnie uzywane jest uproszczenie: test AABB otaczajacego pierwszy walec z AABB otaczajacym drugi walec.
    AABB aabb1 = getWorldAABB(cyl1); // Oblicz AABB dla pierwszego walca
    AABB aabb2 = getWorldAABB(cyl2); // Oblicz AABB dla drugiego walca

    // Sprawdzenie, czy obliczone AABB sa prawidlowe.
    if (aabb1.minPoint.x > aabb1.maxPoint.x || aabb2.minPoint.x > aabb2.maxPoint.x) {
        // Logger::getInstance().warning("CollisionSystem::checkCollision (CylinderBV vs CylinderBV) - Nieprawidlowe AABB dla jednego lub obu walcow.");
        return false; // Jeden lub oba AABB sa nieprawidlowe.
    }

    // Logger::getInstance().warning("CollisionSystem: Test kolizji CylinderBV vs CylinderBV jest wysoce uproszczony (AABB vs AABB). Wymagana dokladniejsza implementacja.");
    // Jesli AABB sie przecinaja, zakladamy potencjalna kolizje. Moze to prowadzic do falszywych pozytywow.
    return aabb1.intersects(aabb2);
}

