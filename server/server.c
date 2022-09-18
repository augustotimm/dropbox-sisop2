//
// Created by augusto on 03/08/2022.
//

#include "server.h"
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include "user.h"
#include "server_functions.h"
#include "./replica-manager/replica-manager.h"
#include <arpa/inet.h>



#define LIVENESSPORT 9997
#define MAX 2048



user_list* connectedUserListHead = NULL;
replica_info_list* replicaList = NULL;
bool isPrimary = false;
int electionValue = -1;
int electionPort = -1;

pthread_mutex_t startElectionMutex;
pthread_cond_t electionFinished;

pthread_mutex_t waitForPrimaryMutex;
pthread_cond_t primaryIsRunning;


bool isElectionRunning = false;
int backupsReady = 0;

char rootPath[KBYTE];

pthread_cond_t backupsReadyCond;
pthread_mutex_t backupsReadyMutex;

pthread_cond_t closedUserConnection;
pthread_mutex_t connectedUsersMutex;

pthread_mutex_t connectedReplicaListMutex;

pthread_mutex_t backupConnectionMutex;

backup_conn_list* backupConnectionList = NULL;

pthread_t* electionSocketThread = NULL;

pthread_t* connectionToPrimary = NULL;


struct new_connection_argument {
    int socket;
    struct in_addr ipAddr;
    char* ipString;
};

void connectUser(int socket, char* username, char* sessionCode, char* ipAddr, int port);
int connectSyncDir(int socket, char* username, char* sessionCode);
int connectSyncListener(int socket, char*username,  char* sessionCode);
void* newBackupConnection(void* args);
void* listenElectionMessages();

void* clientListen(void* voidArg)
{
    client_thread_argument* argument = (client_thread_argument*) voidArg;
    char* path = argument->clientDirPath;
    int socket = argument->socket;

    char currentCommand[13];
    bzero(currentCommand, sizeof(currentCommand));



    recv(socket, currentCommand, sizeof(currentCommand), 0);
    if(strcmp(currentCommand, endCommand) != 0) {
        printf("Connection out of sync\n");
        printf("[clientListen] Expected end command signal but received: %s\n\n", currentCommand);
        return NULL;
    }

    listenForSocketMessage(socket, path, argument->user, true, backupConnectionList, &backupConnectionMutex);

    close(socket);
    *argument->isThreadComplete = true;
    free(argument);
    pthread_cond_signal(&closedUserConnection);

}

void writeMessageToSocket(int socket, char* message) {
    write(socket, message, strlen(message)+1);
}

void* newConnection(void* arg) {
    struct new_connection_argument *argument = (struct new_connection_argument*) arg;
    int socket = argument->socket;
    char newSocketType[USERNAMESIZE];
    char username[USERNAMESIZE];
    bzero(newSocketType, sizeof(newSocketType));
    recv(socket, newSocketType, sizeof(newSocketType), 0);
    write(socket, &endCommand, sizeof(endCommand));

    bzero(username, USERNAMESIZE);
    recv(socket, username, USERNAMESIZE, 0);
    write(socket, &endCommand, sizeof(endCommand));

    char sessionCode[USERNAMESIZE];
    bzero(sessionCode, USERNAMESIZE);
    recv(socket, sessionCode, USERNAMESIZE, 0);
    write(socket, &endCommand, sizeof(endCommand));

    printf("\nNew Connection type: %s", newSocketType);
    if(strcmp(newSocketType, socketTypes[CLIENTSOCKET]) == 0) {
        int port = 0;
        bzero(newSocketType, sizeof(newSocketType));
        recv(socket, newSocketType, sizeof(newSocketType), 0);
        write(socket, &endCommand, sizeof(endCommand));

        sscanf(newSocketType, "%d", &port);

        connectUser(socket, username, sessionCode, argument->ipString, port);
        free(argument->ipString);
    }
    if(strcmp(newSocketType, socketTypes[SYNCSOCKET]) == 0) {
        connectSyncDir(socket, username, sessionCode);
    }
    if(strcmp(newSocketType, socketTypes[SYNCLISTENSOCKET]) == 0) {
        connectSyncListener(socket, username, sessionCode);
    }
}

