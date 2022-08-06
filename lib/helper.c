//
// Created by augusto on 02/08/2022.
//
#include "helper.h"
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>

int getFileSize(FILE *ptrfile);

thread_list* initThreadListElement() {
    thread_list* newElement = calloc(1, sizeof(thread_list));
    newElement->isThreadComplete = false;
    newElement->next = NULL;
    newElement->prev = NULL;

    return newElement;
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
    write(socket, endCommand, sizeof(endCommand));
    return 0;
}

int getFileSize(FILE *ptrfile)
{
    int size;

    fseek(ptrfile, 0L, SEEK_END);
    size = ftell(ptrfile);

    rewind(ptrfile);

    return size;
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

    recv(socket, buff, KBYTE, 0);
    if(strcmp(buff, endCommand) != 0) {
        printf("Connection out of sync\n");
        printf("Expected end command signal but received: %s\n\n", buff);
        return OUTFOSYNCERROR;
    }

    printf("File %s has been downloaded\n\n", fileName);
    return 0;
}

char* strcatSafe(char* head, char* tail) {
    char* destiny = calloc(strlen(head) + strlen(tail), sizeof(char));
    strcpy(destiny, head);
    strcat(destiny, tail);
    return destiny;
}
