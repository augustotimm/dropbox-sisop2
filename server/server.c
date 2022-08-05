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

#define PORT 8888
#define MAX 2048
#define SA struct sockaddr


char commands[5][13] = {"upload", "download", "list", "get_sync_dir", "exit"};

//TODO change to relative path

void upload(int socket) {
    printf("upload function\n");
    char fileName[FILENAMESIZE];
    recv(socket, fileName, sizeof(fileName), 0);
    char* filePath = strcatSafe(path, fileName);


    printf("Receiving file: %s\n", filePath);
    receiveFile(socket, filePath);
    free(filePath);
}

void download(int socket) {
    printf("download function");
    char fileName[FILENAMESIZE];
    bzero(fileName, sizeof(fileName));
    recv(socket, fileName, sizeof(fileName), 0);


    char* filePath = strcatSafe(path, fileName);

    sendFile(socket, filePath);
    free(filePath);

}

void list() {
    printf("list function");
}

void sync() {
    printf("sync function");
}

void* clientThread(void* conf)
{
    char buff[MAX];
    int socket = * (int*) conf;
    char username[USERNAMESIZE];
    recv(socket, username, USERNAMESIZE, 0);

    thread_list* watchDirThread = initThreadListElement();

    char currentCommand[13];
    bzero(currentCommand, sizeof(currentCommand));
    //waiting for command
    printf("waiting for first command\n");
    for (;;) {
        bzero(buff, MAX);

        // read the message from client and copy it in buffer
        recv(socket, currentCommand, sizeof(currentCommand), 0);
        printf("COMMAND: %s\n", currentCommand);

        if(strcmp(currentCommand, commands[UPLOAD]) ==0 ) {
            upload(socket);
        } else if(strcmp(currentCommand, commands[DOWNLOAD]) ==0 ) {
            download(socket);
        } else if(strcmp(currentCommand, commands[LIST]) ==0 ) {
            list();
        } else if(strcmp(currentCommand, commands[SYNC]) ==0 ) {
            sync();
        }



        if (strcmp(currentCommand, commands[EXIT]) == 0) {
            printf("Server Exit...\n");
            close(socket);
            return NULL;
        }
    }

}


// Driver function
int main()
{
    sem_init(&userListWrite, 0, 1);
    connectedUserListHead = NULL;
    strcpy(path, "/home/augusto/repositorios/ufrgs/dropbox-sisop2/watch_folder/");

    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;
    pthread_t userThread;
    char* username = "user";
    pthread_create(&userThread, NULL, startUserSession, (void*) username);

    pthread_t thread[5];
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
    servaddr.sin_port = htons(PORT);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Server listening..\n");
    len = sizeof(cli);

    int i = 0;
    while( (connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t*)&len))  || i < 5)
    {
        puts("Connection accepted");
        pthread_create(&thread[i], NULL, clientThread, &connfd);

        //Reply to the client

    }
}
