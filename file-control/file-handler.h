//
// Created by augusto on 24/07/2022.
//
#ifndef DROPBOX_SISOP2_FILE_HANDLER_H
#define DROPBOX_SISOP2_FILE_HANDLER_H
#include "../lib/helper.h"

#include <stdlib.h>


#define MAX_EVENTS 1024  /* Maximum number of events to process*/
#define LEN_NAME 16  /* Assuming that the length of the filename won't exceed 16 bytes*/
#define EVENT_SIZE  ( sizeof (struct inotify_event) ) /*size of one event*/
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME ))

void* watchDir(void* args);
char* getuserDirPath(char* username);
void sig_handler(int sig);

typedef struct sync_dir_conn {
    int *socket;
    int *listenerSocket;
} sync_dir_conn;

typedef struct watch_dir_argument {
    char* dirPath;
    sync_dir_conn* socketConnList;
    pthread_mutex_t* userSem;
    received_file_list *filesReceived;
} watch_dir_argument;
#endif //DROPBOX_SISOP2_FILE_HANDLER_H
