//
// Created by augusto on 02/08/2022.
//
#include "helper.h"
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "../server/server_functions.h"



socket_conn_list* initSocketConnList(int socket, char* sessionCode, bool isClient) {
    socket_conn_list* newElement = calloc(1, sizeof(socket_conn_list));
    if(isClient)
        newElement->listenerSocket = socket;
    else
        newElement->socket = socket;

    newElement->next = NULL;
    newElement->prev = NULL;
    newElement->sessionCode = calloc(strlen(sessionCode) + 1, sizeof(char));

    strcpy(newElement->sessionCode, sessionCode);

    return newElement;
}

char* strcatSafe(char* head, char* tail) {
    char* destiny = calloc(strlen(head) + strlen(tail) + 1, sizeof(char));
    strcpy(destiny, head);
    strcat(destiny, tail);
    return destiny;
}

file_info_list* initFileInfoListElement() {
    file_info_list* newElement = calloc(1, sizeof(file_info_list));
    newElement->next = NULL;
    newElement->prev = NULL;

    return newElement;
}

file_info getFileInfo(char* path, char* fileName) {
    file_info infos;
    struct stat info;
    char* pathname = strcatSafe(path, fileName);
    stat(pathname, &info);

    infos.fileName = fileName;
    infos.lastAccessDate = localtime(&info.st_atime);
    infos.lastChangeDate = localtime(&info.st_ctime);
    infos.lastModificationDate = localtime(&info.st_mtime);
    free(pathname);
    return infos;
}

file_info_list* getListOfFiles(char* path) {
    file_info_list* listOfFiles = NULL;
    DIR *dirPath;
    struct dirent *dir;

    dirPath = opendir(path);

    if(dirPath) {
        while ((dir = readdir(dirPath)) != NULL) {
            if (dir->d_type == DT_REG) { // verifica se é um arquivo
                file_info infos = getFileInfo(path, dir->d_name);

                //printFileInfos(infos);
                file_info_list* newList = initFileInfoListElement();
                newList->fileInfo = infos;
                DL_APPEND(listOfFiles, newList);
            }
        }
        closedir(dirPath);
    }
    return listOfFiles;
}

void printFileInfos(file_info fileInfo) {
    char fModDate[20], fAccessDate[20], fChangeDate[20];

    strftime(fModDate, sizeof(fModDate), "%b %d %y %H:%M", fileInfo.lastModificationDate);
    strftime(fAccessDate, sizeof(fAccessDate), "%b %d %y %H:%M", fileInfo.lastAccessDate);
    strftime(fChangeDate, sizeof(fChangeDate), "%b %d %y %H:%M", fileInfo.lastChangeDate);
    printf("%s - Last access: %s, Last modification : %s, Last change: %s\n",
           fileInfo.fileName,fAccessDate,fChangeDate,fModDate);
}

void printFileInfoList(file_info_list* fileInfoList) {
    file_info_list* infosList;
    DL_FOREACH(fileInfoList, infosList) {
        printFileInfos(infosList->fileInfo);
    }
}

void deleteFile(char* filename, char* path) {
    char* filePath = strcatSafe(path,filename);
    if( remove(filePath) == 0 )
        printf("%s file deleted successfully.\n",  filename);
    else {
        printf("Unable to delete the file\n");
    }
}

