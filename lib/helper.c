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


int getFileSize(FILE *ptrfile);
time_t  modification;

socket_conn_list* initSocketConnList(int socket, struct in_addr ipaddr, bool isClient) {
    socket_conn_list* newElement = calloc(1, sizeof(socket_conn_list));
    if(isClient)
        newElement->listenerSocket = socket;
    else
        newElement->socket = socket;

    newElement->next = NULL;
    newElement->prev = NULL;
    newElement->ipAddr = ipaddr;

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

char* tmToIso(struct tm* time) {
    int y,M,d,h,m;
    float s;
    char* timestamp = calloc(BUFFERSIZE, sizeof(char ));
    y = time->tm_year + 1900; // Year since 1900
    M = time->tm_mon + 1;     // 0-11
    d = time->tm_mday;        // 1-31
    h = time->tm_hour;        // 0-23
    m = time->tm_min;         // 0-59
    s = time->tm_sec;

    snprintf(timestamp, "%d-%d-%dT%d:%d:%fZ", &y, &M, &d, &h, &m, &s);

    return timestamp;
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

int listenForSocketMessage(int socket, char* clientDirPath, sem_t* dirSem, received_file_list* filesList) {
    char currentCommand[13];
    char fileName[FILENAMESIZE];

    for (;;) {
        bzero(currentCommand, sizeof(currentCommand));
        bzero(fileName, sizeof(fileName));


        // read the message from client and copy it in buffer
        recv(socket, currentCommand, sizeof(currentCommand), 0);
        if(strcmp(currentCommand, commands[UPLOAD]) ==0 ) {
            sem_wait(dirSem);
            download(socket, clientDirPath, filesList);
            sem_post(dirSem);
        } else if(strcmp(currentCommand, commands[DOWNLOAD]) ==0 ) {
            recv(socket, fileName, sizeof(fileName), 0);
            sem_wait(dirSem);
            char* filePath = strcatSafe(clientDirPath, fileName);
            upload(socket, filePath, fileName);
            sem_post(dirSem);
        } else if(strcmp(currentCommand, commands[LIST]) ==0 ) {
            list();
        } else if(strcmp(currentCommand, commands[DELETE]) ==0 ) {
            recv(socket, fileName, sizeof(fileName), 0);
            sem_wait(dirSem);
            deleteFile(fileName, clientDirPath);
            write(socket, &endCommand, sizeof(endCommand));
            sem_post(dirSem);
        }

        if (strcmp(currentCommand, commands[EXIT]) == 0 || *currentCommand == "\0") {

            return 0;
        }
    }

}

struct tm  iso8601ToTM(char* timestamp) {
    int y,M,d,h,m;
    float s;
    sscanf(timestamp, "%d-%d-%dT%d:%d:%fZ", &y, &M, &d, &h, &m, &s);

    struct tm time = { 0 };
    time.tm_year = y - 1900; // Year since 1900
    time.tm_mon = M - 1;     // 0-11
    time.tm_mday = d;        // 1-31
    time.tm_hour = h;        // 0-23
    time.tm_min = m;         // 0-59
    time.tm_sec = (int)s;
    return time;
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

bool checkFileExists(char* filePath) {
    return access(filePath, F_OK) == 0;
}