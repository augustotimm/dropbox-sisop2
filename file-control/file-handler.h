//
// Created by augusto on 24/07/2022.
//

#include <sys/stat.h>
#include<time.h>
#include<stdio.h>
#include<sys/inotify.h>
#include<unistd.h>
#include<stdlib.h>
#include<signal.h>
#include<fcntl.h>


#ifndef DROPBOX_SISOP2_FILE_HANDLER_H
#define DROPBOX_SISOP2_FILE_HANDLER_H
#include "../event-handler/event-handler.h"
#include "../lib/helper.h"


#define MAX_EVENTS 1024  /* Maximum number of events to process*/
#define LEN_NAME 16  /* Assuming that the length of the filename won't exceed 16 bytes*/
#define EVENT_SIZE  ( sizeof (struct inotify_event) ) /*size of one event*/
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME ))

time_t getFileLastModifiedEpoch(char* pathname);

void* watchDir(void* args);
char* getuserDirPath(char* username);
void sig_handler(int sig);


typedef struct watch_dir_argument {
    char* dirPath;
    socket_conn_list* socketConnList;
} watch_dir_argument;
#endif //DROPBOX_SISOP2_FILE_HANDLER_H