int connectSyncListener(int socket, char*username, char* sessionCode) {
    char* path = strcatSafe(rootPath, username);
    char* dirPath = strcatSafe(path, "/");
    free(path);

    user_list *user = findUser(username);

    addNewSocketConn(&user->user, socket, sessionCode, true);


    listenForSocketMessage(socket, dirPath, &user->user, true, backupConnectionList, &backupConnectionMutex);
    close(socket);
}

int connectSyncDir(int socket, char* username, char* sessionCode) {
    user_list* user = findUser(username);
    if(user == NULL) {
        // sync dir connection of user logged out
        close(socket);
        return -1;
    }
    // uploadAllFiles(socket, &user->user);
    addSyncDir(socket, &user->user, sessionCode);

}

void connectUser(int socket, char* username, char* sessionCode, char* ipAddr, int port) {
    if(startUserSession(username, socket, ipAddr, port, sessionCode) != 0) {
        writeMessageToSocket(socket, "FALSE");
        close(socket);
    }
    backup_conn_list *elt = NULL;
    pthread_mutex_lock(&backupConnectionMutex);

    char buff[20];
    bzero(buff, sizeof(buff));
    DL_FOREACH(backupConnectionList, elt) {
        recv(elt->socket, buff, sizeof(buff), 0);
        if(strcmp(buff, commands[WAITING]) != 0) {
            printf("expected waiting command");
        }

        write(elt->socket, &commands[USERCONN], sizeof(commands[USERCONN]));

        bzero(buff, sizeof(buff));
        recv(elt->socket, buff, sizeof(buff), 0);
        if(strcmp(buff, endCommand) != 0) {
            printf("[connectUser] expected endCommand command");
        }

        write(elt->socket, username, strlen(username));

        recv(elt->socket, buff, sizeof(buff), 0);
        if(strcmp(buff, endCommand) != 0) {
            printf("[connectUser] expected endCommand command");
        }

        write(elt->socket, ipAddr, strlen(ipAddr));
        recv(elt->socket, buff, sizeof(buff), 0);
        bzero(buff, sizeof(buff));

        sprintf(buff, "%d", port);
        write(elt->socket, buff, strlen(buff));
        recv(elt->socket, buff, sizeof(buff), 0);
        bzero(buff, sizeof(buff));

        write(elt->socket, sessionCode, strlen(sessionCode));
    }
    pthread_mutex_unlock(&backupConnectionMutex);
}

int closeBackupUserSession(char* username, char* sessionCode) {
    backup_conn_list *elt = NULL;
    char buff[20];
    bzero(buff, sizeof(buff));

    pthread_mutex_lock(&backupConnectionMutex);
    DL_FOREACH(backupConnectionList, elt) {
        recv(elt->socket, buff, sizeof(buff), 0);
        if(strcmp(buff, commands[WAITING]) != 0) {
            printf("expected waiting command");
        }

        write(elt->socket, &commands[USERCLOSE], sizeof(commands[USERCLOSE]));

        bzero(buff, sizeof(buff));
        recv(elt->socket, buff, sizeof(buff), 0);
        if(strcmp(buff, endCommand) != 0) {
            printf("[connectUser] expected endCommand command");
        }

        write(elt->socket, username, strlen(username));

        recv(elt->socket, buff, sizeof(buff), 0);
        if(strcmp(buff, endCommand) != 0) {
            printf("[connectUser] expected endCommand command");
        }

        write(elt->socket, sessionCode, strlen(sessionCode));

    }

    pthread_mutex_unlock(&backupConnectionMutex);
    return 0;
}

void* userDisconnectedEvent(void *arg) {
    while(1) {
        user_list* currentUser = NULL, *userTmp = NULL;

        pthread_cond_wait(&closedUserConnection, &connectedUsersMutex);
        DL_FOREACH_SAFE(connectedUserListHead, currentUser, userTmp) {
            pthread_mutex_lock(currentUser->user.userAccessSem);
            for(int i =0; i < USERSESSIONNUMBER; i++) {
                if(isSessionClosed(currentUser->user, i)) {
                    closeBackupUserSession(currentUser->user.username, currentUser->user.clientThread[i]->sessionCode);
                    freeSession(&currentUser->user, i);
                }
            }

            if(!hasSessionOpen(currentUser->user)) {
                DL_DELETE(connectedUserListHead, currentUser);

                freeUserList(currentUser);
            } else
                pthread_mutex_unlock(currentUser->user.userAccessSem);

        }
        pthread_mutex_unlock(&connectedUsersMutex);
    }
}

