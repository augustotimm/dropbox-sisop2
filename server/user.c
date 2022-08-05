//
// Created by augusto on 04/08/2022.
//
#include "../lib/helper.h"
#include "user.h"
#include "user.h"
#include "server.h"
#include <string.h>
#include "../file-control/file-handler.h"

int startWatchDir(user_t user, char* dirPath);

int  userCompare(user_list* a, user_list* b) {
    return strcmp(a->user.username,b->user.username);

}
int addSession(user_list* user) {
    if(user->user.clientThread) {

    }

}

user_list* createUser(char* username) {
    user_list* newUser = calloc(1, sizeof(user_list));
    newUser->prev = NULL;
    newUser->next = NULL;

    newUser->user.username = username;
    newUser->user.watchDirThread = NULL;

    return newUser;
}

void* startUserSession( void* voidUsername) {
    char* username = voidUsername;
    user_list* user = NULL;
    user_list etmp;

    etmp.user.username = username;

    DL_SEARCH(connectedUserListHead, user, &etmp, userCompare);
    if(user){
        addSession(user);
    }
    else{
        sem_wait( &userListWrite);
        DL_SEARCH(connectedUserListHead, user, &etmp, userCompare);
        if(user){
            addSession(user);
        }
        else{
            char* dirPath = getuserDirPath(username);
            user_list* newUser = createUser(username);
            startWatchDir(newUser->user, dirPath);
            DL_APPEND(connectedUserListHead, newUser);
        }

    }
}

int startWatchDir(user_t user, char* dirPath) {
    user.watchDirThread = (d_thread*) calloc(1, sizeof(d_thread));
    watch_dir_argument * argument = (watch_dir_argument*)calloc(1, sizeof(thread_argument));

    argument->isThreadComplete = &(user.watchDirThread->isThreadComplete);
    argument->isUserActive = user.isUserActive;
    argument->dirPath = dirPath;
    return pthread_create(&user.watchDirThread->thread, NULL, watchDir, (void*) argument);
}

char* getuserDirPath(char* username) {
    char* filePath = strcatSafe(path, username);
    char* dirPath = strcatSafe(filePath, "/");
    free(filePath);

    return dirPath;
}