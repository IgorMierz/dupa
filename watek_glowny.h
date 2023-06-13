#ifndef WATEK_GLOWNY_H
#define WATEK_GLOWNY_H

/* pętla główna aplikacji: zmiany stanów itd */
void mainLoop(Constructor* constructor);
void removePartsRequest(Constructor* constructor);
int findFightRequestTime(Constructor* constructor); // w takim projekcie nie warto używać template :)
int findPartsRequestTime(Constructor* constructor);

void returnParts(Constructor* constructor);
#endif
