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
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_EVENTS 1024  /* Maximum number of events to process*/
#define LEN_NAME 16  /* Assuming that the length of the filename won't exceed 16 bytes*/
#define EVENT_SIZE  ( sizeof (struct inotify_event) ) /*size of one event*/
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME ))

#define COULD_NOT_WATCH -13
int fd,wd;

time_t getFileLastModifiedEpoch(char* pathname);

void watchDir(char *pathToDir, bool* parentFinished, bool* threadFinished);

void sig_handler(int sig);
#endif //DROPBOX_SISOP2_FILE_HANDLER_H
