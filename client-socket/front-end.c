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
#include "../lib/helper.h"

#define SA struct sockaddr

int* clientSocket = NULL;
int* syncDirSocket = NULL;
int* syncListenSocket = NULL;
char serverIp[15];

pthread_mutex_t isConnectionOpenMutex;

void* newServerConnection(void* args) {
    int* connSocket = (int*) args;
    char buff[BUFFERSIZE];
    bzero(buff, sizeof(buff));

    recv(*connSocket, buff, sizeof(buff), 0);
    write(*connSocket, &endCommand, sizeof(endCommand));
    printf("\n---frontend command: %s\n", buff);
    if(strcmp(buff, frontEndCommands[DEAD]) == 0) {
        pthread_mutex_lock(&isConnectionOpenMutex);
    }
    if(strcmp(buff, frontEndCommands[NEWPRIMARY]) == 0) {
        pthread_mutex_unlock(&isConnectionOpenMutex);
    }
    close(*connSocket);

    free(connSocket);
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
        int *connSocket = calloc(1, sizeof(int));
        *connSocket = connfd;

        pthread_t newServerConn;

        pthread_create(&newServerConn, NULL, newServerConnection, (void*)connSocket);

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