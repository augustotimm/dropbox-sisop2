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
    bzero(buff, sizeof(buff));
    recv(socket, buff, sizeof(buff), 0);
    if(strcmp(buff, endCommand) != 0) {
        write(socket, &endCommand, sizeof(endCommand));
        printf("Connection out of sync\n");
        printf("[upload] Expected end command signal but received: %s\n\n", buff);
        return;
    }
    bzero(buff, sizeof(buff));
    strcpy(buff, fileName);

    write(socket, buff, strlen(buff));
    bzero(buff, sizeof(buff));
    recv(socket, buff, sizeof(buff), 0);


    sendFile(socket, filePath);
    printf("FILE %s uploaded successfully\n", fileName);

}

char* download(int socket, char* path, received_file_list* list, bool appendFile) {
    printf("\ndownload start endC\n");

    write(socket, &endCommand, sizeof(endCommand));
    char fileName[FILENAMESIZE];
    bzero(fileName, sizeof(fileName));
    recv(socket, fileName, sizeof(fileName), 0);
    if(strcmp(fileName, endCommand) == 0) {
        printf("Connection out of sync\n");
        printf("Expected filename but received: endCommand\n\n");
        return NULL;
    }
    write(socket, &endCommand, sizeof(endCommand));


    char* filePath = strcatSafe(path, fileName);
    if(receiveFile(socket, filePath) == 0 && appendFile) {
        received_file_list* newFile = createReceivedFile(fileName, socket);
        DL_APPEND(list, newFile);
    }
    printf("File %s downloaded successfully\n\n", fileName);
    free(filePath);
    char *returnFileName = calloc(strlen(fileName) + 1, sizeof(char));
    strcpy(returnFileName, fileName);

    return returnFileName;
}

int receiveFile(int socket, char* fileName) {
    int fileSize, bytesLeft;
    FILE* file;
    char buff[KBYTE];
    char message[KBYTE];
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

    if (fileSize < 0)
    {
        printf("File not found in sender\n\n\n");
        return -1;
    }
    file = fopen(fileName, "wb");

    bytesLeft = fileSize;
    int byteCount = 0;
    int bytesRead = 0;
    while(bytesLeft > 0)
    {
        bzero(message, sizeof(message));
        byteCount = 0;
        while((byteCount < KBYTE && bytesLeft >= KBYTE) || (bytesLeft < KBYTE && byteCount < bytesLeft)){
            bytesRead = 0;
            bzero(buff, sizeof(buff));
            bytesRead = recv(socket, buff, KBYTE - byteCount, 0);

            for( int i = 0; i < bytesRead; i++) {
                message[byteCount + i] = buff[i];
            }
            byteCount += bytesRead;

        }

        // escreve no arquivo os bytes lidos
        if(bytesLeft > KBYTE)
        {
            fwrite(message, KBYTE, 1, file);
        }
        else
        {
            fwrite(message, bytesLeft, 1, file);
        }
        // decrementa a quantidade de bytes lidos
        bytesLeft -= KBYTE;
        // printf("bytes left: %d\n", bytesLeft);
    }
    write(socket, commands[DOWNLOAD], sizeof(commands[DOWNLOAD]));

    fclose(file);
    char endCommandBuff[BUFFERSIZE];
    bzero(endCommandBuff, sizeof(endCommandBuff));
    printf("\nreceiveFile end endC\n");

    int commandBytes = 0;
    recv(socket, endCommandBuff, sizeof(endCommand), MSG_WAITALL);


    printf("\n[receiveFile] last command signal received: %s bytesRead: %d **\n", endCommandBuff, commandBytes);

    if(strcmp(endCommandBuff, commands[UPLOAD]) != 0) {
        printf("Connection out of sync\n");
        printf("[receiveFile] Expected end command signal but received: %s\n\n", endCommandBuff);
        return OUTOFSYNCERROR;
    }
    return 0;
}

