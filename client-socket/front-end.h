//
// Created by Augusto on 14/09/2022.
//

#ifndef DROPBOX_SISOP2_FRONT_END_H
#define DROPBOX_SISOP2_FRONT_END_H
#include <pthread.h>
#include <stdbool.h>

extern pthread_mutex_t isConnectionOpenMutex;

extern int* clientSocket;
extern int* syncDirSocket;
extern int* syncListenSocket;

extern char serverIp[15];

void connectToServer(int* connSocket, int port);
#endif //DROPBOX_SISOP2_FRONT_END_H
