//
// Created by augusto on 24/07/2022.
//
#include "../lib/utlist.h"
#include "file-handler.h"

struct stat info;
struct tm* tm_info;
time_t  epoch_time;

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

int main(int argc, char **argv){
    thread_list* threadList = NULL;
    thread_list* elt = NULL, *tmp = NULL;

    char *path_to_be_watched;
    signal(SIGINT,sig_handler);

    path_to_be_watched = "/home/augusto/repositorios/ufrgs/dropbox-sisop2/watch_folder/";

    /* Step 1. Initialize inotify */
    fd = inotify_init();


    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)  // error checking for fcntl
        exit(2);

    /* Step 2. Add Watch */
    wd = inotify_add_watch(fd,path_to_be_watched,IN_MODIFY | IN_CREATE | IN_DELETE);

    if(wd==-1){
        printf("Could not watch : %s\n",path_to_be_watched);
        return COULD_NOT_WATCH;
    }
    else{
        printf("Watching : %s\n",path_to_be_watched);
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
                char* filePath = getFilePath(path_to_be_watched, event->name);
                newElement = initThreadListElement();
                thread_argument* fileEventArgument = (thread_argument*)calloc(1, sizeof(thread_argument));
                fileEventArgument->isThreadComplete = &(newElement->isThreadComplete);
                fileEventArgument->argument = (void*) filePath;
                if ( event->mask & IN_CREATE ) {
                    if ( event->mask & IN_ISDIR ) {
                        newElement = initThreadListElement();

                        pthread_create(&(newElement->thread), NULL, createdDir, fileEventArgument);
                        DL_APPEND(threadList, newElement);
                    }
                    else {
                        pthread_create(&(newElement->thread), NULL, createdFile, fileEventArgument);
                        printf( "The file %s was created.\n", event->name );
                        DL_APPEND(threadList, newElement);
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
            }
            i += EVENT_SIZE + event->len;
        }
        
        DL_FOREACH_SAFE(threadList,elt,tmp) {
            if(elt->isThreadComplete == true) {
                printf("Thread completed running");
                DL_DELETE(threadList,elt);
                free(elt);
            }
        }
    }
}

char* getFilePath(char* path, char* fileName) {
    char* filePath = calloc(strlen(fileName) + strlen(path), sizeof(char));
    strcpy(filePath, path);
    strcat(filePath, fileName);

    return filePath;
}