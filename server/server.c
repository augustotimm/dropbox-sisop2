//
// Created by augusto on 03/08/2022.
//

#include "server.h"
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include "user.h"
#include "server_functions.h"

#define MAX 2048

#include <unistd.h>




void connectUser(int socket);
int connectSyncDir(int socket, char* username);
void* clientListen(void* voidArg)
{
    client_thread_argument* argument = voidArg;
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
        printf("Expected end command signal but received: %s\n\n", currentCommand);
        return NULL;
    }

    listenForSocketMessage(socket, path, argument->userAccessSem);

    printf("Server Exiting socket: %d\n", socket);
    close(socket);
    *argument->isThreadComplete = true;
    free(argument);
    pthread_cond_signal(&closedUserConnection);

}
void writeMessageToSocket(int socket, char* message) {
    write(socket, message, strlen(message)+1);
}
void* newConnection(void* arg) {
    int socket = * (int*) arg;
    free(arg);
    char newSocketType[USERNAMESIZE];
    bzero(newSocketType, sizeof(newSocketType));
    recv(socket, newSocketType, sizeof(newSocketType), 0);
    if(strcmp(newSocketType, socketTypes[CLIENTSOCKET]) == 0) {
        connectUser(socket);
    }
    if(strcmp(newSocketType, socketTypes[SYNCSOCKET]) == 0) {
        bzero(newSocketType, USERNAMESIZE);
        write(socket, &endCommand, sizeof(endCommand));

        // get username to find in connected usersList
        recv(socket, newSocketType, USERNAMESIZE, 0);
        connectSyncDir(socket, newSocketType);

    }

}

int connectSyncDir(int socket, char* username) {
    user_list* user = findUser(username);
    if(user == NULL) {
        // sync dir connection of user logged out
        close(socket);
        return -1;
    }
    addSyncDir(socket, &user->user);

}

void connectUser(int socket) {
    char username[USERNAMESIZE];
    bzero(username, USERNAMESIZE);
    recv(socket, username, USERNAMESIZE, 0);
    if(startUserSession(username, socket) != 0) {
        writeMessageToSocket(socket, "FALSE");
        close(socket);
    }
}

void* userDisconnectedEvent(void *arg) {
    while(1) {
        user_list* currentUser = NULL, *userTmp = NULL;

        pthread_cond_wait(&closedUserConnection, &connectedUsersMutex);
        DL_FOREACH_SAFE(connectedUserListHead, currentUser, userTmp) {
            if(!hasSessionOpen(currentUser->user)) {
                sem_wait(&currentUser->user.userAccessSem);
                DL_DELETE(connectedUserListHead, currentUser);

                freeUserList(currentUser);
            }
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
        puts("Connection accepted");
        pthread_t newUserThread;
        int* newSocket = calloc(1, sizeof(int ));
        *newSocket = connfd;


        pthread_create(&newUserThread, NULL, newConnection, newSocket);

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
        puts("Connection accepted");
        pthread_t newUserThread;
        int* newSocket = calloc(1, sizeof(int ));
        *newSocket = connfd;


        pthread_create(&newUserThread, NULL, newConnection, newSocket);

        pthread_detach(newUserThread);
        //Reply to the client

    }
}


// Driver function
int main()
{
    printf("Insira o caminho para a pasta raiz onde ficarão as pastas de usuários\n");
    fgets(rootPath, sizeof(rootPath), stdin);
    rootPath[strcspn(rootPath, "\n")] = 0;
    connectedUserListHead = NULL;

    pthread_t userDisconnectedThread;
    pthread_create(&userDisconnectedThread, NULL, userDisconnectedEvent, NULL);
    pthread_detach(userDisconnectedThread);
    pthread_t clientConnThread;
    pthread_create(&clientConnThread, NULL, clientConn, NULL);

    pthread_t syncDirConnThread;
    pthread_create(&syncDirConnThread, NULL, syncDirConn, NULL);
    void** args;
    pthread_join(clientConnThread, args);
    pthread_join(syncDirConnThread, args);

}