void* clientConn(void* args) {

    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("CLIENTCONN socket creation failed...\n");
        exit(0);
    }
    else
        printf("CLIENTCONN Socket successfully created..\n");

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        printf("setsockopt(SO_REUSEADDR) failed");

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERVERPORT);

    // Binding newly created socket to given IP and verification
    int socketBind = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (socketBind != 0) {
        printf("CLIENTCONN socket bind failed: %d...\n", socketBind);
        exit(0);
    }
    else
        printf("CLIENTCONN Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("CLIENTCONN Listen failed...\n");
        exit(0);
    }
    else
        printf("CLIENTCONN Server listening..\n");
    len = sizeof(cli);

    int i = 0;
    while( (connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t*)&len))  || i < 5)
    {
        struct in_addr ipAddr = cli.sin_addr;

        printf("\nConnection accepted: Clientconn");
        pthread_t newUserThread;
        struct new_connection_argument *arg = calloc(1, sizeof(struct new_connection_argument));
        arg->socket = connfd;
        arg->ipAddr = ipAddr;
        char *some_addr;

        some_addr = inet_ntoa(ipAddr);


        arg->ipString = strcatSafe(some_addr, "\0");

        pthread_create(&newUserThread, NULL, newConnection, arg);

        pthread_detach(newUserThread);
        //Reply to the client

    }
}

void* syncDirConn(void* args) {

    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        printf("setsockopt(SO_REUSEADDR) failed");

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SYNCPORT);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
        printf("SYNCDIR socket bind failed...\n");
        exit(0);
    }
    else
        printf("SYNCDIR Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("SYNCDIR Listen failed...\n");
        exit(0);
    }
    else
        printf("SYNCDIR Server listening..\n");
    len = sizeof(cli);

    int i = 0;
    while( (connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t*)&len))  || i < 5)
    {
        struct in_addr ipAddr = cli.sin_addr;

        printf("\nConnection accepted: Syncdir conn");
        pthread_t newUserThread;


        struct new_connection_argument *arg = calloc(1, sizeof(struct new_connection_argument));
        arg->socket = connfd;
        arg->ipAddr = ipAddr;

        pthread_create(&newUserThread, NULL, newConnection, arg);

        pthread_detach(newUserThread);
        //Reply to the client
    }
}

void* syncDirListenerConn(void* args) {

    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        printf("setsockopt(SO_REUSEADDR) failed");

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SYNCLISTENERPORT);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
        printf("SYNCLISTENER socket bind failed...\n");
        exit(0);
    }
    else
        printf("SYNCLISTENER Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("SYNCLISTENER Listen failed...\n");
        exit(0);
    }
    else
        printf("SYNCLISTENER Server listening..\n");
    len = sizeof(cli);

    int i = 0;
    while( (connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t*)&len))  || i < 5)
    {
        struct in_addr ipAddr = cli.sin_addr;

        printf("\nConnection accepted: syncDirListenerConn");
        pthread_t newUserThread;


        struct new_connection_argument *arg = calloc(1, sizeof(struct new_connection_argument));
        arg->socket = connfd;
        arg->ipAddr = ipAddr;

        pthread_create(&newUserThread, NULL, newConnection, arg);

        pthread_detach(newUserThread);
        //Reply to the client

    }
}

void broadcastMessageToAllFrontEnd(const char* message){
    pthread_mutex_lock(&connectedUsersMutex);
    user_list *current = NULL;
    DL_FOREACH(connectedUserListHead, current){
        pthread_mutex_lock(current->user.userAccessSem);
        for(int i = 0; i < USERSESSIONNUMBER; i++) {
            if(current->user.clientThread[i] != NULL)
                sendMessageToFrontEnd(*current->user.clientThread[i], message);
        }
        pthread_mutex_unlock(current->user.userAccessSem);
    }

    pthread_mutex_unlock(&connectedUsersMutex);
}

