/**
* @file ICollidable.h
* @brief Definicja interfejsu ICollidable.
*
* Plik ten zawiera deklarację interfejsu ICollidable, który musi być
* implementowany przez wszystkie obiekty mogące brać udział w kolizjach.
*/
#ifndef ICOLLIDABLE_H
#define ICOLLIDABLE_H

// Pelne dolaczenie BoundingVolume.h jest potrzebne, jesli ColliderType
// jest zdefiniowany wewnatrz BoundingVolume.h (np. jako enum class).
// Jesli ColliderType jest prostym typem lub enumem zdefiniowanym globalnie
// lub w innym pliku, mozna by rozwazyc inne podejscie.
// Zakladamy, ze BoundingVolume.h dostarcza definicji ColliderType.
#include "BoundingVolume.h" // Dla definicji BoundingVolume i ColliderType

// Deklaracja wyprzedzajaca nie jest juz scisle potrzebna, jesli BoundingVolume.h jest dolaczony powyzej
// class BoundingVolume;

/**
 * @interface ICollidable
 * @brief Interfejs dla obiektow, ktore moga uczestniczyc w systemie detekcji kolizji.
 *
 * Definiuje podstawowy kontrakt dla obiektow, ktore posiadaja ksztalt kolizyjny
 * (BoundingVolume) i moga byc sprawdzane pod katem kolizji z innymi obiektami.
 */
class ICollidable {
public:
    /**
     * @brief Wirtualny destruktor.
     * Niezbedny dla poprawnego zarzadzania pamiecia przy polimorfizmie.
     */
    virtual ~ICollidable() = default;

    /**
     * @brief Zwraca wskaznik do modyfikowalnego ksztaltu kolizji (Bounding Volume) obiektu.
     * Implementacja tej metody powinna zwrocic wlasciwy obiekt BoundingVolume
     * powiazany z tym obiektem kolidujacym.
     * @return Wskaznik do BoundingVolume.
     */
    virtual BoundingVolume* getBoundingVolume() = 0;

    /**
     * @brief Zwraca staly wskaznik do ksztaltu kolizji (Bounding Volume) obiektu.
     * Wersja const metody getBoundingVolume().
     * @return Staly wskaznik do BoundingVolume.
     */
    virtual const BoundingVolume* getBoundingVolume() const = 0;

    /**
     * @brief Zwraca typ kolidera obiektu.
     * Typ kolidera (np. statyczny, dynamiczny, trigger) okresla, jak obiekt
     * zachowuje sie w systemie kolizji.
     * @return Wartosc typu ColliderType.
     * @note Ta metoda posiada domyslna implementacje zwracajaca ColliderType::STATIC.
     * Klasy pochodne moga ja nadpisac, aby zwrocic inny typ.
     * Alternatywnie, moglaby byc to metoda czysto wirtualna (`= 0;`),
     * zmuszajac kazda klase implementujaca do zdefiniowania swojego typu.
     */
    virtual ColliderType getColliderType() const {
        return ColliderType::STATIC; // Domyslny typ kolidera
    }

    /**
     * @brief Ustawia, czy kolizje dla tego obiektu sa wlaczone.
     * Obiekty z wylaczonymi kolizjami sa ignorowane przez system detekcji kolizji.
     * @param enabled True, aby wlaczyc kolizje, false, aby wylaczyc.
     */
    virtual void setCollisionsEnabled(bool enabled) = 0;

    /**
     * @brief Sprawdza, czy kolizje dla tego obiektu sa aktualnie wlaczone.
     * @return True, jesli kolizje sa wlaczone, false w przeciwnym wypadku.
     */
    virtual bool collisionsEnabled() const = 0;

    // Opcjonalne metody, ktore mozna dodac w przyszlosci:
    // /**
    //  * @brief Zwraca unikalny identyfikator obiektu gry, do ktorego nalezy ten kolider.
    //  * Moze byc uzyteczne do debugowania lub w logice gry opartej na kolizjach.
    //  * @return Identyfikator obiektu gry.
    //  */
    // virtual unsigned int getGameObjectId() const = 0;

    // /**
    //  * @brief Zwraca wskaznik typu void do obiektu-wlasciciela tego kolidera.
    //  * Pozwala na uzyskanie dostepu do pelnego obiektu gry z poziomu systemu kolizji,
    //  * wymaga odpowiedniego rzutowania.
    //  * @return Wskaznik do obiektu-wlasciciela.
    //  */
    // virtual void* getOwner() const = 0;
};

#endif // ICOLLIDABLE_H
