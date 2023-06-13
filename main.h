#ifndef MAINH
#define MAINH
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <queue>

#include "util.h"
/* boolean */
#define SEC_IN_STATE 1
#define STATE_CHANGE_PROB 20
#define BUILD_TIME_MAX_WAIT 10
#define FIGHT_MAX_WAIT 10
#define REPAIR_MAX_WAIT 10
#define ROOT 0

/* tutaj TYLKO zapowiedzi - definicje w main.c */
extern int rank;
extern int size;
extern int ackCount;
extern int lamport;
extern int partsNeededModulo;
// extern int partsAvailable;
extern pthread_t threadKom;
extern pthread_mutex_t lamportMut;
extern pthread_mutex_t ackCountMut;
// extern pthread_mutex_t partsMut;
// extern pthread_mutex_t partsQueueMut;
extern State state;

struct PartsRequest {
    int id;
    int lamport;
    int parts;
};

struct FightRequest {
    int id;
    int lamport;
};

struct RepairParts{
    int partsToRepair;
    int timeOfRepair;
    int partRequest;
};

class Constructor {
public:
    Constructor(int id, int partsAvailable){
        this->id = id;
        this->partsAvailable = partsAvailable;
    }
    int id;
    int partsAvailable;
    int partsNeeded;
    int fightTime = -1;
    int lastPartRequestTs; // timestamp ostaniego zapytania o części(potrzebne przy zwracaniu części)
    int fightRequestTs; // timestamp requesta o walkę
    std::queue<PartsRequest> partsRequests;
    std::queue<FightRequest> fightsRequest;
    std::vector<RepairParts> repairParts; // vector bo będzie często sortowany

    pthread_mutex_t partsMut = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t partsQueueMut = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t fightQueueMut = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t repairVectorMut = PTHREAD_MUTEX_INITIALIZER;
};

/* macro debug - działa jak printf, kiedy zdefiniowano
   DEBUG, kiedy DEBUG niezdefiniowane działa jak instrukcja pusta 
   
   używa się dokładnie jak printfa, tyle, że dodaje kolorków i automatycznie
   wyświetla rank

   w związku z tym, zmienna "rank" musi istnieć.

   w printfie: definicja znaku specjalnego "%c[%d;%dm [%d]" escape[styl bold/normal;kolor [RANK]
                                           FORMAT:argumenty doklejone z wywołania debug poprzez __VA_ARGS__
					   "%c[%d;%dm"       wyczyszczenie atrybutów    27,0,37
                                            UWAGA:
                                                27 == kod ascii escape. 
                                                Pierwsze %c[%d;%dm ( np 27[1;10m ) definiuje styl i kolor literek
                                                Drugie   %c[%d;%dm czyli 27[0;37m przywraca domyślne kolory i brak pogrubienia (bolda)
                                                ...  w definicji makra oznacza, że ma zmienną liczbę parametrów
                                            
*/
#ifdef DEBUG
#define debug(FORMAT,...) printf("%c[%d;%dm [%d, %d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, lamport, ##__VA_ARGS__, 27,0,37);
#else
#define debug(...) ;
#endif

// makro println - to samo co debug, ale wyświetla się zawsze
#define println(FORMAT,...) printf("%c[%d;%dm [%d, %d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, lamport, ##__VA_ARGS__, 27,0,37);


#endif
