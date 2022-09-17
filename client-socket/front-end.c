//
// Created by Augusto on 14/09/2022.
//

#include "front-end.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include "./test-client.h"

#define SA struct sockaddr

typedef struct new_conn_args{
    int connSocket;
    char* ipAddress;
}new_conn_args;

int* clientSocket = NULL;
int* syncDirSocket = NULL;
int* syncListenSocket = NULL;
char serverIp[15];

pthread_mutex_t isConnectionOpenMutex;

pthread_mutex_t newServerConnectionMutex;

bool isPrimaryDead = false;

void* newServerConnection(void* args) {
    new_conn_args * newConnArgs = (new_conn_args*) args;
    int connSocket = newConnArgs->connSocket;
    char buff[BUFFERSIZE];
    bzero(buff, sizeof(buff));

    recv(connSocket, buff, sizeof(buff), 0);
    write(connSocket, &endCommand, sizeof(endCommand));
    printf("\n---frontend command: %s\n", buff);

    printf("\nServer IP ADDRESS:\n%s\n", newConnArgs->ipAddress);

    if(strcmp(buff, frontEndCommands[DEAD]) == 0 && !isPrimaryDead) {
        pthread_mutex_lock(&isConnectionOpenMutex);
        isPrimaryDead = true;
    }
    if(strcmp(buff, frontEndCommands[NEWPRIMARY]) == 0) {
        pthread_mutex_lock(&newServerConnectionMutex);
        if(isPrimaryDead) {
            pthread_cancel(clientThread);
            pthread_cancel(listenSyncThread);

            bzero(serverIp, sizeof(serverIp));
            strcpy(serverIp, newConnArgs->ipAddress);

            int clientStarted = startClient(false);
            if(clientStarted != 0){
                exit(OUTOFSYNCERROR);
            }

            isPrimaryDead = false;
            pthread_mutex_unlock(&isConnectionOpenMutex);
        }

        pthread_mutex_unlock(&newServerConnectionMutex);

    }
    close(connSocket);

    free(newConnArgs->ipAddress);
    free(newConnArgs);
}

void* listenForReplicaMessage(void* args) {
    int port = * (int*) args;
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("FRONTENDCONN socket creation failed...\n");
        exit(0);
    }
    else
        printf("FRONTENDCONN Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
        printf("FRONTENDCONN socket bind failed...\n");
        exit(0);
    }
    else
        printf("FRONTENDCONN Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("FRONTENDCONN Listen failed...\n");
        exit(0);
    }
    else
        printf("FRONTENDCONN Server listening..\n");
    len = sizeof(cli);

    int i = 0;
    while( (connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t*)&len))  || i < 5)
    {
        new_conn_args *newConnArgs = calloc(1, sizeof(new_conn_args));
        newConnArgs->connSocket = connfd;

        newConnArgs->ipAddress = calloc(15, sizeof(char));
        bzero(newConnArgs->ipAddress, 15);

        strcpy(newConnArgs->ipAddress, inet_ntoa(cli.sin_addr));

        pthread_t newServerConn;

        pthread_create(&newServerConn, NULL, newServerConnection, (void*)newConnArgs);

        pthread_detach(newServerConn);
    }
}

void connectToServer(int* connSocket, int port) {
    struct sockaddr_in servaddr;
    int sockfd;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");



    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(serverIp);
    servaddr.sin_port = htons(port);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("Connection with the server failed...\n");
        exit(0);
    }
    else
        printf("Connected to the server..\n");

    *connSocket = sockfd;
}

void newConnection(int sockfd, int socketType){
    write(sockfd, &socketTypes[socketType], sizeof(socketTypes[socketType]));
    char endCommand[6];
    bzero(endCommand, sizeof(endCommand));

    recv(sockfd, &endCommand, sizeof(endCommand), 0);

    write(sockfd, &username, sizeof(username));
    bzero(endCommand, sizeof(endCommand));
    recv(sockfd, &endCommand, sizeof(endCommand), 0);

    write(sockfd, sessionCode, strlen(sessionCode));
    bzero(endCommand, sizeof(endCommand));
    recv(sockfd, &endCommand, sizeof(endCommand), 0);

}

void addSocketConn(int socket,  bool isListener) {
    pthread_mutex_lock(&socketConnMutex);
    if(socketConn == NULL) {
        socketConn = calloc(1, sizeof(sync_dir_conn));
        socketConn->listenerSocket = calloc(1, sizeof(int));
        socketConn->socket = calloc(1, sizeof(int));

        if(isListener)
            *socketConn->listenerSocket = socket;
        else
            *socketConn->socket = socket;
    }
    else {
        if(isListener) {
            *socketConn->listenerSocket = socket;
        }
        else {
            *socketConn->socket = socket;
        }
    }
    pthread_mutex_unlock(&socketConnMutex);
}
