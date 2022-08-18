#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "../lib/helper.h"
#include "../file-control/file-handler.h"
#include "../server/server_functions.h"

#define MAX 80
#define SA struct sockaddr
char username[USERNAMESIZE];

void startWatchDir(struct in_addr ipAddr);
void addSocketConn(int socket, bool isListener);
int downloadAll(int socket);
void rand_str(char *dest, size_t length);

char path[KBYTE];
char rootPath[KBYTE];

pthread_mutex_t syncDirSem;

int listenerSocket;

pthread_t listenSyncThread;
received_file_list *filesReceived;

socket_conn_list* socketConn = NULL;
char* sessionCode;

void clientUpload(int socket) {

    write(socket, &commands[UPLOAD], sizeof(commands[UPLOAD]));

    char filePath[KBYTE];
    char* fileName;
    bzero(filePath, sizeof(filePath));

    printf("Insert file path:\n");
    fgets(filePath, sizeof(filePath), stdin);
    filePath[strcspn(filePath, "\n")] = 0;
    fileName = basename(filePath);
    upload(socket, filePath, fileName);

}

void newConnection(int sockfd, int socketType){
    write(sockfd, &socketTypes[socketType], sizeof(socketTypes[socketType]));
    printf("username: %s\n", username);
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

int clientDownload(int socket) {
    write(socket, &commands[DOWNLOAD], sizeof(commands[DOWNLOAD]));

    printf("File name to download:\n");
    char fileName[FILENAMESIZE];
    fgets(fileName, sizeof(fileName), stdin);
    fileName[strcspn(fileName, "\n")] = 0;
    write(socket, fileName, strlen(fileName));
    char* filePath = strcatSafe(path, fileName);
    receiveFile(socket, filePath);


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
    user_t *clientUser = calloc(1, sizeof(user_t));
    clientUser->syncSocketList = socketConn;
    clientUser->userAccessSem = &syncDirSem;
    clientUser->filesReceived = filesReceived;
    clientUser->username = username;
    listenForSocketMessage(socket, path, clientUser, false);
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
    servaddr.sin_addr = ipAddr;
    servaddr.sin_port = htons(SYNCPORT);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

    newConnection(sockfd, SYNCSOCKET);
    addSocketConn(sockfd, true);

    int* newSocket = calloc(1, sizeof(int ));
    *newSocket = sockfd;
    listenerSocket = sockfd;
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
    addSocketConn(sockfd, false);


    pthread_t syncDirThread;
    watch_dir_argument* argument = calloc(1, sizeof(watch_dir_argument));
    argument->dirPath = path;
    argument->socketConnList = socketConn;
    argument->userSem = &syncDirSem;
    argument->filesReceived = filesReceived;

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

        recv(connfd, buff, sizeof(buff), 0);
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
    srand((unsigned int)(time(NULL)));
    sessionCode = calloc(20, sizeof(char));
    rand_str(sessionCode, 18);
    int sockfd;
    struct sockaddr_in servaddr;
    pthread_mutex_init(&syncDirSem, NULL);

    DL_APPEND(filesReceived, createReceivedFile("\n", -1));

    bzero(path, sizeof(path));
    printf("Insira o caminho para a pasta sync_dir\n");
    fgets(path, sizeof(path), stdin);
    path[strcspn(path, "\n")] = 0;

    printf("Enter username: ");
    fgets(username, USERNAMESIZE, stdin);
    username[strcspn(username, "\n")] = 0;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));
    char ipAddress[15];
    bzero(ipAddress, sizeof(ipAddress));
    printf("Insira o IP do servidor\n");
    fgets(ipAddress, sizeof(ipAddress), stdin);
    ipAddress[strcspn(ipAddress, "\n")] = 0;

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ipAddress);
    servaddr.sin_port = htons(SERVERPORT);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("Connection with the server failed...\n");
        exit(0);
    }
    else
        printf("Connected to the server..\n");

    newConnection(sockfd, CLIENTSOCKET);


    char buff[USERNAMESIZE];
    bzero(buff, sizeof(buff));
    recv(sockfd, buff, sizeof(buff), 0);
    printf("SERVER CONNECTION STATUS: %s\n", buff);

    write(sockfd, &endCommand, sizeof(endCommand));

    startListenSyncDir(servaddr.sin_addr);
    bzero(buff, sizeof(buff));

    recv(sockfd, buff, sizeof(buff), 0);
    if(strcmp(buff, commands[WAITING]) != 0) {
        printf("[clientThread] expected waiting command");
    }

    if(downloadAll(sockfd) != 0) {
        return OUTOFSYNCERROR;
    }


    startWatchDir(servaddr.sin_addr);

    // function for user commands
    clientThread(sockfd);

    // close the socket
    close(sockfd);
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