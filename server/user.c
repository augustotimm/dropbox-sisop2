//
// Created by augusto on 04/08/2022.
//
#include "../lib/helper.h"
#include "user.h"
#include "user.h"
#include "server.h"
#include <string.h>
#include "../file-control/file-handler.h"
#define OUTOFSESSION -90

int startWatchDir(user_t user, char* dirPath);

int  userCompare(user_list* a, user_list* b) {
    return strcmp(a->user.username,b->user.username);

}

bool hasAvailableSession(user_t user) {
    return (user.clientThread[0] == NULL || user.clientThread[1] == NULL);
}

bool addSession(user_t user, d_thread* clientThread){
    if(user.clientThread[0] == NULL) {
        user.clientThread[0] = clientThread;
        return true;
    }
    if(user.clientThread[1] == NULL) {
        user.clientThread[1] = clientThread;
        return true;
    }

    return false;
}

int startSession(user_list* user, int sessionSocket) {
    if(hasAvailableSession(user->user)) {
        sem_wait( &(user->user.startSessionSem));
        if(hasAvailableSession(user->user)) {
            d_thread* clientThread = (d_thread*) calloc(1, sizeof(d_thread));
            // TODO args has to be the socket the client thread should use
            pthread_create(&clientThread->thread, NULL, watchDir, &sessionSocket);
            if(!addSession(user->user, clientThread)) {
                sem_post(&(user->user.startSessionSem));
                printf("User has reached limit of active sessions\n");
                return OUTOFSESSION;
            }
            sem_post(&(user->user.startSessionSem));
            return 1;

        }
        else {
            printf("User has reached limit of active sessions\n");
            sem_post(&(user->user.startSessionSem));
            return OUTOFSESSION;
        }
    }

}

user_list* createUser(char* username) {
    user_list* newUser = calloc(1, sizeof(user_list));
    newUser->prev = NULL;
    newUser->next = NULL;

    newUser->user.clientThread[0] = NULL;
    newUser->user.clientThread[1] = NULL;
    newUser->user.username = username;
    newUser->user.watchDirThread = NULL;
    sem_init(&newUser->user.startSessionSem, 0, 1);

    return newUser;
}

void* startUserSession( void* voidArg) {
    start_user_argument* argument = (start_user_argument*) voidArg;
    char* username = argument->username;
    user_list* user = NULL;
    user_list etmp;

    etmp.user.username = username;

    DL_SEARCH(connectedUserListHead, user, &etmp, userCompare);
    if(user){
        startSession(user, argument->socket);
    }
    else{
        sem_wait( &userListWrite);
        DL_SEARCH(connectedUserListHead, user, &etmp, userCompare);
        if(user){
            startSession(user, argument->socket);
        }
        else{
            char* dirPath = getuserDirPath(username);
            user_list* newUser = createUser(username);
            startWatchDir(newUser->user, dirPath);
            DL_APPEND(connectedUserListHead, newUser);
            sem_post(&userListWrite);
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