void* backupStartConnectionWithPrimary(void* args) {
    replica_info_list *primaryList = (replica_info_list*) args;
    replica_info_t primary = primaryList->replica;


    struct sockaddr_in servaddr;
    char buff[20];
    bzero(buff, sizeof(buff));


    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(primary.ipAddr);
    servaddr.sin_port = htons(LIVENESSPORT);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Socket to primary replica creation failed...\n");
    }
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        printf("Connection with Primary backup failed\n");
    }

    printf("\nPrimary is alive\n");

    write(sockfd, &socketTypes[BACKUPSOCKET], sizeof(socketTypes[BACKUPSOCKET]));
    recv(sockfd, buff, sizeof(buff), 0);
    if(strcmp(buff, endCommand) != 0) {
        printf("[backupStartConnectionWithPrimary] expected endCommand command");
    }

    backupListenForMessage(sockfd, rootPath, &isElectionRunning);

    pthread_mutex_lock(&startElectionMutex);
    if(!isElectionRunning) {
        broadcastMessageToAllFrontEnd(frontEndCommands[DEAD]);
        pthread_t electionThread;
        pthread_create(&electionThread, NULL, startElection, NULL);
        pthread_detach( electionThread);
        isElectionRunning = true;
    }

    pthread_mutex_unlock(&startElectionMutex);
}

void backupReplicaStart() {
    printf("\n[backupReplicaStart0]");
    printf("\n[backupReplicaStart1]");


    replica_info_list* primary = findPrimaryReplica(replicaList);

    printf("\n[backupReplicaStart2]");

    if(electionSocketThread == NULL) {
        printf("\n[backupReplicaStart3]");

        printf("\n[backupReplicaStart]");

        electionSocketThread = (pthread_t*) calloc(1, sizeof(pthread_t));
        pthread_create(electionSocketThread, NULL, listenElectionMessages, NULL);
        pthread_detach(*electionSocketThread);
    }

    printf("\n[backupReplicaStart4]");

    pthread_t backupReadyThread;
    pthread_create(&backupReadyThread, NULL, sendBackupReadyMessage, NULL);
    pthread_detach(backupReadyThread);

    pthread_cond_wait(&primaryIsRunning, &waitForPrimaryMutex);
    pthread_mutex_unlock(&waitForPrimaryMutex);
    printf("\n[backupReplicaStart5] Starting connection to primary");

    if(connectionToPrimary == NULL) {
        connectionToPrimary = (pthread_t*) calloc(1, sizeof(pthread_t));
    }   else {
        free(connectionToPrimary);
        connectionToPrimary = (pthread_t*) calloc(1, sizeof(pthread_t));
        printf("\nRestarting connection to primary");
    }

    printf("\n[backupReplicaStart6] primary: %d", primary->replica.electionValue);

    pthread_create(connectionToPrimary, NULL, backupStartConnectionWithPrimary, (void*) primary);
    pthread_detach(*connectionToPrimary);

    printf("\n[backupReplicaStart] finished");
}

