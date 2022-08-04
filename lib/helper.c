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
    char buff[2*KBYTE];
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
    }
}

int getFileSize(FILE *ptrfile)
{
    int size;

    fseek(ptrfile, 0L, SEEK_END);
    size = ftell(ptrfile);

    rewind(ptrfile);

    return size;
}