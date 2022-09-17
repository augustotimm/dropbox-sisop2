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
extern int frontEndPort;



void connectToServer(int* connSocket, int port);
void* listenForReplicaMessage(void* args);

void newConnection(int sockfd, int socketType);
void addSocketConn(int socket,  bool isListener);
int startClient(bool shouldDownloadAll);
#endif //DROPBOX_SISOP2_FRONT_END_H