void* newBackupConnection(void* args) {
    struct new_connection_argument *argument = (struct new_connection_argument*) args;
    int socket = argument->socket;

    printf("\nNEW BACKUP CONNECTION:\t");

    char newSocketType[SOCKETTYPESIZE + 1];
    bzero(newSocketType, sizeof(newSocketType));

    recv(socket, newSocketType, sizeof(newSocketType), 0);

    write(socket, &endCommand, strlen(endCommand));

    printf("%s", newSocketType);

    if(strcmp(newSocketType, socketTypes[BACKUPSOCKET]) == 0) {
       backup_conn_list *newConn = (backup_conn_list*) calloc(1, sizeof(backup_conn_list));
       newConn->socket = socket;

       newConn->prev = NULL;
       newConn->next = NULL;

       pthread_mutex_lock(&backupConnectionMutex);
       DL_APPEND(backupConnectionList, newConn);
       pthread_mutex_unlock(&backupConnectionMutex);

       return NULL;
   }
    if(strcmp(newSocketType, socketTypes[ELECTIONSOCKET]) == 0) {
        char buff[20];
        bzero(buff, sizeof(buff));
        recv(socket, buff, sizeof(buff), 0);
        int replicaElectionValue;
        sscanf(buff, "%d", &replicaElectionValue);

        if( replicaElectionValue < electionValue) {
            write(socket, &endCommand, strlen(endCommand));
        }
        else{
            write(socket, &continueCommand, strlen(continueCommand));
        }

        close(socket);

        pthread_mutex_lock(&startElectionMutex);
        if(!isElectionRunning && !isPrimary) {
            pthread_t electionThread;
            pthread_create(&electionThread, NULL, startElection, NULL);
            pthread_detach( electionThread);
            isElectionRunning = true;
            pthread_cancel(*connectionToPrimary);
        }
        pthread_mutex_unlock(&startElectionMutex);

        printf("\nELECTION MESSAGE answered");
        return NULL;

    }
    if(strcmp(newSocketType, socketTypes[ELECTIONCOORDSOCKET]) == 0) {
        char buff[20];
        bzero(buff, sizeof(buff));
        recv(socket, buff, sizeof(buff), 0);
        int replicaElectionValue;
        sscanf(buff, "%d", &replicaElectionValue);

        printf("\n----[ELECTIONCOORDSOCKET]startElectionMutex locked----\n");
        pthread_mutex_lock(&startElectionMutex);
        isElectionRunning = true;
        pthread_mutex_unlock(&startElectionMutex);
        printf("\n----[ELECTIONCOORDSOCKET]startElectionMutex unlocked----\n");


        printf("\n----[ELECTIONCOORDSOCKET]connectedReplicaListMutex locked----\n");
        pthread_mutex_lock(&connectedReplicaListMutex);
        deletePrimary();
        pthread_mutex_unlock(&connectedReplicaListMutex);
        printf("\n----[ELECTIONCOORDSOCKET]connectedReplicaListMutex unlocked----\n");

        updatePrimary(replicaElectionValue);
        pthread_cond_signal(&electionFinished);
        printf("\n----[ELECTIONCOORDSOCKET]electionFinished cond----\n");



    }
    if(strcmp(newSocketType, socketTypes[NEWPRIMARYSOCKET]) == 0) {
        printf("\nNew primary found me");
        pthread_cond_signal(&primaryIsRunning);
    }
    if(strcmp(newSocketType, socketTypes[RESTARTEDSOCKET]) == 0) {
        printf("\n---RESTARTEDBACKUP---\n");

        backupsReady +=1;
        replica_info_list *elt = NULL;
        int count;
        pthread_mutex_lock(&connectedReplicaListMutex);
        DL_COUNT(replicaList, elt, count);
        pthread_mutex_unlock(&connectedReplicaListMutex);
        if(backupsReady == count) {
            printf("\n---BackupsReady---\n");
            pthread_cond_signal(&backupsReadyCond);
            backupsReady = 0;
        }
    }


    close(socket);
}

void* listenLivenessCheck(void* args) {
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        printf("setsockopt(SO_REUSEADDR) failed");

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(LIVENESSPORT);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
        printf("LIVENESS socket bind failed...\n");
        exit(0);
    }
    else
        printf("LIVENESS Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("LIVENESS Listen failed...\n");
        exit(0);
    }
    else
        printf("LIVENESS Server listening..\n");
    len = sizeof(cli);

    int i = 0;
    while( (connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t*)&len))  || i < 5)
    {
        struct in_addr ipAddr = cli.sin_addr;

        printf("\nConnection accepted: Listenlivenesscheck");
        pthread_t newBackupThread;


        struct new_connection_argument *arg = calloc(1, sizeof(struct new_connection_argument));
        arg->socket = connfd;
        arg->ipAddr = ipAddr;

        pthread_create(&newBackupThread, NULL, newBackupConnection, arg);

        pthread_detach(newBackupThread);
    }
}