int listenForSocketMessage(int socket, char* clientDirPath, user_t*  user, bool shouldBroadcast, socket_conn_list* backupList, pthread_mutex_t* backupMutex) {
    char currentCommand[13];
    char fileName[FILENAMESIZE];

    for (;;) {
        bzero(currentCommand, sizeof(currentCommand));
        bzero(fileName, sizeof(fileName));

        printf("\n[listenForSocketMessage] WAITING\n");
        write(socket, &commands[WAITING], sizeof(commands[WAITING]));

        // read the message from client and copy it in buffer
        recv(socket, currentCommand, sizeof(currentCommand), 0);
        if(strcmp(currentCommand, commands[UPLOAD]) ==0 ) {
            pthread_mutex_lock(user->userAccessSem);
            char* fileName = download(socket, clientDirPath, user->filesReceived, !shouldBroadcast);
            if(fileName == NULL) {
                break;
            }
            if(shouldBroadcast) {
                broadCastFile(user->syncSocketList, socket, fileName, clientDirPath, backupList, backupMutex);

            }
            pthread_mutex_unlock(user->userAccessSem);
            free(fileName);
            printf("\n\n[listenForSocketMessage] finished upload\n\n");
        } else if(strcmp(currentCommand, commands[DOWNLOAD]) ==0 ) {
            recv(socket, fileName, sizeof(fileName), 0);
            pthread_mutex_lock(user->userAccessSem);
            char* filePath = strcatSafe(clientDirPath, fileName);
            upload(socket, filePath, fileName);
            pthread_mutex_unlock(user->userAccessSem);
        } else if(strcmp(currentCommand, commands[LIST]) ==0 ) {
            list();
        } else if(strcmp(currentCommand, commands[DELETE]) ==0 ) {
            recv(socket, fileName, sizeof(fileName), 0);
            pthread_mutex_lock(user->userAccessSem);
            deleteFile(fileName, clientDirPath);
            write(socket, &endCommand, sizeof(endCommand));

            if(shouldBroadcast) {
                broadCastDelete(user->syncSocketList, socket, fileName, backupList, backupMutex);
            }
            pthread_mutex_unlock(user->userAccessSem);
            printf("\n\b[listenForSocketMessage] finished delete\n\n");
        } else if(strcmp(currentCommand, commands[DOWNLOADALL]) ==0 ) {
            pthread_mutex_lock(user->userAccessSem);
            uploadAllFiles(socket, clientDirPath);
            pthread_mutex_unlock(user->userAccessSem);
        }

        if (strcmp(currentCommand, commands[EXIT]) == 0 || strlen(currentCommand) == 0) {

            return 0;
        }
    }
    return -1;
}

int backupListenForMessage(int socket, char* rootFolderPath) {
    char currentCommand[13];
    char fileName[FILENAMESIZE];
    char username[USERNAMESIZE];

    for (;;) {
        bzero(currentCommand, sizeof(currentCommand));
        bzero(fileName, sizeof(fileName));
        bzero(username, sizeof(username));


        printf("\n[listenForSocketMessage] WAITING\n");
        write(socket, &commands[WAITING], sizeof(commands[WAITING]));

        // read the message from client and copy it in buffer
        recv(socket, currentCommand, sizeof(currentCommand), 0);

        write(socket, &endCommand, strlen(endCommand));

        recv(socket, username, sizeof(username), 0);


        char* filePath = strcatSafe(rootFolderPath, username);
        char* clientDirPath = strcatSafe(filePath, "/");
        free(filePath);

        if(strcmp(currentCommand, commands[UPLOAD]) ==0 ) {
            char* fileName = download(socket, clientDirPath, NULL, false);
            if(fileName == NULL) {
                break;
            }
            free(fileName);
            printf("\n\n[listenForBackupMessage] finished upload\n\n");
        } else if(strcmp(currentCommand, commands[DELETE]) ==0 ) {
            recv(socket, fileName, sizeof(fileName), 0);
            deleteFile(fileName, clientDirPath);
            write(socket, &endCommand, sizeof(endCommand));

            printf("\n\b[listenForBackupMessage] finished delete\n\n");
        }
        free(clientDirPath);

        if (strcmp(currentCommand, commands[EXIT]) == 0 || strlen(currentCommand) == 0) {

            return 0;
        }
    }
    return -1;
}


received_file_list* createReceivedFile(char* name, int socket){
    received_file_list* newFile = (received_file_list*) calloc(1, sizeof(received_file_list));
    newFile->fileName = (char*) calloc(strlen(name), sizeof(char));
    strcpy(newFile->fileName, name);
    newFile->socketReceiver = socket;
    newFile->prev = NULL;
    newFile->next = NULL;

    return newFile;
}

void freeReceivedFile(received_file_list* file) {
    free(file->fileName);
}

void freeFileInfo(file_info info) {
    free(info.lastModificationDate);
    free(info.lastAccessDate);
    free(info.lastChangeDate);
}