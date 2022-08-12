//
// Created by augusto on 24/07/2022.
//
#include "file-handler.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include<stdio.h>
#include<sys/inotify.h>
#include<unistd.h>
#include<signal.h>
#include<fcntl.h>
#include "../server/server_functions.h"
#include "../server/server_globals.h"
struct stat info;
time_t  epoch_time;
// /home/augusto/repositorios/ufrgs/dropbox-sisop2/watch_folder/
// /home/augusto/repositorios/ufrgs/dropbox-sisop2/client-socket/sync/
//TODO change to relative path
extern char rootPath[KBYTE];

int getSocketFromReceivedFile(received_file_list* head, char* fileName);
int findSyncDirSocket(socket_conn_list* head, int clientSocket);

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

    signal(SIGINT,sig_handler);

    /* Step 1. Initialize inotify */
    fd = inotify_init();


    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)  // error checking for fcntl
        exit(2);

    /* Step 2. Add Watch */
    wd = inotify_add_watch(fd,pathToDir,IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);

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

        /* Step 3. Read buffer*/
        length = read(fd,buffer,BUF_LEN);

        /* Step 4. Process the events which has occurred */
        while(i<length){
            struct inotify_event *event = (struct inotify_event *) &buffer[i];

            if(event->len){
                sem_wait(argument->userSem);
                socket_conn_list* elt = NULL;
                int receiverSocket = getSocketFromReceivedFile(argument->filesReceived, event->name);
                int forbiddenSocket = findSyncDirSocket(argument->socketConnList, receiverSocket);
                if ( event->mask & IN_CLOSE_WRITE || event->mask & IN_CREATE || event->mask & IN_MOVED_TO) {
                    printf( "The file %s was created.\n", event->name );

                    DL_FOREACH(argument->socketConnList, elt) {
                        if(elt->socket != forbiddenSocket) {
                            write(elt->socket, &commands[UPLOAD], sizeof(commands[UPLOAD]));
                            char* filePath = strcatSafe(pathToDir, event->name);
                            upload(elt->socket, filePath, event->name);
                            free(filePath);
                        }
                    }
                    printf( "The file %s was created.\n", event->name );
                } else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {


                    DL_FOREACH(argument->socketConnList, elt) {
                        if(elt->socket != forbiddenSocket) {
                            write(elt->socket, &commands[DELETE], sizeof(commands[DELETE]));
                            write(elt->socket, event->name, strlen(event->name));
                            char buff[BUFFERSIZE];
                            bzero(buff, sizeof(buff));
                            recv(elt->socket, buff, sizeof(buff), 0);
                        }
                    }
                    printf( "The file %s was removed.\n", event->name );
                }

                sem_post(argument->userSem);
            }
            i += EVENT_SIZE + event->len;
        }
    }
}


int fileNameCompare(received_file_list* a, received_file_list* b) {
    return strcmp(a->fileName,b->fileName);
}


int getSocketFromReceivedFile(received_file_list* head, char* fileName) {
    received_file_list *etmp = createReceivedFile(fileName, -1);
    received_file_list *file;
    DL_SEARCH(head, file, etmp, fileNameCompare);
    if(file != NULL) {
        return file->socketReceiver;
    }

    return -1;
}

int compareClientSocket(socket_conn_list* a, socket_conn_list* b) {
    if(a->listenerSocket == b->listenerSocket)
        return 0;
    else
        return false;
}

int findSyncDirSocket(socket_conn_list* head, int clientSocket) {
    socket_conn_list *conn = NULL;
    socket_conn_list etmp;
    etmp.listenerSocket = clientSocket;
    DL_SEARCH(head, conn, &etmp, compareClientSocket);
    if(conn == NULL)
        return -1;
    return conn->socket;
}

