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
#include <netinet/in.h>


#define FILENAMESIZE 256
#define KBYTE 1024
#define BUFFERSIZE 30

#define USERCLOSE 9
#define USERCONN 8
#define WAITING 7
#define DOWNLOADALL 6
#define DELETE 5
#define EXIT 4
#define SYNC 3
#define LIST 2
#define DOWNLOAD 1
#define UPLOAD 0
#define USERSESSIONNUMBER 2
#define SYNCLISTENERPORT 7777
#define SERVERPORT 8888
#define SYNCPORT 9999


#define USERNAMESIZE 64

#define OUTOFSYNCERROR -99

#define CLIENTSOCKET 0
#define SYNCSOCKET 1
#define SYNCLISTENSOCKET 2
#define BACKUPSOCKET 3
#define ELECTIONSOCKET 4
#define ELECTIONCOORDSOCKET 5

#define DEAD 0
#define NEWPRIMARY 0


static const char endCommand[] = "\nend\n";
static const char continueCommand[] = "\ncontinue\n";
static const char frontEndCommands [2][13] = {
        "dead",
        "new primary"
};

static const char commands[10][13] = {
        "upload",
        "download",
        "list local",
        "sync",
        "exit",
        "delete",
        "download all",
        "waiting",
        "user conn",
        "user close"
};
static const char socketTypes[6][13] = {
        "client",
        "syncdir",
        "synclisten",
        "backup",
        "election",
        "coordinator"
};

typedef struct received_file_list {
    int socketReceiver;
    char* fileName;
    struct received_file_list *next, *prev;
} received_file_list;

typedef struct user_session_t {
    pthread_t thread;
    bool isThreadComplete;
    char* ipAddr;
    int frontEndPort;
    char* sessionCode;
} user_session_t;

typedef struct socket_conn_list {
    int socket;
    int listenerSocket;
    char* sessionCode;
    struct socket_conn_list *prev, *next;
} socket_conn_list;

typedef struct backup_conn_list {
    int socket;
    struct backup_conn_list *prev, *next;
} backup_conn_list;


typedef struct user_t {
    user_session_t* clientThread[USERSESSIONNUMBER];
    pthread_mutex_t* userAccessSem;
    socket_conn_list* syncSocketList;
    char* username;
    // first element can never be null
    received_file_list *filesReceived;
} user_t;

typedef struct user_list {
    user_t user;
    struct user_list *next, *prev;
} user_list;

typedef struct client_thread_argument {
    bool* isThreadComplete;
    int socket;
    char* clientDirPath;
    user_t* user;
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
socket_conn_list* initSocketConnList(int socket, char* sessionCode, bool isClient);

//server comunication functions
int listenForSocketMessage(int socket, char* clientDirPath, user_t*  user, bool shouldBroadcast, backup_conn_list* backupList, pthread_mutex_t* backupMutex);


//file information functions
file_info_list* getListOfFiles(char* pathname);
void printFileInfos(file_info fileInfo);
void printFileInfoList(file_info_list* fileInfoList);
struct tm iso8601ToTM(char* timestamp);
received_file_list* createReceivedFile(char* name, int socket);
void freeReceivedFile(received_file_list* file);

void deleteFile(char* filename, char* path);
void freeFileInfo(file_info info);
file_info getFileInfo(char* path, char* fileName);

#endif //DROPBOX_SISOP2_HELPER_H
