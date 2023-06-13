#include "main.h"
#include "watek_komunikacyjny.h"

/* wątek komunikacyjny; zajmuje się odbiorem i reakcją na komunikaty */
void *startKomWatek(void* arg)
{
	Constructor* constructor = static_cast<Constructor*>(arg);
    MPI_Status status;
    bool is_message = false;
    packet_t pakiet;
    /* Obrazuje pętlę odbierającą pakiety o różnych typach */
    while (true) {
		debug("czekam na recv");
        MPI_Recv(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

		// Update zegar lamporta
		incrementLamportClock(pakiet.ts);

        switch (status.MPI_TAG) {
			case REQUEST_CRITICAL: 
				debug("Dostalem REQUEST CRTICAL od %d z prośbą o %d części", pakiet.src, pakiet.data);
				constructor->partsRequests.push(PartsRequest{pakiet.src, pakiet.requestTs, pakiet.data}); // Dodaj request do kolejki części
				sendPacket( 0, status.MPI_SOURCE, MessageType::ACK_CRITICAL);
			break;

			case REQUEST_FIGHT:
				debug("Dostalem REQUEST FIGHT od %d z ts = %d", pakiet.src, pakiet.requestTs);
				constructor->fightsRequest.push(FightRequest{pakiet.src, pakiet.requestTs});  // Dodaj request do kolejki walk
				sendPacket(0, status.MPI_SOURCE, MessageType::ACK_FIGHT);
			break;

			case REQUEST_BEGIN_FIGHT:
				debug("Dostalem REQUEST BEGIN FIGHT od %d", pakiet.src);
				constructor->fightTime = pakiet.data; // pakiet.data zawiera losowo wygenerowany czas(żeby zapewnić ten sam czas czekania dla obydwu konstruktorów)
				changeState(FIGHTING);
				sendPacket(0, status.MPI_SOURCE, MessageType::ACK_BEGIN_FIGHT);
			break;

			case ACK_CRITICAL: 
				pthread_mutex_lock(&ackCountMut);
				++ackCount;
				pthread_mutex_unlock(&ackCountMut);
				debug("Dostałem ACK CRITICAL od %d, mam już %d", status.MPI_SOURCE, ackCount);
			break;

			case ACK_FIGHT:
				pthread_mutex_lock(&ackCountMut);
				++ackCount;
				pthread_mutex_unlock(&ackCountMut);
				debug("Dostałem ACK FIGHT od %d, mam już %d", status.MPI_SOURCE, ackCount);
			break;

			case ACK_BEGIN_FIGHT:
				debug("Dostalem ACK BEGIN FIGHT od %d", pakiet.src);
				changeState(FIGHTING);
			break;

			case RELEASE_CRITICAL:
				// pthread_mutex_lock(&constructor->partsMut);
		    	// constructor->partsAvailable -= pakiet.data; // pakiet.data zawiera liczbę pobranych części
				// pthread_mutex_unlock(&constructor->partsMut);
				debug("Dostałem RELEASE CRITICAL od %d, dostępne części: %d", status.MPI_SOURCE, constructor->partsAvailable);
			break;

			case RELEASE_RETURN_PARTS: {
				debug("Dostalem RELEASE RETURN PARTS od %d", pakiet.src);
				// pthread_mutex_lock(&constructor->partsMut);
		    	// constructor->partsAvailable += pakiet.data; // pakiet.data zawiera liczbę zwróconych części
				// pthread_mutex_unlock(&constructor->partsMut);

				// Odejmij wartość zwróconych części z tego samego requesta
				int tsOfPartRequest = pakiet.requestTs;
				pthread_mutex_lock(&constructor->partsQueueMut);
				int size = constructor->partsRequests.size(), count = 0;
				while(count++ < size){
					PartsRequest req = constructor->partsRequests.front();
					if(req.lamport == tsOfPartRequest){
						req.parts -= pakiet.data;
						if(req.parts <= 0){
							constructor->partsRequests.pop();
							break;
						}
					}
					constructor->partsRequests.pop();
					constructor->partsRequests.push(req);
				}
				pthread_mutex_unlock(&constructor->partsQueueMut);
			break;
			}

			case RELEASE_FIGHT_REQUEST: {
				// println("Dostałem prośbę usunięcia requesta z %d ts = %d", pakiet.src, pakiet.requestTs);
				removeFightRequest(constructor, pakiet.src, pakiet.requestTs);
			break;
			}
			default:
				break;
        }
    }
}

// Usuń request(y?) z podanym id
void removeFightRequest(Constructor* constructor, int id, int ts){
	pthread_mutex_lock(&constructor->fightQueueMut);
	int size = constructor->fightsRequest.size();
	for(int index = 0; index < size; ++index){
		auto request = constructor->fightsRequest.front();
		constructor->fightsRequest.pop();
		if(request.id != id && request.lamport != ts)
			constructor->fightsRequest.push(request);
		else println("Usuwam request od %d z ts = %d", request.id, request.lamport);
	}
	pthread_mutex_unlock(&constructor->fightQueueMut);
}
