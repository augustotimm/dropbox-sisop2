//
// Created by augusto on 24/07/2022.
//
#include "file-handler.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include<stdio.h>
#include<sys/inotify.h>
#include<unistd.h>
#include<signal.h>
#include<fcntl.h>
#include "../client-socket/front-end.h"
#include "../server/server_functions.h"
#include "../client-socket/test-client.h"

struct stat info;
time_t  epoch_time;

//TODO change to relative path
extern char rootPath[KBYTE];

int getSocketFromReceivedFile(received_file_list* head, char* fileName);
int findSyncDirSocket(sync_dir_conn conn, int clientSocket);
int fd,wd;

void sig_handler(int sig){

    /* Step 5. Remove the watch descriptor and close the inotify instance*/
    inotify_rm_watch( fd, wd );
    close( fd );
    exit( 0 );

}

char* getuserDirPath(char* username) {
    char slash[2] = "/";
    char* filePath = strcatSafe(rootPath, username);
    char* dirPath = strcatSafe(filePath, slash);
    free(filePath);
    struct stat st = {0};
    if(stat(dirPath, &st) == -1) {
        mkdir(dirPath, 0700);
    }

    return dirPath;
}


void* watchDir(void* args){
    watch_dir_argument* argument = (watch_dir_argument *) args;
    char* pathToDir = argument->dirPath;

    sync_dir_conn *conn = argument->socketConnList;

    signal(SIGINT,sig_handler);

    /* Step 1. Initialize inotify */
    fd = inotify_init();


    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)  // error checking for fcntl
        exit(2);

    /* Step 2. Add Watch */
    wd = inotify_add_watch(fd,pathToDir,IN_CLOSE_WRITE  | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);

    if(wd==-1){
        printf("Could not watch : %s\n",pathToDir);
        return NULL;
    }
    else{
        printf("Watching : %s\n",pathToDir);
    }


    while(1){

        int i=0,length;
        char buffer[BUF_LEN];
        char buff[20];

        /* Step 3. Read buffer*/
        length = read(fd,buffer,BUF_LEN);

        /* Step 4. Process the events which has occurred */
        while(i<length){
            struct inotify_event *event = (struct inotify_event *) &buffer[i];

            if(event->len){
                // printf("EVENT MASK: %d\n", event->mask);
                pthread_mutex_lock(argument->userSem);
                int receiverSocket = getSocketFromReceivedFile(argument->filesReceived, event->name);
                int forbiddenSocket = findSyncDirSocket(*conn, receiverSocket);
                // printf("forbidden socket: %d\n", forbiddenSocket);
                if (event->mask & IN_CLOSE_WRITE || event->mask & IN_MOVED_TO) {
                    printf( "The file %s was created.\n", event->name );
                    pthread_mutex_lock(&isConnectionOpenMutex);
                    pthread_mutex_unlock(&isConnectionOpenMutex);

                    if(*conn->socket != forbiddenSocket) {
                        bzero(buff, sizeof(buff));
                        recv(*conn->socket, buff, sizeof(buff), 0);
                        if(strcmp(buff, commands[WAITING]) != 0) {
                            printf("\n[watchDir upload] expected waiting command received: %s\n", buff);
                        }
                        printf("sending file %s to socket %d\n", event->name, *conn->socket);
                        write(*conn->socket, &commands[UPLOAD], sizeof(commands[UPLOAD]));
                        char* filePath = strcatSafe(pathToDir, event->name);
                        upload(*conn->socket, filePath, event->name);
                        free(filePath);
                    }
                } else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {

                    pthread_mutex_lock(&isConnectionOpenMutex);
                    pthread_mutex_unlock(&isConnectionOpenMutex);

                    printf( "The file %s was removed.\n", event->name );
                    bzero(buff, sizeof(buff));
                    recv(*conn->socket, buff, sizeof(buff), 0);
                    if(strcmp(buff, commands[WAITING]) != 0) {
                        printf("\n[watchDir delete] expected waiting command received: %s\n", buff);
                    }
                    if(*conn->socket != forbiddenSocket) {
                        write(*conn->socket, &commands[DELETE], sizeof(commands[DELETE]));
                        write(*conn->socket, event->name, strlen(event->name));
                        char buff[BUFFERSIZE];
                        bzero(buff, sizeof(buff));
                        recv(*conn->socket, buff, sizeof(buff), 0);
                    }
                }

                pthread_mutex_unlock(argument->userSem);
            }
            i += EVENT_SIZE + event->len;
        }
    }
}

int getSocketFromReceivedFile(received_file_list* head, char* fileName) {
    received_file_list *tmp = NULL, *currentFile = NULL;
    DL_FOREACH_SAFE(head, currentFile, tmp ) {
        if(strcmp(currentFile->fileName, fileName) == 0) {
            DL_DELETE(head, currentFile);
            int socket = currentFile->socketReceiver;
            free(currentFile->fileName);
            return socket;
        }
    }
    return -1;
}
int findSyncDirSocket(sync_dir_conn conn, int clientSocket) {

    if(*conn.listenerSocket == clientSocket)
        return *conn.socket;
    return -1;
}

