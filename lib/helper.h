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

#define FILENAMESIZE 64
#define KBYTE 1024

#define EXIT 4
#define SYNC 3
#define LIST 2
#define DOWNLOAD 1
#define UPLOAD 0

#define USERNAMESIZE 64

#define OUTFOSYNCERROR -99

typedef struct request_t
{
    char file[200];
    int command;
} request_t;


typedef struct thread_list {
    pthread_t thread;
    bool isThreadComplete;
    struct thread_list *next, *prev;
} thread_list;

typedef struct thread_argument {
    bool* isThreadComplete;
    void *argument;
} thread_argument;

thread_list* initThreadListElement();
char* strcatSafe(char* head, char* tail);
//server comunication functions

int sendFile(int socket, char* filepath);
int receiveFile(int socket, char* fileName);
#endif //DROPBOX_SISOP2_HELPER_H