void primaryReplicaStart() {

//
    pthread_t listenLivenessCheckThread;
    pthread_create(&listenLivenessCheckThread, NULL, listenLivenessCheck, NULL);
    pthread_detach(listenLivenessCheckThread);

    pthread_t userDisconnectedThread;
    pthread_create(&userDisconnectedThread, NULL, userDisconnectedEvent, NULL);
    pthread_detach(userDisconnectedThread);

    pthread_t syncDirConnThread;
    pthread_create(&syncDirConnThread, NULL, syncDirConn, NULL);
    pthread_detach(syncDirConnThread);

    pthread_t listenSyncDirConnThread;
    pthread_create(&listenSyncDirConnThread, NULL, syncDirListenerConn, NULL);
    pthread_detach(listenSyncDirConnThread);

    pthread_t clientConnThread;
    pthread_create(&clientConnThread, NULL, clientConn, NULL);
    pthread_detach(clientConnThread);

    if(electionSocketThread != NULL){
        pthread_cond_wait(&backupsReadyCond, &backupsReadyMutex);
        pthread_mutex_unlock(&backupsReadyMutex);
    }

    wait(1);
    broadcastNewPrimaryToBackups();
    broadcastMessageToAllFrontEnd(frontEndCommands[NEWPRIMARY]);
}

void* listenElectionMessages() {
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(electionPort);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
        printf("Election socket bind failed...\n");
        exit(0);
    }
    else
        printf("Election Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("Election Listen failed...\n");
        exit(0);
    }
    else
        printf("Election Server listening..\n");
    len = sizeof(cli);

    int i = 0;
    while( (connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t*)&len))  || i < 5)
    {
        struct in_addr ipAddr = cli.sin_addr;

        printf("\nConnection accepted: election message");
        pthread_t newBackupThread;


        struct new_connection_argument *arg = calloc(1, sizeof(struct new_connection_argument));
        arg->socket = connfd;
        arg->ipAddr = ipAddr;

        pthread_create(&newBackupThread, NULL, newBackupConnection, arg);

        pthread_detach(newBackupThread);
    }
}

void intHandler(int dummy) {
    printf("[intHandler]");
    user_list* currentUser = NULL, *userTmp = NULL;

    pthread_mutex_lock(&connectedUsersMutex);
    pthread_mutex_lock(&backupConnectionMutex);
    DL_FOREACH_SAFE(connectedUserListHead, currentUser, userTmp) {
        pthread_mutex_lock(currentUser->user.userAccessSem);
        for(int i =0; i < USERSESSIONNUMBER; i++) {
            if(currentUser->user.clientThread[i] != NULL)
                close(currentUser->user.clientThread[i]->sessionSocket);
        }
        socket_conn_list* currentSocketConn = NULL;
        DL_FOREACH(currentUser->user.syncSocketList, currentSocketConn) {
            close(currentSocketConn->socket);
            close(currentSocketConn->listenerSocket);
        }

    }

    backup_conn_list *replica = NULL;
    DL_FOREACH(backupConnectionList, replica) {
        close(replica->socket);
    }


    exit(0);
}

int main()
{
    signal(SIGINT, intHandler);


    sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);
    connectedUserListHead = NULL;

    bzero(rootPath, sizeof(rootPath));
    printf("Insira o caminho para a pasta raiz onde ficarão as pastas de usuários\n");
    fgets(rootPath, sizeof(rootPath), stdin);
    rootPath[strcspn(rootPath, "\n")] = 0;
    char* configPath = strcatSafe(rootPath, "config.txt");
    replicaList = readConfig( configPath);

    replica_info_list* primary = findPrimaryReplica(replicaList);

    if(!primary) {
        isPrimary = true;
    }

    for(;;){
        if (isPrimary) {
            printf("\n[main] isPrimary\n");
            primaryReplicaStart();

            char buff[10];
            fgets(buff, sizeof(buff), stdin);
            buff[strcspn(buff, "\n")] = 0;
            if(strlen(buff) > 1) {
                intHandler(1);
            }
        }
        else {
            printf("\n[main] isBackup\n");

            backupReplicaStart();
        }
        pthread_cond_wait(&electionFinished, &startElectionMutex);

        isElectionRunning = false;
        fflush(stdout);
        printf("\n[main] restarting replica\n");
        pthread_mutex_unlock(&startElectionMutex);
    }

    exit(0);
}


