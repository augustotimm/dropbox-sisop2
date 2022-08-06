//
// Created by augusto on 24/07/2022.
//
#include "file-handler.h"
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct stat info;
struct tm* tm_info;
time_t  epoch_time;

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
    char* filePath = strcatSafe(rootPath, username);
    char* dirPath = strcatSafe(filePath, "/");
    free(filePath);
    struct stat st = {0};
    if(stat(dirPath, &st) == -1) {
        mkdir(dirPath, 0700);
    }

    return dirPath;
}


void* watchDir(void* args){

    char* pathToDir = (char*)args;

    signal(SIGINT,sig_handler);

    /* Step 1. Initialize inotify */
    fd = inotify_init();


    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)  // error checking for fcntl
        exit(2);

    /* Step 2. Add Watch */
    wd = inotify_add_watch(fd,pathToDir,IN_MODIFY | IN_CREATE | IN_DELETE);

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
            thread_list* newElement;
            struct inotify_event *event = (struct inotify_event *) &buffer[i];

            if(event->len){
                char* filePath = getFilePath(pathToDir, event->name);
                newElement = initThreadListElement();
                thread_argument* fileEventArgument = (thread_argument*)calloc(1, sizeof(thread_argument));
                fileEventArgument->isThreadComplete = &(newElement->isThreadComplete);
                fileEventArgument->argument = (void*) filePath;
                if ( event->mask & IN_CREATE ) {
                    if ( event->mask & IN_ISDIR ) {
                        newElement = initThreadListElement();
                        printf( "The file %s was created.\n", event->name );
                        // pthread_create(&(newElement->thread), NULL, createdDir, fileEventArgument);
                        // DL_APPEND(threadList, newElement);
                    }
                    else {
                        // pthread_create(&(newElement->thread), NULL, createdFile, fileEventArgument);
                        printf( "The file %s was created.\n", event->name );
                        // DL_APPEND(threadList, newElement);
                    }
                }
                else if ( event->mask & IN_DELETE ) {
                    if ( event->mask & IN_ISDIR ) {
                        printf( "The directory %s was deleted.\n", event->name );
                    }
                    else {
                        printf( "The file %s was deleted.\n", event->name );
                    }
                }
                else if ( event->mask & IN_MODIFY ) {
                    if ( event->mask & IN_ISDIR ) {
                        printf( "The directory %s was modified.\n", event->name );
                    }
                    else {
                        printf( "The file %s was modified.\n", event->name );
                    }
                }
                if ( event->mask & IN_CLOSE_WRITE ) {
                    printf( "The directory %s IN_CLOSE_WRITE.\n", event->name );
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