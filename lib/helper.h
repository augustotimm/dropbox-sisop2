//
// Created by augusto on 02/08/2022.
//

#ifndef DROPBOX_SISOP2_HELPER_H
#define DROPBOX_SISOP2_HELPER_H
#include "./utlist.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint-gcc.h>
#include <semaphore.h>
#include "../server/server_functions.h"

#define FILENAMESIZE 64
#define KBYTE 1024
#define BUFFERSIZE 16
#define EXIT 4
#define SYNC 3
#define LIST 2
#define DOWNLOAD 1
#define UPLOAD 0
#define USERSESSIONNUMBER 2
#define SERVERPORT 8889
#define SYNCPORT 9998


#define USERNAMESIZE 64

#define OUTOFSYNCERROR -99

#define CLIENTSOCKET 0
#define SYNCSOCKET 1


static const char endCommand[] = "\nend\n";
static const char commands[5][13] = {"upload", "download", "list local", "sync", "exit"};
static const char socketTypes[2][8] = {"client", "syncdir"};


typedef struct d_thread {
    pthread_t thread;
    bool isThreadComplete;
} d_thread;

typedef struct socket_conn_list {
    int socket;
    struct socket_conn_list *prev, *next;
} socket_conn_list;

typedef struct user_t {
    d_thread* clientThread[USERSESSIONNUMBER];
    d_thread watchDirThread;
    sem_t userAccessSem;
    socket_conn_list* syncSocketList;
    char* username;
} user_t;

typedef struct user_list {
    user_t user;
    struct user_list *next, *prev;
} user_list;

typedef struct thread_argument {
    bool* isThreadComplete;
    void *argument;
} thread_argument;

typedef struct client_thread_argument {
    bool* isThreadComplete;
    int socket;
    char* clientDirPath;
    sem_t* userAccessSem;
} client_thread_argument;

typedef struct file_info {
    char* fileName;
    struct tm* lastAccessDate;   /* time of last access */
    struct tm*  lastModificationDate;   /* time of last modification */
    struct tm*  lastChangeDate;   /* time of last status change */
} file_info;

typedef struct file_info_list {
    file_info fileInfo;
    struct file_info_list *next, *prev;
} file_info_list;

char* strcatSafe(char* head, char* tail);
socket_conn_list* initSocketConnList(int socket);

//server comunication functions
int sendFile(int socket, char* filepath);
int receiveFile(int socket, char* fileName);
int listenForSocketMessage(int socket, char* clientDirPath, sem_t* dirSem);


//file information functions
file_info_list* getListOfFiles(char* pathname);
void printFileInfos(file_info fileInfo);
void printFileInfoList(file_info_list* fileInfoList);

void deleteFile(char* filename, char* path);
#endif //DROPBOX_SISOP2_HELPER_H
