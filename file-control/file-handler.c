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

struct stat info;
time_t  epoch_time;

//TODO change to relative path
char rootPath[KBYTE] = "/home/augusto/repositorios/ufrgs/dropbox-sisop2/watch_folder/";

time_t getFileLastModifiedEpoch(char* pathname) {
    char buffer[16];

    stat(pathname, &info);
    epoch_time = time(&info.st_mtime);

    return epoch_time;
}

int fd,wd;

time_t getFileLastModifiedEpoch(char* pathname);

char* getFilePath(char* path, char* fileName);

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
                socket_conn_list* elt = NULL;
                // TODO Create event threads
                if ( event->mask & IN_CLOSE_WRITE || event->mask & IN_CREATE || event->mask & IN_MOVED_TO) {
                    char* filePath = strcatSafe(pathToDir, event->name);
                    sem_wait(argument->userSem);
                    DL_FOREACH(argument->socketConnList, elt) {
                        upload(elt->socket, filePath);
                    }
                    sem_post(argument->userSem);
                    free(filePath);
                    printf( "The file %s was created.\n", event->name );
                } else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                    printf( "The file %s was removed.\n", event->name );
                }

                }
            i += EVENT_SIZE + event->len;
        }
    }
}

char* getFilePath(char* path, char* fileName) {
    char* filePath = calloc(strlen(fileName) + strlen(path), sizeof(char));
    strcpy(filePath, path);
    strcat(filePath, fileName);

    return filePath;
}