#include "main.h"
#include "watek_glowny.h"

#include <queue>
#include <algorithm>

void mainLoop(Constructor* constructor)
{
    srandom(rank);
    int perc;

    while (true) {
	returnParts(constructor);
	pthread_mutex_lock(&lamportMut);
	switch (state) {
	    case WAITING_FOR_ACCESS: 
			perc = random() % 100;
			if (perc < STATE_CHANGE_PROB) {
				// constructor->partsNeeded = rand() % partsNeededModulo + 1;
				 constructor->partsNeeded = 5;
				println("Ubiegam się o sekcję krytyczną. Liczba potrzebnych części: %d", constructor->partsNeeded);
				
				packet_t *pkt = (packet_t*)malloc(sizeof(packet_t));
				pkt->src = constructor->id;
				pkt->requestTs = lamport;
				pkt->data = constructor->partsNeeded;
				pthread_mutex_lock(&ackCountMut);
				ackCount = 0; // Reset liczby ACK
				pthread_mutex_unlock(&ackCountMut);

				for(int i=0; i< size; ++i){
					if(i != constructor->id) // i != rank też działa
						sendPacket(pkt, i, REQUEST_CRITICAL);
				}

				// pthread_mutex_lock(&partsQueueMut); Tu na 99.9% niepotrzebne
				constructor->partsRequests.push(PartsRequest{rank, lamport, constructor->partsNeeded});
				constructor->lastPartRequestTs = lamport;
				// pthread_mutex_unlock(&partsQueueMut);
				changeState(State::WAITING_FOR_PARTS);
				free(pkt);
			} 
		break;
	    case WAITING_FOR_PARTS:
			debug("Czekam na wszystkie ACK aby pobrać dane. Obecnie: %d, a potrzebne %d", ackCount, size - 1);
			if (ackCount >= (size - 1)) {
				// pthread_mutex_lock(&ackCountMut); // Prawdopodobnie nie jest potrzebny
				int lowerSums = 0;
				int myRequestTime = findPartsRequestTime(constructor);
				if(myRequestTime == -1) break; // nie powinno się zdarzyć 

				std::queue<PartsRequest> tmp = constructor->partsRequests;
				while(!tmp.empty()){
					PartsRequest req = tmp.front();
					tmp.pop();

					if(req.lamport < myRequestTime) // Sumuj requesty z wcześniejszym timestampem (nie bardzo cost efficient ale działa 8)
						lowerSums += req.parts;
				}
				debug("Potrzebne części %d, dostepne: %d", constructor->partsNeeded, constructor->partsAvailable - lowerSums);
				if(constructor->partsAvailable - lowerSums - constructor->partsNeeded >= 0)
					changeState(COLLECTING_PARTS);
				// pthread_mutex_unlock(&ackCountMut);
			}
		break;
	    case COLLECTING_PARTS: {
			println("Pobieram %d części z sekcji krytycznej", constructor->partsNeeded);

			// pthread_mutex_lock(&constructor->partsMut); // Potrzebne?
		    // constructor->partsAvailable -= constructor->partsNeeded;
			// pthread_mutex_unlock(&constructor->partsMut);

		    println("Wychodzę z sekcji krytycznej")
		    packet_t *pkt = (packet_t*)malloc(sizeof(packet_t));
			pkt->src = constructor->id;
		    pkt->data = constructor->partsNeeded;
			pkt->requestTs = lamport;
			// Nie potrzebujemy czekać na ACK bo i tak jesteśmy w kolejce
		    for(int i = 0; i< size; ++i){
				if (i != constructor->id)
					sendPacket(pkt, i, RELEASE_CRITICAL);
			}
		    changeState(BUILDING_ROBOT);
		    free(pkt);
		break;
		}
		case BUILDING_ROBOT: {
			println("Buduje robota!");
			sleep(rand() % BUILD_TIME_MAX_WAIT + 1);
			println("Robot gotowy!");

			pthread_mutex_lock(&ackCountMut);
			ackCount = 0; // Reset liczby ACK
			pthread_mutex_unlock(&ackCountMut);

			packet_t* pkt = (packet_t*)malloc(sizeof(packet_t));
			pkt->src = constructor->id;
			pkt->requestTs = lamport;

			constructor->fightsRequest.push(FightRequest{constructor->id, lamport}); // Dodaj własny request do swojej kolejki
			// pkt->data niepotrzebne bo nie mamy co wysłać i tak
			for(int i = 0; i < size; ++i){
				if(i != constructor->id){
					// println("Wysyłam REQUEST FIGHT do %d z %d", i, pkt->requestTs);
					sendPacket(pkt, i, REQUEST_FIGHT); // wyślij requesty z prośbą o walkę
				}
			}

			changeState(READY_TO_FIGHT);
			free(pkt);
		break;
		}
		case READY_TO_FIGHT:
			debug("Czekam na wszystkie ACK aby zacząć walkę. Obecnie: %d, a potrzebne %d", ackCount, size - 1);
			if(ackCount == size - 1){ // Otrzymałem ack od wszystkich innych konstruktorów
				changeState(WAITING_FOR_FIGHT); 
				println("Dostałem wszystkie ACK aby zacząć walkę.");
			}
		break;
		case WAITING_FOR_FIGHT: {
			println("Czekam na walkę")
			int lowerTs = 0; // liczba requestów o walkę z mniejszym ts
			int myRequestTs = findFightRequestTime(constructor);
			if(myRequestTs == -1) break; // nie powinno się zdarzyć 
			
			std::queue<FightRequest> tmp = constructor->fightsRequest;
			
			int size = tmp.size(), count = 0;
			while(count++ < size){
				FightRequest req = tmp.front();
				tmp.pop();
				debug("REQUEST od %d ts: %d ", req.id, req.lamport);
				if(req.lamport < myRequestTs)
					++lowerTs;
				tmp.push(req);
			}
			std::vector<FightRequest> requestsToSort;
			requestsToSort.reserve(size); // Żeby uniknąć niepotrzebnych kopii w pamięci
			while(!tmp.empty()){
				requestsToSort.push_back(tmp.front());
				tmp.pop();
			}
			std::sort(requestsToSort.begin(), requestsToSort.end(), [](const FightRequest& a, const FightRequest& b){ return a.lamport < b.lamport; });
			// for(int i = 0; i < requestsToSort.size(); ++i)
			// 	printf("ID: %d TS: %d", requestsToSort[i].id, requestsToSort[i].lamport);
 			// Jeśli liczba requestów o walkę młodszych od mojego jest nieparzysta to znam swoją parę, w innym przypadku znam tylko inne pary(lub żadne)
			int requestsToRemove = (lowerTs % 2 == 1) ? lowerTs - 1 : lowerTs;
			pthread_mutex_lock(&constructor->fightQueueMut);
			for(int start = requestsToRemove; start < requestsToSort.size(); ++start)
				tmp.push(requestsToSort[start]);

			// Jeśli został jeszcze jeden request przed swoim request to została jeszcze jedna para (swój robot vs robot bezpośrednio nad nim w kolejce)
			if(lowerTs - requestsToRemove > 0 && tmp.size() >= 2){
				FightRequest fightReq = tmp.front();
				tmp.pop();
				FightRequest myReq = tmp.front();
				tmp.pop();

				// wyślij pakiet z prośbą o walkę
				packet_t* pkt = (packet_t*)malloc(sizeof(packet_t));
				pkt->src = constructor->id;
				pkt->requestTs = lamport;
				
				int fightTime = rand() % FIGHT_MAX_WAIT + 1; // czas walki(można też wysłać razem z ack)
				pkt->data = fightTime;
				constructor->fightTime = fightTime;
				constructor->fightRequestTs = pkt->requestTs;
				// println("%d chce walczyć z %d", myReq.id, fightReq.id);
				sendPacket(pkt, fightReq.id, REQUEST_BEGIN_FIGHT);
				changeState(SENT_REQUEST); // nieobsługiwany, po prostu czeka
				free(pkt);
			}
			constructor->fightsRequest = tmp;

			pthread_mutex_unlock(&constructor->fightQueueMut);
		break;
		}
			
		case FIGHTING: {
			// println("%d walczy", constructor->id);
			if(constructor->fightTime < 0) // NIE POWINNO SIĘ ZDARZYĆ!
				constructor->fightTime = rand() % FIGHT_MAX_WAIT + 1;
			sleep(constructor->fightTime);
			changeState(FINISHED_FIGHTING);
		break;
		}

		case FINISHED_FIGHTING: {
			println("%d skończył walkę", constructor->id);
			int toRepair = rand() % (constructor->partsNeeded + 1); // Liczba części do natychmiastowej naprawy(od razu zwrócona do puli części)
			if(constructor->partsNeeded - toRepair > 0){ // Jeśli nie wszystkie części zostały naprawione instant
				pthread_mutex_lock(&constructor->repairVectorMut);
				int repairTime = rand() % REPAIR_MAX_WAIT + 1;
				constructor->repairParts.push_back(RepairParts{constructor->partsNeeded - toRepair, repairTime, constructor->lastPartRequestTs});
				// Posortuj malejąco(łatwiej usuwać elementu z końca)
				std::sort(constructor->repairParts.begin(), constructor->repairParts.end(), [](const RepairParts& a, const RepairParts& b){
					return a.timeOfRepair > b.timeOfRepair;
				});
				pthread_mutex_unlock(&constructor->repairVectorMut);
			}

			packet_t* pkt = (packet_t*)malloc(sizeof(packet_t));
			pkt->src = constructor->id; 
			pkt->data = constructor->partsNeeded;
			pkt->requestTs = lamport;
			for(int i = 0; i < size; ++i){
				if(i != constructor->id)
					sendPacket(pkt, i, RELEASE_RETURN_PARTS);
			}
			println("%d zwraca %d części", constructor->id, toRepair);
			changeState(WAITING_FOR_ACCESS);
			free(pkt);
		break;
		}
	    default: 
			break;
        }
		pthread_mutex_unlock(&lamportMut);
        sleep(SEC_IN_STATE);
    }
}

