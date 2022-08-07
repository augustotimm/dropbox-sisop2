//
// Created by augusto on 02/08/2022.
//
#include "helper.h"
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

int getFileSize(FILE *ptrfile);
struct stat info;
time_t  modification;
file_info infos;

thread_list* initThreadListElement() {
    thread_list* newElement = calloc(1, sizeof(thread_list));
    newElement->isThreadComplete = false;
    newElement->next = NULL;
    newElement->prev = NULL;

    return newElement;
}

socket_conn_list* initSocketConnList(int socket) {
    socket_conn_list* newElement = calloc(1, sizeof(socket_conn_list));
    newElement->socket = socket;
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
    char* destiny = calloc(strlen(head) + strlen(tail) + 1, sizeof(char));
    strcpy(destiny, head);
    strcat(destiny, tail);
    return destiny;
}

time_t getFileLastModified(char* pathname) {
    char buffer[16];

    stat(pathname, &info);
    modification = time(&info.st_mtime);



    return modification;
}

file_info_list* initFileInfoListElement() {
    file_info_list* newElement = calloc(1, sizeof(file_info_list));
    newElement->next = NULL;
    newElement->prev = NULL;

    return newElement;
}

file_info_list* getListOfFiles(char* path) {
    file_info_list* listOfFiles = NULL;
    DIR *dirPath;
    struct dirent *dir;
    char* pathname = NULL;

    dirPath = opendir(path);

    if(dirPath) {
        while ((dir = readdir(dirPath)) != NULL) {
            if (dir->d_type == DT_REG) { // verifica se é um arquivo
                pathname = strcatSafe(path, dir->d_name);
                stat(pathname, &info);
                infos.fileName = dir->d_name;
                infos.lastAccessDate = localtime (&info.st_atime);
                infos.lastChangeDate = localtime (&info.st_ctime);
                infos.lastModificationDate = localtime (&info.st_mtime);
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
    struct tm * timeinfo;

    timeinfo = localtime(fileInfo.lastModificationDate);
    strftime(fModDate, sizeof(fModDate), "%b %d %y %H:%M", timeinfo);
    timeinfo = localtime(fileInfo.lastAccessDate);
    strftime(fAccessDate, sizeof(fAccessDate), "%b %d %y %H:%M", timeinfo);
    timeinfo = localtime(fileInfo.lastChangeDate);
    strftime(fChangeDate, sizeof(fChangeDate), "%b %d %y %H:%M", timeinfo);
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
