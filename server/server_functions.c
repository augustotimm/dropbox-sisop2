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



void upload(int socket, char* filePath, char* fileName) {
    char buff[BUFFERSIZE];
    recv(socket, buff, sizeof(buff), 0);

    write(socket, fileName, strlen(fileName));

    printf("clientUpload function\n");
    sendFile(socket, filePath);
}

void download(int socket, char* path) {
    write(socket, &endCommand, sizeof(endCommand));
    printf("clientDownload function");
    char fileName[FILENAMESIZE];
    bzero(fileName, sizeof(fileName));
    recv(socket, fileName, sizeof(fileName), 0);


    char* filePath = strcatSafe(path, fileName);
    receiveFile(socket, filePath);
    free(filePath);

}

void list() {
    printf("list function");
}