// Usuń request z swoim id
void removePartsRequest(Constructor* constructor){
	pthread_mutex_lock(&constructor->partsQueueMut); // Chyba nie trzeba
	int size = constructor->partsRequests.size(), count = 0;
	while(count++ < size){
		PartsRequest req =  constructor->partsRequests.front();
		constructor->partsRequests.pop();
		if(req.id == constructor->id)
			break;
		 constructor->partsRequests.push(req);
	}
	pthread_mutex_unlock(&constructor->partsQueueMut);
}

// Znajdź swój request w kolejce walk(można przerobić z uniwersalną kolejką jako parametr)
int findFightRequestTime(Constructor* constructor){
	pthread_mutex_lock(&constructor->fightQueueMut); // Chyba nie trzeba
	int size = constructor->fightsRequest.size(), count = 0, res = -1;
	while(count++ < size){
		FightRequest req = constructor->fightsRequest.front();
		if(req.id == constructor->id){
			res = req.lamport;
			break;
		}
		constructor->fightsRequest.pop();
		constructor->fightsRequest.push(req);
	}
	pthread_mutex_unlock(&constructor->fightQueueMut);
	return res;
}

int findPartsRequestTime(Constructor* constructor){
	pthread_mutex_lock(&constructor->partsQueueMut); // Chyba nie trzeba
	int size = constructor->partsRequests.size(), count = 0, res = -1;
	while(count++ < size){
		PartsRequest req = constructor->partsRequests.front();
		if(req.id == constructor->id){
			res = req.lamport;
			break;
		}
		constructor->partsRequests.pop();
		constructor->partsRequests.push(req);
	}
	pthread_mutex_unlock(&constructor->partsQueueMut);
	return res;
}


void returnParts(Constructor* constructor){
	pthread_mutex_lock(&constructor->repairVectorMut);
	// for(auto& part: parts){}
	for(int i = 0; i < constructor->repairParts.size(); ++i)
		constructor->repairParts[i].timeOfRepair -= 1; // Zmniejsz czas do skońćzenia naprawy wszystkich części
	
	// Zwróć wszystkie części, który zostały własnie naprawione
	while(constructor->repairParts.size() > 0 &&
	 		constructor->repairParts.back().timeOfRepair <= 0){
		RepairParts parts = constructor->repairParts.back();
		constructor->repairParts.pop_back();


		packet_t* pkt = (packet_t*)malloc(sizeof(packet_t));
		pkt->src = constructor->id;
		pkt->data = parts.partsToRepair; // Przekaź liczbę zwróconych części
		pkt->requestTs = parts.partRequest;

		println("%d zwraca %d części", constructor->id, parts.partsToRepair);
		for(int i = 0; i < size; ++i){
			if(i != constructor->id)
				sendPacket(pkt, i, RELEASE_RETURN_PARTS);
		}

		free(pkt);
	}
	pthread_mutex_unlock(&constructor->repairVectorMut);
}