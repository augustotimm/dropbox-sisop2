//
// Created by augusto on 04/08/2022.
//
#include "../lib/helper.h"
#include "user.h"
#include "server.h"
#include <string.h>
#include "../file-control/file-handler.h"
#define OUTOFSESSION -90

int startWatchDir(user_t* user, char* dirPath);

int  userCompare(user_list* a, user_list* b) {
    return strcmp(a->user.username,b->user.username);

}

bool hasAvailableSession(user_t user) {
    return (user.clientThread[0] == NULL || user.clientThread[1] == NULL);
}

void* killUser(void * voidUserList){
    user_list* user = (user_list*) voidUserList;
    printf("killing user: %s\n", user->user.username);
}

bool addSession(user_t* user, d_thread* clientThread){
    if(user->clientThread[0] == NULL) {
        user->clientThread[0] = clientThread;
        return true;
    }
    if(user->clientThread[1] == NULL) {
        user->clientThread[1] = clientThread;
        return true;
    }

    return false;
}

int startNewSession(user_list* user, int sessionSocket) {
    if(hasAvailableSession(user->user)) {
        sem_wait( &(user->user.startSessionSem));
        if(hasAvailableSession(user->user)) {
            d_thread* newClientThread = (d_thread*) calloc(1, sizeof(d_thread));
            // TODO args has to be the socket the client thread should use
            thread_argument* argument = (thread_argument*) calloc(1, sizeof(thread_argument));

            argument->isThreadComplete = &(newClientThread->isThreadComplete);
            argument->argument = (void*) sessionSocket;

            pthread_create(&newClientThread->thread, NULL, clientConnThread, argument);
            pthread_detach(newClientThread->thread);

            if(!addSession(&user->user, newClientThread)) {
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
    newUser->user.username = (char*) calloc(strlen(username), sizeof(char));

    strcpy(newUser->user.username, username);
    newUser->user.isUserActive = true;
    sem_init(&newUser->user.startSessionSem, 0, 1);

    return newUser;
}

int startUserSession( char* username, int socket) {
    user_list* user = NULL;
    user_list etmp;

    etmp.user.username = username;

    DL_SEARCH(connectedUserListHead, user, &etmp, userCompare);
    if(user){
        return startNewSession(user, socket);
    }
    else{
        sem_wait( &userListWrite);
        DL_SEARCH(connectedUserListHead, user, &etmp, userCompare);
        if(user){
            return startNewSession(user, socket);
        }
        else{
            char* dirPath = getuserDirPath(username);
            user_list* newUser = createUser(username);
            DL_APPEND(connectedUserListHead, newUser);
            int startedWatchingDir = startWatchDir(&(newUser->user), dirPath);
            sem_post(&userListWrite);
            return startNewSession(newUser, socket);
        }

    }
}

int startWatchDir(user_t* user, char* dirPath) {
    watch_dir_argument * argument = (watch_dir_argument*)calloc(1, sizeof(watch_dir_argument));
    argument->isThreadComplete = &(user->watchDirThread.isThreadComplete);
    argument->isUserActive = &(user->isUserActive);
    argument->dirPath = dirPath;
   return pthread_create(&user->watchDirThread.thread, NULL, watchDir, argument);
}

char* getuserDirPath(char* username) {
    char* filePath = strcatSafe(path, username);
    char* dirPath = strcatSafe(filePath, "/");
    free(filePath);

    return dirPath;
}