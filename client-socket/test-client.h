//
// Created by augusto on 03/08/2022.
//
#include "../lib/helper.h"


#ifndef DROPBOX_SISOP2_TEST_CLIENT_H
#define DROPBOX_SISOP2_TEST_CLIENT_H
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

extern char username[USERNAMESIZE];
extern char* sessionCode;
extern sync_dir_conn* socketConn;
extern pthread_mutex_t socketConnMutex;
extern pthread_t listenSyncThread;
extern pthread_t clientThread;
extern char path[KBYTE];
extern received_file_list *filesReceived;
extern pthread_mutex_t syncDirSem;
extern pthread_cond_t exitCond;

#endif //DROPBOX_SISOP2_TEST_CLIENT_H
