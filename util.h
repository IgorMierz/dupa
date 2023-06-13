#ifndef UTILH
#define UTILH
#include "main.h"

/* typ pakietu */
typedef struct {
    int ts;       /* timestamp (zegar lamporta */
    int src;  

    int data;     /* przykładowe pole z danymi; można zmienić nazwę na bardziej pasującą */
    int requestTs; // timestamp 
} packet_t;
/* packet_t ma trzy pola, więc NITEMS=3. Wykorzystane w inicjuj_typ_pakietu */
#define NITEMS 4

/* Typy wiadomości */
/* TYPY PAKIETÓW */
// #define ACK     1
// #define REQUEST_CRITICAL 2
// #define REQUEST_FIGHT 3
// #define RELEASE 4
// #define APP_PKT 5
// #define FINISH  6

enum State {
    WAITING_FOR_ACCESS, // czeka na dostep do sekcji krytcznej
    WAITING_FOR_PARTS, // czeka na czesci
    COLLECTING_PARTS, // jest w sekcji
    BUILDING_ROBOT, // był w sekcji i teraz buduje robota przez x czasu
    READY_TO_FIGHT, // gotowy do walki i czeka na ACK
    WAITING_FOR_FIGHT, // czeka na walkę
    SENT_REQUEST, // wysłał prośbę do walki i czeka na ACK
    FIGHTING, // walczy
    FINISHED_FIGHTING // skończył walkę i chce oddać części
};

enum MessageType {
    ACK_FIGHT, // ACK przy prośbie o dołączenie do kolejki walk
    ACK_CRITICAL, // ACK przy prośbie o dostęp do sekcji krytycznej
    ACK_BEGIN_FIGHT, // ACK przy prośbię o rozpoczęcie walki
    REQUEST_CRITICAL, // REQUEST o dostęp do sekcji krytycznej
    REQUEST_FIGHT, // REQUEST o gotowości do walki (dodanie do kolejki walk)
    REQUEST_BEGIN_FIGHT, // REQUEST o walkę z konstrukotrem bezpośrednio nad w kolejce walk
    RELEASE_CRITICAL, // RELEASE informujący o pobraniu danych z kolejki
    RELEASE_FIGHT_REQUEST, // Usuń fight request z kolejki
    RELEASE_RETURN_PARTS // RELEASE informujący o zwróceniu części
};

extern MPI_Datatype MPI_PAKIET_T;
void inicjuj_typ_pakietu();
void incrementLamportClock(int lamport);
/* wysyłanie pakietu, skrót: wskaźnik do pakietu (0 oznacza stwórz pusty pakiet), do kogo, z jakim typem */
void sendPacket(packet_t *pkt, int destination, int tag);

extern pthread_mutex_t stateMut;
/* zmiana stanu, obwarowana muteksem */
void changeState(State newState);
#endif
