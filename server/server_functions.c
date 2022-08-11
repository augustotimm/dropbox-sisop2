//
// Created by augusto on 06/08/2022.
//
#include "server.h"
#include "server_functions.h"
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>

int getFileSize(FILE *ptrfile)
{
    int size;

    fseek(ptrfile, 0L, SEEK_END);
    size = ftell(ptrfile);

    rewind(ptrfile);

    return size;
}


void upload(int socket, char* filePath, char* fileName) {
    char buff[BUFFERSIZE];
    recv(socket, buff, sizeof(buff), 0);

    write(socket, fileName, strlen(fileName));

    printf("clientUpload function\n");
    sendFile(socket, filePath);
}

void download(int socket, char* path, received_file_list* list) {
    write(socket, &endCommand, sizeof(endCommand));
    char fileName[FILENAMESIZE];
    bzero(fileName, sizeof(fileName));
    recv(socket, fileName, sizeof(fileName), 0);


    char* filePath = strcatSafe(path, fileName);
    if(receiveFile(socket, filePath) == 0) {
        received_file_list* newFile = createReceivedFile(fileName, socket);
        DL_APPEND(list, newFile);
    }
    free(filePath);

}



int receiveFile(int socket, char* fileName) {
    int fileSize, bytesLeft;
    FILE* file;
    char buff[KBYTE];

    if(recv(socket, &fileSize, sizeof(fileSize), 0) < 0) {
        printf("Failure receiving filesize\n");
    }

    if (fileSize < 0)
    {
        printf("File not found in sender\n\n\n");
        return -1;
    }
    file = fopen(fileName, "wb");

    bytesLeft = fileSize;

    while(bytesLeft > 0)
    {
        recv(socket, buff, KBYTE, 0);

        // escreve no arquivo os bytes lidos
        if(bytesLeft > KBYTE)
        {
            fwrite(buff, KBYTE, 1, file);
        }
        else
        {
            fwrite(buff, bytesLeft, 1, file);
        }
        // decrementa a quantidade de bytes lidos
        bytesLeft -= KBYTE;
    }
    fclose(file);
    write(socket, endCommand, sizeof(endCommand));

    recv(socket, buff, KBYTE, 0);
    if(strcmp(buff, endCommand) != 0) {
        printf("Connection out of sync\n");
        printf("Expected end command signal but received: %s\n\n", buff);
        return OUTOFSYNCERROR;
    }

    printf("File %s has been downloaded\n\n", fileName);
    return 0;
}

int sendFile(int socket, char* filepath) {
    int fileSize, byteCount;
    FILE* file;
    char buff[KBYTE];
    bzero(buff, sizeof(buff));

    if (file = fopen(filepath, "rb"))
    {
        fileSize = getFileSize(file);

        // escreve estrutura do arquivo no socket
        byteCount = write(socket, &fileSize, sizeof(int));

        while(!feof(file))
        {
            fread(buff, sizeof(buff), 1, file);

            byteCount = write(socket, buff, KBYTE);
            if(byteCount < 0)
                printf("ERROR sending file\n");
        }
        fclose(file);
    }
        // arquivo não existe
    else
    {
        fileSize = -1;
        byteCount = write(socket, &fileSize, sizeof(fileSize));
        return -1;
    }
    recv(socket, buff, KBYTE, 0);
    if(strcmp(buff, endCommand) != 0) {
        printf("Connection out of sync\n");
        printf("Expected end command signal but received: %s\n\n", buff);
        return OUTOFSYNCERROR;
    }
    write(socket, endCommand, sizeof(endCommand));
    return 0;
}


void list() {
    printf("list function");
}