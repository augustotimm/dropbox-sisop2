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



#define LIVENESSPORT 9998
#define MAX 2048



user_list* connectedUserListHead = NULL;
replica_info_list* replicaList = NULL;
bool isPrimary = false;
int electionValue = -1;
int electionPort = -1;

pthread_mutex_t startElectionMutex;
bool isElectionRunning = false;


char rootPath[KBYTE];

pthread_cond_t closedUserConnection;
pthread_mutex_t connectedUsersMutex;

pthread_mutex_t connectedReplicaListMutex;

pthread_mutex_t backupConnectionMutex;
socket_conn_list* backupConnectionList = NULL;

pthread_t* electionThread = NULL;

struct new_connection_argument {
    int socket;
    struct in_addr ipAddr;
    char* ipString;
};

void connectUser(int socket, char* username, char* sessionCode, char* ipAddr, int port);
int connectSyncDir(int socket, char* username, char* sessionCode);
int connectSyncListener(int socket, char*username,  char* sessionCode);
void* newBackupConnection(void* args);
void listenElectionMessages();

void* clientListen(void* voidArg)
{
    client_thread_argument* argument = (client_thread_argument*) voidArg;
    char* path = argument->clientDirPath;
    int socket = argument->socket;

    char currentCommand[13];
    bzero(currentCommand, sizeof(currentCommand));

    strcpy(currentCommand, "TRUE");
    write(socket, currentCommand, sizeof(currentCommand));

    //waiting for command
    printf("waiting for first command\n");
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
    socket_conn_list *elt = NULL;
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
    socket_conn_list *elt = NULL;
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
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERVERPORT);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
        printf("CLIENTCONN socket bind failed...\n");
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

        puts("Connection accepted");
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

        puts("Connection accepted");
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

        puts("Connection accepted");
        pthread_t newUserThread;


        struct new_connection_argument *arg = calloc(1, sizeof(struct new_connection_argument));
        arg->socket = connfd;
        arg->ipAddr = ipAddr;

        pthread_create(&newUserThread, NULL, newConnection, arg);

        pthread_detach(newUserThread);
        //Reply to the client

    }
}

void* backupStartConnectionWithPrimary(replica_info_t primary) {
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
        printf("Connection with the Primary backup failed\n");
    }

    printf("Primary is alive\n");

    write(sockfd, &socketTypes[BACKUPSOCKET], sizeof(socketTypes[BACKUPSOCKET]));
    recv(sockfd, buff, sizeof(buff), 0);
    if(strcmp(buff, endCommand) != 0) {
        printf("[backupStartConnectionWithPrimary] expected endCommand command");
    }

    backupListenForMessage(sockfd, rootPath, &isElectionRunning);


    pthread_mutex_lock(&startElectionMutex);
    isElectionRunning = true;
    pthread_mutex_unlock(&startElectionMutex);

    deletePrimary();

    startElection();
}

void backupReplicaStart(replica_info_t primary) {

    // liberar essa memória
    if(electionThread == NULL) {
        electionThread = (pthread_t*) calloc(1, sizeof(pthread_t));
        pthread_create(electionThread, NULL, listenElectionMessages, NULL);
        pthread_detach(electionThread);
    }

    backupStartConnectionWithPrimary(primary);
}

void* newBackupConnection(void* args) {
    struct new_connection_argument *argument = (struct new_connection_argument*) args;
    int socket = argument->socket;

    char newSocketType[USERNAMESIZE];
    bzero(newSocketType, sizeof(newSocketType));
    recv(socket, newSocketType, sizeof(newSocketType), 0);

    write(socket, &endCommand, strlen(endCommand));

    if(strcmp(newSocketType, socketTypes[BACKUPSOCKET]) == 0) {
       socket_conn_list *newConn = (socket_conn_list*) calloc(1, sizeof(socket_conn_list));
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

        wait(1);
        close(socket);

        pthread_mutex_lock(&startElectionMutex);
        isElectionRunning = true;
        pthread_mutex_unlock(&startElectionMutex);

        return NULL;

    }
    if(strcmp(newSocketType, socketTypes[ELECTIONCOORDSOCKET]) == 0) {
        char buff[20];
        bzero(buff, sizeof(buff));
        recv(socket, buff, sizeof(buff), 0);
        int replicaElectionValue;
        sscanf(buff, "%d", &replicaElectionValue);

        updatePrimary(replicaElectionValue);
    }
}

void listenLivenessCheck() {
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

        puts("Connection accepted");
        pthread_t newBackupThread;


        struct new_connection_argument *arg = calloc(1, sizeof(struct new_connection_argument));
        arg->socket = connfd;
        arg->ipAddr = ipAddr;

        pthread_create(&newBackupThread, NULL, newBackupConnection, arg);

        pthread_detach(newBackupThread);
    }
}

void primaryReplicaStart() {

    if(electionThread != NULL){
        pthread_cancel(electionThread);
        free(electionThread);
    }
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

    listenLivenessCheck();
}

void listenElectionMessages() {
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

        puts("Connection accepted");
        pthread_t newBackupThread;


        struct new_connection_argument *arg = calloc(1, sizeof(struct new_connection_argument));
        arg->socket = connfd;
        arg->ipAddr = ipAddr;

        pthread_create(&newBackupThread, NULL, newBackupConnection, arg);

        pthread_detach(newBackupThread);
    }
}

int main()
{

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
        if (isPrimary)
            primaryReplicaStart();
        else {
            replica_info_t primaryCopy;
            primaryCopy.port = primary->replica.port;
            primaryCopy.ipAddr = (char *) calloc(strlen(primary->replica.ipAddr) + 1, sizeof(char));
            strcpy(primaryCopy.ipAddr, primary->replica.ipAddr);

            backupReplicaStart(primaryCopy);
        }
    }

    exit(0);
}