int sendFile(int socket, char* filepath) {
    int fileSize, byteCount;
    FILE* file;
    char buff[KBYTE];
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
            byteCount = -1;

            bzero(buff, sizeof(buff));

            fread(buff, KBYTE, 1, file);

            byteCount = write(socket, buff, KBYTE);

            if(byteCount < 0) {
                printf("ERROR sending file\n");
                printf("byteCount: %d\n\n");
                return -1;
            }
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
    bzero(buff, sizeof(buff));
    recv(socket, buff, KBYTE, 0);

    write(socket, &commands[UPLOAD], sizeof(commands[UPLOAD]));

    if(strcmp(buff, &commands[DOWNLOAD]) != 0) {
        printf("Connection out of sync\n");
        printf("[sendFile] Expected end command signal but received: %s\n\n", buff);
        return OUTOFSYNCERROR;
    }
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

void broadCastFile(socket_conn_list* socketList, int forbiddenSocket, char* fileName, char* clientDirPath) {
    socket_conn_list *current = NULL, *tmp = NULL;
    char* filePath = strcatSafe(clientDirPath, fileName);
    char buff[20];
    bzero(buff, sizeof(buff));
    DL_FOREACH_SAFE(socketList, current, tmp) {
        if (current->listenerSocket != forbiddenSocket) {
            recv(current->socket, buff, sizeof(buff), 0);
            if(strcmp(buff, commands[WAITING]) != 0) {
                printf("[broadCastFile] expected waiting command");
            }

            write(current->socket, &commands[UPLOAD], sizeof(commands[UPLOAD]));

            upload(current->socket, filePath, fileName);


        }
    }
    free(filePath);
}

void broadCastFileToBackups(char* fileName, char* clientDirPath, backup_conn_list *backupList, pthread_mutex_t* backupMutex, char* username) {
    char* filePath = strcatSafe(clientDirPath, fileName);
    char buff[20];
    bzero(buff, sizeof(buff));


    backup_conn_list* elt = NULL;
    pthread_mutex_lock(backupMutex);

    DL_FOREACH(backupList, elt) {
        recv(elt->socket, buff, sizeof(buff), 0);
        if(strcmp(buff, commands[WAITING]) != 0) {
            printf("expected waiting command");
        }

        write(elt->socket, &commands[UPLOAD], sizeof(commands[UPLOAD]));

        bzero(buff, sizeof(buff));
        recv(elt->socket, buff, sizeof(buff), 0);
        if(strcmp(buff, endCommand) != 0) {
            printf("[broadCastFileToBackups] expected endCommand command");
        }

        write(elt->socket, username, strlen(username));

        recv(elt->socket, buff, sizeof(buff), 0);
        if(strcmp(buff, endCommand) != 0) {
            printf("[broadCastFileToBackups] expected endCommand command");
        }

        upload(elt->socket, filePath, fileName);
    }
    pthread_mutex_unlock(backupMutex);

    free(filePath);
}


void broadCastDelete(socket_conn_list* socketList, int forbiddenSocket, char* fileName) {
    socket_conn_list *current = NULL, *tmp = NULL;
    char buff[20];
    bzero(buff, sizeof(buff));
    DL_FOREACH_SAFE(socketList, current, tmp) {
        if (current->listenerSocket != forbiddenSocket) {
            recv(current->socket, buff, sizeof(buff), 0);
            if(strcmp(buff, commands[WAITING]) != 0) {
                printf("expected waiting command");
            }
            write(current->socket, &commands[DELETE], sizeof(commands[DELETE]));
            write(current->socket, fileName, strlen(fileName));

        }
    }

}

void broadCastDeleteToBackups(char* fileName, backup_conn_list *backupList, pthread_mutex_t* backupMutex, char* username) {
    backup_conn_list *current = NULL, *tmp = NULL;
    char buff[20];
    bzero(buff, sizeof(buff));
    backup_conn_list* elt = NULL;
    pthread_mutex_lock(backupMutex);

    DL_FOREACH(backupList, elt) {
        recv(elt->socket, buff, sizeof(buff), 0);
        if(strcmp(buff, commands[WAITING]) != 0) {
            printf("expected waiting command");
        }

        write(elt->socket, &commands[DELETE], sizeof(commands[DELETE]));
        recv(elt->socket, buff, sizeof(buff), 0);
        if(strcmp(buff, endCommand) != 0) {
            printf("[broadCastDeleteToBackups] expected endCommand command");
        }

        write(elt->socket, username, strlen(username));

        recv(elt->socket, buff, sizeof(buff), 0);
        if(strcmp(buff, endCommand) != 0) {
            printf("[broadCastDeleteToBackups] expected endCommand command");
        }

        write(elt->socket, fileName, strlen(fileName));
    }
    pthread_mutex_unlock(backupMutex);

}