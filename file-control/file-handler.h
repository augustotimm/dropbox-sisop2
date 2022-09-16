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


#endif //DROPBOX_SISOP2_FILE_HANDLER_H
