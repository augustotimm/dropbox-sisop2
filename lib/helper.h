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

#define FILENAMESIZE 64
#define KBYTE 1024

#define EXIT 4
#define SYNC 3
#define LIST 2
#define DOWNLOAD 1
#define UPLOAD 0
#define USERSESSIONNUMBER 2

#define USERNAMESIZE 64

#define OUTFOSYNCERROR -99
static const char endCommand[] = "\nend\n";



typedef struct thread_list {
    pthread_t thread;
    bool isThreadComplete;
    struct thread_list *next, *prev;
} thread_list;

typedef struct d_thread {
    pthread_t thread;
    bool isThreadComplete;
} d_thread;

typedef struct user_t {
    d_thread* clientThread[USERSESSIONNUMBER];
    pthread_t watchDirThread;
    sem_t userAccessSem;
    char* username;
    bool isUserActive;
    int userid;
} user_t;

typedef struct user_list {
    user_t user;
    bool canDie;
    struct user_list *next, *prev;
} user_list;

typedef struct thread_argument {
    bool* isThreadComplete;
    void *argument;
} thread_argument;

typedef struct client_thread_argument {
    bool* isThreadComplete;
    int socket;
} client_thread_argument;


thread_list* initThreadListElement();
char* strcatSafe(char* head, char* tail);
//server comunication functions

int sendFile(int socket, char* filepath);
int receiveFile(int socket, char* fileName);
#endif //DROPBOX_SISOP2_HELPER_H
