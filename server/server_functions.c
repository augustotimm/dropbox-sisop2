//
// Created by augusto on 06/08/2022.
//
#include "../lib/helper.h"
#include "server.h"
#include "server_functions.h"
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>



void upload(int socket, char* path, sem_t* userSem) {
    printf("upload function\n");
    char fileName[FILENAMESIZE];
    recv(socket, fileName, sizeof(fileName), 0);
    char* filePath = strcatSafe(path, fileName);


    sem_wait(userSem);
    receiveFile(socket, filePath);
    sem_post(userSem);
    free(filePath);
}

void download(int socket, char* path, sem_t* userSem) {
    printf("download function");
    char fileName[FILENAMESIZE];
    bzero(fileName, sizeof(fileName));
    recv(socket, fileName, sizeof(fileName), 0);


    char* filePath = strcatSafe(path, fileName);
    sem_wait(userSem);
    sendFile(socket, filePath);
    sem_post(userSem);
    free(filePath);

}

void list() {
    printf("list function");
}

int sync_dir(int clientSocket) {

}