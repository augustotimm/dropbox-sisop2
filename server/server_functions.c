//
// Created by augusto on 06/08/2022.
//
#include "server.h"
#include "server_functions.h"
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/stat.h>
#include <dirent.h>


int getFileSize(char* filename)
{
    struct stat sb;

    stat(filename, &sb);

    return sb.st_size;
}


void upload(int socket, char* filePath, char* fileName) {
    char buff[BUFFERSIZE];
    recv(socket, buff, sizeof(buff), 0);
    if(strcmp(buff, endCommand) != 0) {
        write(socket, &endCommand, sizeof(endCommand));
        printf("Connection out of sync\n");
        printf("Expected end command signal but received: %s\n\n", buff);
        return;
    }
    strcpy(buff, fileName);

    write(socket, buff, strlen(buff));

    sendFile(socket, filePath);
    printf("FILE %s was uploaded\n", fileName);

}

void download(int socket, char* path, received_file_list* list) {
    write(socket, &endCommand, sizeof(endCommand));
    char fileName[FILENAMESIZE];
    bzero(fileName, sizeof(fileName));
    recv(socket, fileName, sizeof(fileName), 0);
    if(strcmp(fileName, endCommand) == 0) {
        printf("Connection out of sync\n");
        printf("Expected filename but received: endCommand\n\n");
        return;
    }


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
    bzero(buff, sizeof(buff));

    recv(socket, buff, KBYTE, 0);
    if(strcmp(buff, commands[UPLOAD]) != 0) {
        write(socket, endCommand, sizeof(endCommand));
        printf("Connection out of sync\n");
        printf("Expected upload command but received: %s\n\n", buff);
        return OUTOFSYNCERROR;
    }
    write(socket, &commands[DOWNLOAD], sizeof(commands[DOWNLOAD]));
bzero(buff, sizeof(buff));


    if(recv(socket, &fileSize, sizeof(fileSize), 0) < 0) {
        printf("Failure receiving filesize\n");
    }
    printf("File size: %d\n", fileSize);

    if (fileSize < 0)
    {
        printf("File not found in sender\n\n\n");
        return -1;
    }
    file = fopen(fileName, "wb");

    bytesLeft = fileSize;

    while(bytesLeft > 0)
    {
        printf("bytes left: %d\n", bytesLeft);
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
    char* buff = calloc(KBYTE, sizeof(char));
    bzero(buff, strlen(buff));

    printf("sending file: %s\n", filepath);

    write(socket, &commands[UPLOAD], sizeof(commands[UPLOAD]));

    recv(socket, buff, KBYTE, 0);
    if(strcmp(buff, commands[DOWNLOAD]) != 0) {
        write(socket, endCommand, sizeof(endCommand));
        printf("Connection out of sync\n");
        printf("Expected upload command but received: %s\n\n", buff);
        return OUTOFSYNCERROR;
    }
    bzero(buff, sizeof(buff));

    if (file = fopen(filepath, "rb"))
    {
        fileSize = getFileSize(filepath);

        // escreve estrutura do arquivo no socket
        byteCount = write(socket, &fileSize, sizeof(int));
        printf("File size: %d\n", fileSize);

        while(!feof(file) && fileSize > 0)
        {
            fread(buff, KBYTE, 1, file);

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
    free(buff);
    return 0;
}


void list() {
    printf("list function");
}

int uploadAllFiles(int socket, char* path) {

    DIR *dirPath;
    struct dirent *dir;

    dirPath = opendir(path);

    if(dirPath) {
        while ((dir = readdir(dirPath)) != NULL) {
            if (dir->d_type == DT_REG) { // verifica se é um arquivo
                char* filePath = strcatSafe(path, dir->d_name);
                write(socket, &commands[UPLOAD], sizeof(commands[UPLOAD]));
                char* fileName = strcatSafe(dir->d_name, "");
                upload(socket, filePath, fileName);
                free(filePath);
            }
        }
        closedir(dirPath);
    }
    write(socket, &commands[EXIT], sizeof(commands[EXIT]));

}