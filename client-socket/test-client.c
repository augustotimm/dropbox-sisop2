#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "../lib/helper.h"
#include "../file-control/file-handler.h"
#include "../server/server_functions.h"
#include "./front-end.h"

#define MAX 80
#define SA struct sockaddr
char username[USERNAMESIZE];

void startWatchDir();
void addSocketConn(int socket, bool isListener);
int downloadAll(int socket);
void rand_str(char *dest, size_t length);

char path[KBYTE];
char rootPath[KBYTE];

pthread_mutex_t syncDirSem;
pthread_t listenSyncThread;
received_file_list *filesReceived;

socket_conn_list* socketConn = NULL;
char* sessionCode;

int frontEndPort = 0;

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

    write(*socket, fileName, strlen(fileName));

    char* filePathName = strcatSafe(filePath, fileName);

    write(*socket, &endCommand, sizeof(endCommand));

    bzero(receivingFileName, sizeof(receivingFileName));
    recv(*socket, receivingFileName, sizeof(receivingFileName), 0);

    if(strcmp(fileName, endCommand) == 0) {
        printf("Connection out of sync\n");
        printf("Expected filename but received: endCommand\n\n");
        return NULL;
    }
    receiveFile(*socket, filePathName);


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

void* listenSyncDir(void* args) {
    int socket = *(int*)args;
    free(args);
    user_t *clientUser = calloc(1, sizeof(user_t));
    clientUser->syncSocketList = socketConn;
    clientUser->userAccessSem = &syncDirSem;
    clientUser->filesReceived = filesReceived;
    clientUser->username = username;
    listenForSocketMessage(socket, path, clientUser, false, NULL, NULL);
    close(socket);
}

void startListenSyncDir() {


    connectToServer(syncListenSocket, SYNCLISTENERPORT);

    newConnection(*syncListenSocket, SYNCSOCKET);
    addSocketConn(*syncListenSocket, true);


    int created = pthread_create(&listenSyncThread, NULL, listenSyncDir, syncListenSocket);
    pthread_detach(listenSyncThread);
    if(created != 0) {
        printf("Listen sync dir failed\n");
        pthread_cancel(listenSyncThread);
        close(*syncListenSocket);
   }
}

void startWatchDir() {

    connectToServer(syncListenSocket, SYNCPORT);

    newConnection(*syncListenSocket, SYNCSOCKET);
    addSocketConn(*syncListenSocket, false);


    pthread_t syncDirThread;
    watch_dir_argument* argument = calloc(1, sizeof(watch_dir_argument));
    argument->dirPath = path;
    argument->socketConnList = socketConn;
    argument->userSem = &syncDirSem;
    argument->filesReceived = filesReceived;

    pthread_create(&syncDirThread, NULL, watchDir, argument);
    pthread_detach(syncDirThread);
}


void clientThread(int *connfd)
{
    char userInput[MAX];
    char buff[MAX];
    bzero(username, sizeof(username));
    bzero(buff, sizeof(buff));
    int n;

    for (;;) {

        recv(*connfd, buff, sizeof(buff), 0);
        if(strcmp(buff, commands[WAITING]) != 0) {
            printf("[clientThread] expected waiting command");
        }

        bzero(userInput, sizeof(userInput));
        bzero(buff, sizeof(buff));
        printf("Enter the command:");
        n = 0;
        fgets(userInput, sizeof(userInput), stdin);
        userInput[strcspn(userInput, "\n")] = 0;
        if(strcmp(userInput, commands[UPLOAD]) ==0 ) {
            pthread_mutex_lock(&syncDirSem);
            clientUpload(connfd);
            pthread_mutex_unlock(&syncDirSem);
        } else if(strcmp(userInput, commands[DOWNLOAD]) ==0 ) {
            pthread_mutex_lock(&syncDirSem);
            clientDownload(connfd);
            pthread_mutex_unlock(&syncDirSem);
        } else if(strcmp(userInput, commands[LIST]) ==0 ) {
            list_local(path);
        } else if(strcmp(userInput, commands[DELETE]) ==0 ) {
            pthread_mutex_lock(&syncDirSem);
            clientDelete(connfd);
            pthread_mutex_unlock(&syncDirSem);
        }

        if ((strncmp(userInput, "exit", 4)) == 0) {
            printf("Client Exit...\n");
            write(socket, &commands[EXIT], sizeof(commands[EXIT]));
            break;
        }
    }
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

    clientSocket = calloc(1, sizeof(int));
    syncDirSocket = calloc(1, sizeof(int));
    syncListenSocket = calloc(1, sizeof(int));

    connectToServer(clientSocket, SERVERPORT);

    newConnection(*clientSocket, CLIENTSOCKET);
    char portBuffer[BUFFERSIZE];

    sprintf(portBuffer, "%d", frontEndPort);

    write(*clientSocket, portBuffer, sizeof(portBuffer));
    // wait for endcommand
    recv(*clientSocket, portBuffer, sizeof(portBuffer), 0);

    char buff[USERNAMESIZE];
    bzero(buff, sizeof(buff));
    recv(*clientSocket, buff, sizeof(buff), 0);
    printf("SERVER CONNECTION STATUS: %s\n", buff);

    write(*clientSocket, &endCommand, sizeof(endCommand));

    startListenSyncDir();
    bzero(buff, sizeof(buff));

    recv(*clientSocket, buff, sizeof(buff), 0);
    if(strcmp(buff, commands[WAITING]) != 0) {
        printf("[clientThread] expected waiting command");
    }

    if(downloadAll(*clientSocket) != 0) {
        return OUTOFSYNCERROR;
    }


    startWatchDir();

    // function for user commands
    clientThread(clientSocket);

    // close the socket
    close(*clientSocket);
}

void addSocketConn(int socket,  bool isListener) {
    pthread_mutex_lock(&syncDirSem);
    if(socketConn == NULL) {
        socketConn = initSocketConnList(socket, sessionCode, isListener);
    }
    else {
        if(isListener) {
            socketConn->listenerSocket = socket;
        }
        else {
            socketConn->socket = socket;
        }
    }
    pthread_mutex_unlock(&syncDirSem);
}

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

            return 0;
        } else{
            return OUTOFSYNCERROR;
        }
    }
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