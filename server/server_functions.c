//
// Created by augusto on 06/08/2022.
//
#include "../lib/helper.h"
#include "server.h"
#include "server_functions.h"
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>


void upload(int socket, char* path) {
    printf("upload function\n");
    char fileName[FILENAMESIZE];
    recv(socket, fileName, sizeof(fileName), 0);
    char* filePath = strcatSafe(path, fileName);


    printf("Receiving file: %s\n", filePath);
    receiveFile(socket, filePath);
    free(filePath);
}

void download(int socket, char* path) {
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