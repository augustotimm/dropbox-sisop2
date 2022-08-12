#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "../lib/helper.h"
#include "../file-control/file-handler.h"

#define MAX 80
#define SA struct sockaddr
char username[USERNAMESIZE];

void startWatchDir(struct in_addr ipAddr);
void addSocketConn(int socket, struct in_addr ipAddr, bool isListener);

char path[KBYTE] = "/home/timm/repos/ufrgs/dropbox-sisop2/sync/";
char rootPath[KBYTE];

sem_t syncDirSem;

pthread_t listenSyncThread;

socket_conn_list* socketConn = NULL;

void clientUpload(int socket) {
    printf("Upload\n");

    write(socket, &commands[UPLOAD], sizeof(commands[UPLOAD]));

    char filePath[KBYTE];
    char* fileName;
    bzero(filePath, sizeof(filePath));

    printf("Insert name of file:\n");
    fgets(filePath, sizeof(filePath), stdin);
    filePath[strcspn(filePath, "\n")] = 0;
    fileName = basename(filePath);
    upload(socket, filePath, fileName);

}

void newConnection(int sockfd, int socketType){
    write(sockfd, &socketTypes[socketType], sizeof(socketTypes[socketType]));
    printf("username: %s", username);
    char endCommand[6];
    recv(sockfd, &endCommand, sizeof(endCommand), 0);
    write(sockfd, &username, sizeof(username));
}

int clientDownload(int socket) {
    write(socket, &commands[DOWNLOAD], sizeof(commands[DOWNLOAD]));

    printf("File name to download:\n");
    char fileName[FILENAMESIZE];
    fgets(fileName, sizeof(fileName), stdin);
    fileName[strcspn(fileName, "\n")] = 0;
    write(socket, fileName, strlen(fileName));
    char* filePath = strcatSafe(path, fileName);
    download(socket, filePath);
    free(filePath);
}

int clientDelete(int socket) {
    write(socket, &commands[DELETE], sizeof(commands[DELETE]));

    printf("File name to delete:\n");
    char fileName[FILENAMESIZE];
    fgets(fileName, sizeof(fileName), stdin);
    fileName[strcspn(fileName, "\n")] = 0;
    write(socket, fileName, strlen(fileName));
    char buff[BUFFERSIZE];
    bzero(buff, sizeof(buff));
    recv(socket, buff, sizeof(buff), 0);
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
    listenForSocketMessage(socket, path, &syncDirSem, NULL);
    close(socket);
}

void startListenSyncDir(struct in_addr ipAddr) {
    int sockfd;
    struct sockaddr_in servaddr;

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
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(SYNCPORT);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

    newConnection(sockfd, SYNCSOCKET);
    addSocketConn(sockfd, ipAddr, true);

    int* newSocket = calloc(1, sizeof(int ));
    *newSocket = sockfd;
   int created = pthread_create(&listenSyncThread, NULL, listenSyncDir, newSocket);
    pthread_detach(listenSyncThread);
    if(created != 0) {
        printf("Listen sync dir failed\n");
        pthread_cancel(listenSyncThread);
        close(sockfd);
        free(newSocket);
   }
}

void startWatchDir(struct in_addr ipAddr) {
    int sockfd;
    struct sockaddr_in servaddr;

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
    servaddr.sin_addr = ipAddr;
    servaddr.sin_port = htons(SYNCPORT);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

    newConnection(sockfd, SYNCLISTENSOCKET);
    addSocketConn(sockfd, ipAddr, false);


    pthread_t syncDirThread;
    watch_dir_argument* argument = calloc(1, sizeof(watch_dir_argument));
    argument->dirPath = path;
    argument->socketConnList = socketConn;
    argument->userSem = &syncDirSem;

    pthread_create(&syncDirThread, NULL, watchDir, argument);
    pthread_detach(syncDirThread);
}


void clientThread(int connfd)
{
    char userInput[MAX];
    char buff[MAX];
    bzero(username, sizeof(username));
    bzero(buff, sizeof(buff));
    int n;

    for (;;) {
        bzero(userInput, sizeof(userInput));
        bzero(buff, sizeof(buff));
        printf("Enter the command:");
        n = 0;
        fgets(userInput, sizeof(userInput), stdin);
        userInput[strcspn(userInput, "\n")] = 0;
        if(strcmp(userInput, commands[UPLOAD]) ==0 ) {
            sem_wait(&syncDirSem);
            clientUpload(connfd);
            sem_post(&syncDirSem);
        } else if(strcmp(userInput, commands[DOWNLOAD]) ==0 ) {
            sem_wait(&syncDirSem);
            clientDownload(connfd);
            sem_post(&syncDirSem);
        } else if(strcmp(userInput, commands[LIST]) ==0 ) {
            list_local(path);
        } else if(strcmp(userInput, commands[DELETE]) ==0 ) {
            clientDelete(connfd);
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
    int sockfd;
    struct sockaddr_in servaddr;

    sem_init(&syncDirSem, 0, 1);

    //printf("Insira o caminho para a pasta sync_dir\n");
    //fgets(path, sizeof(path), stdin);
    //path[strcspn(path, "\n")] = 0;

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
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(SERVERPORT);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("Connection with the server failed...\n");
        exit(0);
    }
    else
        printf("Connected to the server..\n");

    char userInput[MAX];
    char buff[MAX];
    bzero(username, sizeof(username));
    bzero(buff, sizeof(buff));
    int n;

    write(sockfd, &socketTypes[CLIENTSOCKET], sizeof(socketTypes[CLIENTSOCKET]));



    printf("Enter username: ");
    fgets(username, USERNAMESIZE, stdin);
    username[strcspn(username, "\n")] = 0;
    write(sockfd, &username, sizeof(username));
    recv(sockfd, buff, sizeof(buff), 0);
    printf("SERVER CONNECTION STATUS: %s\n", buff);

    write(sockfd, &endCommand, sizeof(endCommand));

    startListenSyncDir(servaddr.sin_addr);
    //startWatchDir(servaddr.sin_addr);

    // function for user commands
    clientThread(sockfd);

    // close the socket
    close(sockfd);
}

void addSocketConn(int socket, struct in_addr ipAddr, bool isListener) {
    sem_wait(&syncDirSem);
    if(socketConn == NULL) {
        socketConn = initSocketConnList(socket, ipAddr, isListener);
    }
    else {
        if(isListener) {
            socketConn->listenerSocket = socket;
        }
        else {
            socketConn->socket = socket;
        }
    }
    sem_post(&syncDirSem);
}