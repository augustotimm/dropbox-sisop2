//
// Created by augusto on 04/08/2022.
//
#include "../lib/helper.h"
#include "user.h"
#include "server.h"
#include <string.h>
#include "../file-control/file-handler.h"
#define OUTOFSESSION -90

void startWatchDir(user_t* user, char* dirPath);
void freeUserList(user_list* userList);

int  userCompare(user_list* a, user_list* b) {
    return strcmp(a->user.username,b->user.username);

}

bool isSessionAvailable(user_t user, int sessionNumber) {
    return user.clientThread[sessionNumber] == NULL || user.clientThread[sessionNumber]->isThreadComplete;
}

bool isSessionOpen(user_t user, int sessionNumber) {
    return user.clientThread[sessionNumber] != NULL && !user.clientThread[sessionNumber]->isThreadComplete;
}

bool hasSessionOpen(user_t user) {
    int i = 0;
    while(i < USERSESSIONNUMBER) {
        if(isSessionOpen(user, i)){
            return true;
        }
        i++;
    }
    return false;
}

bool hasAvailableSession(user_t user) {
    int i = 0;
    while(i < USERSESSIONNUMBER) {
        if(isSessionAvailable(user, i)){
            return true;
        }
        i++;
    }
    return false;
}

void freeUser(user_t* user) {
    free(user->username);
    user->username = NULL;
    for(int i = 0; i < USERSESSIONNUMBER; i++ ) {
        free(user->clientThread[i]);
        user->clientThread[i] = NULL;
    }
}

void freeUserList(user_list* userList){
    freeUser(&userList->user);
    free(userList);
    userList = NULL;
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
        sem_wait( &(user->user.userAccessSem));
        if(hasAvailableSession(user->user)) {
            d_thread* newClientThread = (d_thread*) calloc(1, sizeof(d_thread));
            client_thread_argument* argument = (client_thread_argument*) calloc(1, sizeof(thread_argument));

            argument->isThreadComplete = &(newClientThread->isThreadComplete);
            argument->socket = sessionSocket;

            pthread_create(&newClientThread->thread, NULL, clientConnThread, argument);
            pthread_detach(newClientThread->thread);

            if(!addSession(&user->user, newClientThread)) {
                sem_post(&(user->user.userAccessSem));
                printf("User has reached limit of active sessions\n");
                return OUTOFSESSION;
            }
            sem_post(&(user->user.userAccessSem));
            return 1;

        }
        else {
            printf("User has reached limit of active sessions\n");
            sem_post(&(user->user.userAccessSem));
            return OUTOFSESSION;
        }
    }

}

user_list* createUser(char* username) {
    user_list* newUser = calloc(1, sizeof(user_list));
    newUser->prev = NULL;
    newUser->next = NULL;
    newUser->canDie = true;
    newUser->user.clientThread[0] = NULL;
    newUser->user.clientThread[1] = NULL;
    newUser->user.username = (char*) calloc(strlen(username) + 1, sizeof(char));

    strcpy(newUser->user.username, username);
    newUser->user.isUserActive = true;
    sem_init(&newUser->user.userAccessSem, 0, 1);

    return newUser;
}

int startUserSession( char* username, int socket) {
    user_list* user = NULL;
    user_list etmp;

    etmp.user.username = username;

    pthread_mutex_lock( &connectedUsersMutex);
    DL_SEARCH(connectedUserListHead, user, &etmp, userCompare);
    if(user){
        user->canDie = true;
        pthread_mutex_unlock( &connectedUsersMutex);
        return startNewSession(user, socket);
    }
    else{
        char* dirPath = getuserDirPath(username);
        user_list* newUser = createUser(username);
        DL_APPEND(connectedUserListHead, newUser);
        pthread_mutex_unlock(&connectedUsersMutex);

        startWatchDir(&(newUser->user), dirPath);
        return startNewSession(newUser, socket);

    }
}

void startWatchDir(user_t* user, char* dirPath) {
    sem_wait( &(user->userAccessSem));
    pthread_create(&user->watchDirThread, NULL, watchDir, dirPath);
    sem_post(&(user->userAccessSem));
}

char* getuserDirPath(char* username) {
    char* filePath = strcatSafe(path, username);
    char* dirPath = strcatSafe(filePath, "/");
    free(filePath);

    return dirPath;
}