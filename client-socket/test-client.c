#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../server/server_functions.h"
#include "./front-end.h"
#include "../file-control/file-handler.h"
#include "./test-client.h"

#define SA struct sockaddr
char username[USERNAMESIZE];

void rand_str(char *dest, size_t length);

char path[KBYTE];
char rootPath[KBYTE];

pthread_mutex_t syncDirSem;
pthread_t listenSyncThread;
pthread_t clientThread;

received_file_list *filesReceived = NULL;

sync_dir_conn* socketConn;
pthread_mutex_t socketConnMutex;

pthread_cond_t exitCond;

char* sessionCode;

void startWatchDir() {


    pthread_t syncDirThread;
    watch_dir_argument* argument = calloc(1, sizeof(watch_dir_argument));
    argument->dirPath = path;
    argument->socketConnList = socketConn;
    argument->userSem = &syncDirSem;
    argument->filesReceived = filesReceived;

    pthread_create(&syncDirThread, NULL, watchDir, argument);
    pthread_detach(syncDirThread);
}


void startListenForReplicaMessages(){
    pthread_t replicaMessageThread;

    pthread_create(&replicaMessageThread, NULL, listenForReplicaMessage, (void*)&frontEndPort);
    pthread_detach(replicaMessageThread);
}

int main()
{
    pthread_mutex_init(&isConnectionOpenMutex, NULL);

    srand((unsigned int)(time(NULL)));
    sessionCode = calloc(20, sizeof(char));
    rand_str(sessionCode, 18);
    pthread_mutex_init(&syncDirSem, NULL);

    DL_APPEND(filesReceived, createReceivedFile("\n", -1));

    bzero(path, sizeof(path));
    printf("Insira o caminho para a pasta sync_dir\n");
    fgets(path, sizeof(path), stdin);
    path[strcspn(path, "\n")] = 0;

    printf("Enter username: ");
    fgets(username, USERNAMESIZE, stdin);
    username[strcspn(username, "\n")] = 0;
    char ipAddress[15];

    printf("Insira o IP do servidor\n");
    fgets(ipAddress, sizeof(ipAddress), stdin);
    ipAddress[strcspn(ipAddress, "\n")] = 0;

    bzero(serverIp, sizeof(serverIp));

    strcpy(serverIp, ipAddress);

    printf("Insira a porta para mensagens do servidor\n");
    fgets(ipAddress, sizeof(ipAddress), stdin);
    ipAddress[strcspn(ipAddress, "\n")] = 0;

    sscanf(ipAddress, "%d", &frontEndPort);

    clientSocket = calloc(1, sizeof(int));
    syncDirSocket = calloc(1, sizeof(int));
    syncListenSocket = calloc(1, sizeof(int));

    startListenForReplicaMessages();


    int created = startClient(true);
    pthread_detach(clientThread);

    if(created != 0) {
        exit(OUTOFSYNCERROR);
    }

    startWatchDir();


    pthread_cond_wait(&exitCond, &syncDirSem);
}

void rand_str(char *dest, size_t length) {
    char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}