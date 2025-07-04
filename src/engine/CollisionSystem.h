/**
* @file CollisionSystem.h
* @brief Definicja klasy CollisionSystem.
*
* Plik ten zawiera deklarację klasy CollisionSystem, która jest
* odpowiedzialna za wykrywanie i obsługę kolizji między
* obiektami w grze.
*/

#ifndef COLLISION_SYSTEM_H
#define COLLISION_SYSTEM_H

#include <vector>
#include <algorithm> // Dla std::remove, std::sort
#include <utility>   // Dla std::pair
#include <map>       // Dla m_cachedAABBs

// --- Deklaracje wyprzedzajace ---
// Pelne definicje tych klas/struktur beda potrzebne w CollisionSystem.cpp
// lub w innych miejscach, gdzie sa faktycznie uzywane.
class ICollidable;
class EventManager;
class BoundingVolume; // Ogolna klasa bazowa ksztaltow kolizji
class AABB;           // Axis-Aligned Bounding Box
class PlaneBV;        // Ksztalt kolizji reprezentujacy plaszczyzne
class OBB;            // Oriented Bounding Box
class CylinderBV;     // Ksztalt kolizji reprezentujacy walec
enum class BoundingShapeType; // Typ ksztaltu kolizji (zdefiniowany w BoundingVolume.h)

/**
 * @class CollisionSystem
 * @brief System odpowiedzialny za wykrywanie kolizji miedzy obiektami w scenie.
 *
 * Uzywa algorytmu Sweep and Prune (SAP) do optymalizacji fazy szerokiej detekcji kolizji,
 * a nastepnie przeprowadza dokladniejsze testy (faza waska) dla potencjalnie
 * kolidujacych par obiektow. System generuje zdarzenia CollisionEvent, ktore
 * moga byc obslugiwane przez inne moduly silnika lub logike gry.
 */
class CollisionSystem {
public:
    /**
     * @brief Konstruktor domyslny.
     * Inicjalizuje system kolizji.
     */
    CollisionSystem();

    /**
     * @brief Destruktor.
     * Czysci liste obiektow kolidujacych i inne dane wewnetrzne.
     */
    ~CollisionSystem();

    /**
     * @brief Dodaje obiekt kolidujacy do systemu.
     * Obiekt zostanie uwzgledniony w nastepnej aktualizacji systemu kolizji.
     * @param collidable Wskaznik do obiektu implementujacego interfejs ICollidable.
     */
    void addCollidable(ICollidable* collidable);

    /**
     * @brief Usuwa obiekt kolidujacy z systemu.
     * Obiekt nie bedzie juz sprawdzany pod katem kolizji.
     * @param collidable Wskaznik do obiektu do usuniecia.
     */
    void removeCollidable(ICollidable* collidable);

    /**
     * @brief Aktualizuje system kolizji i wykrywa kolizje.
     * Ta metoda powinna byc wywolywana w kazdej klatce gry.
     * Przeprowadza algorytm Sweep and Prune, a nastepnie faze waska
     * dla zidentyfikowanych par. Generuje zdarzenia CollisionEvent.
     * @param eventManager Wskaznik do menedzera zdarzen, uzywany do rozglaszania zdarzen kolizji.
     */
    void update(EventManager* eventManager);

private:
    /**
     * @struct Endpoint
     * @brief Reprezentuje punkt koncowy (minimalny lub maksymalny)
     * granicy AABB obiektu na jednej z osi (w tym systemie na osi X).
     * Uzywane przez algorytm Sweep and Prune.
     */
    struct Endpoint {
        ICollidable* collidable; ///< Wskaznik do obiektu kolidujacego powiazanego z tym punktem.
        float value;             ///< Wartosc (pozycja) punktu na osi.
        bool isMin;              ///< True, jesli to jest punkt minimalny (poczatek AABB), false jesli maksymalny (koniec AABB).

