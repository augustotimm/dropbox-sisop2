//
// Created by augusto on 06/08/2022.
//
#include "../lib/helper.h"

#ifndef DROPBOX_SISOP2_SERVER_FUNCTIONS_H
#define DROPBOX_SISOP2_SERVER_FUNCTIONS_H

void upload(int socket, char* filePath, char* fileName);
char* download(int socket, char* path, received_file_list* list, bool appendFile);

void list();

int sendFile(int socket, char* filepath);
int receiveFile(int socket, char* fileName);
int uploadAllFiles(int socket, char* dirPath);
void broadCastFile(socket_conn_list* socketList, int forbiddenSocket, char* fileName, char* clientDirPath);
void broadCastDelete(socket_conn_list* socketList, int forbiddenSocket, char* fileName);

void broadCastFileToBackups(char* fileName, char* clientDirPath, socket_conn_list *backupList, pthread_mutex_t* backupMutex, char* username);
void broadCastDeleteToBackups(char* fileName, socket_conn_list *backupList, pthread_mutex_t* backupMutex, char* username);
#endif //DROPBOX_SISOP2_SERVER_FUNCTIONS_H
