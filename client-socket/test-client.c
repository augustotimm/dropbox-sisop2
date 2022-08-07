#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/helper.h"
#include "../file-control/file-handler.h"

#define MAX 80
#define SA struct sockaddr
char username[USERNAMESIZE];

char commands[5][13] = {"upload", "download", "list local", "get_sync_dir", "exit"};

char* path = "/home/augusto/repositorios/ufrgs/dropbox-sisop2/client-socket/sync/";

void upload(int socket) {
    printf("Upload\n");

    write(socket, &commands[UPLOAD], sizeof(commands[UPLOAD]));

    char fileName[FILENAMESIZE];
    bzero(fileName, sizeof(fileName));

    printf("Insert name of file:\n");
    fgets(fileName, sizeof(fileName), stdin);
    fileName[strcspn(fileName, "\n")] = 0;

    write(socket, &fileName, sizeof(fileName));

    char* filePath = calloc(strlen(fileName) + strlen(path), sizeof(char));
    strcpy(filePath, path);
    strcat(filePath, fileName);

    sendFile(socket, filePath);
    free(filePath);
}

int download(int socket) {
    char fileName[FILENAMESIZE];
    printf("Download\n");

    write(socket, &commands[DOWNLOAD], sizeof(commands[DOWNLOAD]));
    printf("Insert name of file:\n");
    fgets(fileName, sizeof(fileName), stdin);
    fileName[strcspn(fileName, "\n")] = 0;
    write(socket, &fileName, sizeof(fileName));
    return receiveFile(socket, fileName);
}

void list_local(char * pathname) {
    file_info_list* infoList = getListOfFiles(pathname);
    printFileInfoList(infoList);
}

void sync(int serverSocket) {
    printf("sync function");
    write(serverSocket, &commands[SYNC], sizeof(commands[SYNC]));
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
    servaddr.sin_port = htons(SERVERPORT);

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
}


void clientThread(int connfd)
{
    char userInput[MAX];
    char buff[MAX];
    bzero(username, sizeof(username));
    bzero(buff, sizeof(buff));
    int n;

    write(connfd, &socketTypes[CLIENTSOCKET], sizeof(socketTypes[CLIENTSOCKET]));



    printf("Enter username: ");
    fgets(username, USERNAMESIZE, stdin);
    username[strcspn(username, "\n")] = 0;
    write(connfd, &username, sizeof(username));
    recv(connfd, buff, sizeof(buff), 0);
    printf("SERVER CONNECTION STATUS: %s\n", buff);
    sync(connfd);

    for (;;) {
        bzero(userInput, sizeof(userInput));
        bzero(buff, sizeof(buff));
        printf("Enter the string: ");
        n = 0;
        fgets(userInput, sizeof(userInput), stdin);
        userInput[strcspn(userInput, "\n")] = 0;
        if(strcmp(userInput, commands[UPLOAD]) ==0 ) {
            upload(connfd);
        } else if(strcmp(userInput, commands[DOWNLOAD]) ==0 ) {
            if(download(connfd) == OUTFOSYNCERROR) {
                return;
            }
        } else if(strcmp(userInput, commands[LIST]) ==0 ) {
            list_local(path);
        } else if(strcmp(userInput, commands[SYNC]) ==0 ) {
            sync(connfd);
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

    // function for chat
    clientThread(sockfd);

    // close the socket
    close(sockfd);
}