        /**
         * @brief Operator porownania dla sortowania punktow koncowych.
         * Sortuje punkty glownie wedlug ich wartosci na osi. Jesli wartosci sa rowne,
         * punkty minimalne sa umieszczane przed maksymalnymi, co jest wazne
         * dla poprawnej obslugi stykajacych sie lub nakladajacych sie obiektow.
         * @param other Inny Endpoint do porownania.
         * @return True, jesli ten Endpoint powinien byc przed 'other', false w przeciwnym razie.
         */
        bool operator<(const Endpoint& other) const {
            if (value != other.value) {
                return value < other.value;
            }
            return isMin && !other.isMin; // Minimalne punkty przed maksymalnymi przy tej samej wartosci
        }
    };

    /** @brief Wektor przechowujacy wszystkie obiekty kolidujace zarzadzane przez system. */
    std::vector<ICollidable*> m_collidables;

    /**
     * @brief Wektor punktow koncowych AABB wszystkich obiektow na osi X.
     * Uzywany przez algorytm Sweep and Prune do identyfikacji potencjalnych kolizji.
     * Kazdy obiekt dodaje dwa punkty koncowe (min i max) do tej listy.
     */
    std::vector<Endpoint> m_endpointsX;

    /**
     * @brief Mapa przechowujaca (cache'ujaca) globalne AABB dla obiektow w biezacej klatce.
     * Kluczem jest wskaznik do obiektu ICollidable, a wartoscia jest jego obliczone AABB.
     * Pomaga uniknac wielokrotnego obliczania AABB dla tego samego obiektu w jednej klatce.
     */
    std::map<ICollidable*, AABB> m_cachedAABBs;

    /**
     * @brief Metoda pomocnicza do obliczania globalnego AABB dla danego BoundingVolume.
     * Potrafi obsluzyc rozne typy BoundingVolume (AABB, OBB, CylinderBV, PlaneBV)
     * i zwrocic AABB otaczajace je w przestrzeni swiata.
     * @param bv Wskaznik do obiektu BoundingVolume.
     * @return Obliczone AABB w przestrzeni swiata. Dla plaszczyzn zwraca "odwrocone" AABB.
     */
    AABB getWorldAABB(const BoundingVolume* bv);

    // --- Metody pomocnicze do sprawdzania kolizji (faza wąska) ---
    // Kazda z tych metod implementuje test kolizji dla okreslonej pary typow ksztaltow.
    bool checkCollision(const AABB* aabb1, const AABB* aabb2);
    bool checkCollision(const AABB* aabb, const PlaneBV* plane);
    bool checkCollision(const AABB* aabb, const OBB* obb);
    bool checkCollision(const AABB* aabb, const CylinderBV* cylinder);
    bool checkCollision(const PlaneBV* plane1, const PlaneBV* plane2);
    bool checkCollision(const PlaneBV* plane, const OBB* obb);
    bool checkCollision(const PlaneBV* plane, const CylinderBV* cylinder);
    bool checkCollision(const OBB* obb1, const OBB* obb2);
    bool checkCollision(const OBB* obb, const CylinderBV* cylinder);
    bool checkCollision(const CylinderBV* cyl1, const CylinderBV* cyl2);
    // TODO: Rozwazyc dodanie testu kolizji dla SPHERE, jesli zostanie dodany taki typ ksztaltu.

    /**
     * @brief Przeprowadza test kolizji w fazie waskiej miedzy dwoma obiektami.
     * Na podstawie typow ich BoundingVolume, wybiera i wywoluje odpowiednia
     * specjalizowana funkcje checkCollision. Normalizuje kolejnosc argumentow
     * (mniejszy BoundingShapeType jako pierwszy), aby zredukowac liczbe
     * potrzebnych implementacji funkcji checkCollision.
     * @param collidableA Wskaznik do pierwszego obiektu kolidujacego.
     * @param collidableB Wskaznik do drugiego obiektu kolidujacego.
     * @return True, jesli wykryto kolizje, false w przeciwnym wypadku.
     */
    bool performNarrowPhaseCheck(ICollidable* collidableA, ICollidable* collidableB);
};

#endif // COLLISION_SYSTEM_H