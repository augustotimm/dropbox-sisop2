//
// Created by Augusto on 14/09/2022.
//
#include <netdb.h>
#include <libgen.h>

#include "front-end.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include "./test-client.h"
#include <arpa/inet.h>
#include "../server/server_functions.h"


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

int frontEndPort = 0;

void* clientThreadFunction(void* args);
int downloadAll(int socket);

void* newServerConnection(void* args) {
    new_conn_args * newConnArgs = (new_conn_args*) args;
    int connSocket = newConnArgs->connSocket;
    char buff[BUFFERSIZE];
    bzero(buff, sizeof(buff));

    recv(connSocket, buff, sizeof(buff), 0);
    write(connSocket, &endCommand, sizeof(endCommand));
    printf("\n---frontend command: %s", buff);

    if(strcmp(buff, frontEndCommands[DEAD]) == 0 && !isPrimaryDead) {
        pthread_mutex_lock(&isConnectionOpenMutex);
        isPrimaryDead = true;
    }
    if(strcmp(buff, frontEndCommands[NEWPRIMARY]) == 0) {
        pthread_mutex_lock(&newServerConnectionMutex);
        if(isPrimaryDead) {
            pthread_cancel(clientThread);
            pthread_cancel(listenSyncThread);
            printf("\nold threads canceled");

            bzero(serverIp, sizeof(serverIp));
            strcpy(serverIp, newConnArgs->ipAddress);
            printf("\nserver ip updated");

            int clientStarted = startClient(false);
            if(clientStarted != 0) {
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
        printf("\nSocket successfully created..\n");



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
    char endCommand[10];
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

user_t *createClientUser() {
    user_t *clientUser = calloc(1, sizeof(user_t));
    clientUser->syncSocketList = calloc(1, sizeof(socket_conn_list));
    clientUser->syncSocketList->listenerSocket = *socketConn->listenerSocket;

    clientUser->userAccessSem = &syncDirSem;
    clientUser->filesReceived = filesReceived;
    clientUser->username = username;
}

void* listenSyncDir() {
    int socket = *syncListenSocket;

    user_t *clientUser = createClientUser();
    listenForSocketMessage(socket, path, clientUser, false, NULL, NULL);
    close(socket);
}

void startListenSyncDir() {


    connectToServer(syncListenSocket, SYNCPORT);

    newConnection(*syncListenSocket, SYNCSOCKET);
    addSocketConn(*syncListenSocket, true);


    pthread_create(&listenSyncThread, NULL, listenSyncDir, NULL);
    pthread_detach(listenSyncThread);

    printf("\nListen syncdir thread created");
}


int startClient(bool shouldDownloadAll) {
    printf("\nSTARTING CLIENT");

    connectToServer(clientSocket, SERVERPORT);

    newConnection(*clientSocket, CLIENTSOCKET);
    char portBuffer[BUFFERSIZE];

    sprintf(portBuffer, "%d", frontEndPort);

    write(*clientSocket, portBuffer, sizeof(portBuffer));
    // wait for endcommand
    recv(*clientSocket, portBuffer, sizeof(portBuffer), 0);

    char buff[USERNAMESIZE];
    bzero(buff, sizeof(buff));


    write(*clientSocket, &endCommand, sizeof(endCommand));

    startListenSyncDir();
    bzero(buff, sizeof(buff));

    if(shouldDownloadAll){
        recv(*clientSocket, buff, sizeof(buff), 0);

        if(strcmp(buff, commands[WAITING]) != 0) {
            printf("[clientThreadFunction] expected waiting command, received: %s", buff);
            exit(OUTOFSYNCERROR);
        }
        printf("\n[startClient]Waiting command received");
        if(downloadAll(*clientSocket) != 0) {
            exit(OUTOFSYNCERROR);
        }
    }


    int created = pthread_create(&clientThread, NULL, clientThreadFunction, (void*)clientSocket);
    pthread_detach(clientThread);
    printf("\nClient thread created");


    if(created != 0) {
        exit(OUTOFSYNCERROR);
    }

    connectToServer(syncDirSocket,  SYNCLISTENERPORT);

    newConnection(*syncDirSocket, SYNCLISTENSOCKET);
    addSocketConn(*syncDirSocket, false);

    printf("\nclient started");

    return 0;
}

//------client thread functions------
int downloadAll(int socket) {
    char currentCommand[13];
    char fileName[FILENAMESIZE];
    write(socket, &commands[DOWNLOADALL], sizeof(commands[DOWNLOADALL]));

    for (;;) {
        bzero(currentCommand, sizeof(currentCommand));
        bzero(fileName, sizeof(fileName));


        // read the message from client and copy it in buffer
        recv(socket, currentCommand, sizeof(currentCommand), 0);
        if(strcmp(currentCommand, commands[UPLOAD]) ==0 ) {
            download(socket, path, filesReceived, false);
        }else if (strcmp(currentCommand, commands[EXIT]) == 0 ) {
            write(socket, endCommand, sizeof(endCommand));
            return 0;
        } else{
            return OUTOFSYNCERROR;
        }
    }
}

void clientUpload(int *socket) {
    pthread_mutex_lock(&isConnectionOpenMutex);
    pthread_mutex_unlock(&isConnectionOpenMutex);
    write(*socket, &commands[UPLOAD], sizeof(commands[UPLOAD]));

    char filePath[KBYTE];
    char* fileName;
    bzero(filePath, sizeof(filePath));

    printf("Insert file path:\n");
    fgets(filePath, sizeof(filePath), stdin);
    filePath[strcspn(filePath, "\n")] = 0;
    fileName = basename(filePath);
    upload(*socket, filePath, fileName);

}

int clientDownload(int *socket) {
    pthread_mutex_lock(&isConnectionOpenMutex);
    pthread_mutex_unlock(&isConnectionOpenMutex);

    write(*socket, &commands[DOWNLOAD], sizeof(commands[DOWNLOAD]));
    char filePath[KBYTE];
    char receivingFileName[FILENAMESIZE];

    printf("File name to download:\n");
    char fileName[FILENAMESIZE];
    fgets(fileName, sizeof(fileName), stdin);
    fileName[strcspn(fileName, "\n")] = 0;
    bzero(filePath, sizeof(filePath));

    printf("Insert file path:\n");
    fgets(filePath, sizeof(filePath), stdin);
    filePath[strcspn(filePath, "\n")] = 0;

    write(*socket, fileName, FILENAMESIZE);

    char* filePathName = strcatSafe(filePath, fileName);

    write(*socket, &endCommand, sizeof(endCommand));

    bzero(receivingFileName, sizeof(receivingFileName));
    recv(*socket, receivingFileName, sizeof(receivingFileName), 0);

    if(strcmp(fileName, endCommand) == 0) {
        printf("Connection out of sync\n");
        printf("Expected filename but received: endCommand\n\n");
        return OUTOFSYNCERROR;
    }
    write(*socket, &endCommand, sizeof(endCommand));

    if(receiveFile(*socket, filePathName) == 0 ) {
        printf("File %s downloaded successfully\n\n", fileName);
        write(*socket, &endCommand, strlen(endCommand));
    }


    free(filePathName);
}

int clientDelete(int *socket) {
    pthread_mutex_lock(&isConnectionOpenMutex);
    pthread_mutex_unlock(&isConnectionOpenMutex);

    write(*socket, &commands[DELETE], sizeof(commands[DELETE]));

    printf("File name to delete:\n");
    char fileName[FILENAMESIZE];
    fgets(fileName, sizeof(fileName), stdin);
    fileName[strcspn(fileName, "\n")] = 0;
    write(*socket, fileName, strlen(fileName));
    char buff[BUFFERSIZE];
    bzero(buff, sizeof(buff));
    recv(*socket, buff, sizeof(buff), 0);
    if(strcmp(buff, endCommand) != 0) return OUTOFSYNCERROR;

    return 0;

}

void list_local(char * pathname) {
    file_info_list* infoList = getListOfFiles(pathname);
    printFileInfoList(infoList);
}

void* clientThreadFunction(void* args)
{
    int connfd = *(int*) args;
    free(args);
    char userInput[BUFFERSIZE];
    char buff[BUFFERSIZE];
    bzero(buff, sizeof(buff));
    int n;

    for (;;) {

        recv(connfd, buff, sizeof(buff), 0);
        printf("\n[clientThreadFunction] Waiting command received");
        if(strcmp(buff, commands[WAITING]) != 0) {
            printf("[clientThreadFunction] expected waiting command");
        }

        bzero(userInput, sizeof(userInput));
        bzero(buff, sizeof(buff));
        printf("Enter the command:");
        n = 0;
        fgets(userInput, sizeof(userInput), stdin);
        userInput[strcspn(userInput, "\n")] = 0;
        if(strcmp(userInput, commands[UPLOAD]) ==0 ) {
            pthread_mutex_lock(&syncDirSem);
            clientUpload(&connfd);
            pthread_mutex_unlock(&syncDirSem);
        } else if(strcmp(userInput, commands[DOWNLOAD]) ==0 ) {
            pthread_mutex_lock(&syncDirSem);
            clientDownload(&connfd);
            pthread_mutex_unlock(&syncDirSem);
        } else if(strcmp(userInput, commands[LIST]) ==0 ) {
            list_local(path);
        } else if(strcmp(userInput, commands[DELETE]) ==0 ) {
            pthread_mutex_lock(&syncDirSem);
            clientDelete(&connfd);
            pthread_mutex_unlock(&syncDirSem);
        }

        if ((strncmp(userInput, "exit", 4)) == 0) {
            printf("Client Exit...\n");
            write(socket, &commands[EXIT], sizeof(commands[EXIT]));
            close(socket);
            pthread_cond_signal(&exitCond);
            break;
        }
    }
}
