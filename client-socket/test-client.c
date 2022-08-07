#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/helper.h"
#include "../file-control/file-handler.h"

#define MAX 80
#define SA struct sockaddr
char username[USERNAMESIZE];

void startWatchDir(int socket);
char* path = "/home/augusto/repositorios/ufrgs/dropbox-sisop2/client-socket/sync/";

sem_t syncDirSem;

pthread_t listenSyncThread;

void clientUpload(int socket) {
    printf("Upload\n");

    write(socket, &commands[UPLOAD], sizeof(commands[UPLOAD]));

    char fileName[FILENAMESIZE];
    bzero(fileName, sizeof(fileName));

    printf("Insert name of file:\n");
    fgets(fileName, sizeof(fileName), stdin);
    fileName[strcspn(fileName, "\n")] = 0;


    upload(socket, path, fileName);
}

int clientDownload(int socket) {
    printf("clientDownload function");
    char fileName[FILENAMESIZE];
    fgets(fileName, sizeof(fileName), stdin);
    fileName[strcspn(fileName, "\n")] = 0;


    char* filePath = strcatSafe(path, fileName);
    receiveFile(socket, filePath);
    free(filePath);
}

void list_local(char * pathname) {
    file_info_list* infoList = getListOfFiles(pathname);
    printFileInfoList(infoList);
}

void* listenSyncDir(void* args) {
    int socket = *(int*)args;
    free(args);
    listenForSocketMessage(socket, path, &syncDirSem);
    close(socket);
}

void startListenSyncDir() {
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
    write(sockfd, &socketTypes[SYNCSOCKET], sizeof(socketTypes[SYNCSOCKET]));
    printf("username: %s", username);
    char endCommand[6];
    recv(sockfd, &endCommand, sizeof(endCommand), 0);

    write(sockfd, &username, sizeof(username));
    // startWatchDir(sockfd);
    int* newSocket = calloc(1, sizeof(int ));
    *newSocket = sockfd;
    pthread_create(&listenSyncThread, NULL, listenSyncDir, newSocket);
    pthread_detach(listenSyncThread);
}

void startWatchDir(int socket) {
    pthread_t syncDirThread;
    watch_dir_argument* argument = calloc(1, sizeof(watch_dir_argument));
    argument->dirPath = path;
    argument->socketConnList = initSocketConnList(socket);
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
        printf("Enter the string: ");
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

    startListenSyncDir();

    // function for chat
    clientThread(sockfd);

    // close the socket
    close(sockfd);
}