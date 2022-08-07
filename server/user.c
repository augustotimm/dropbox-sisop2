//
// Created by augusto on 04/08/2022.
//
#include "../lib/helper.h"
#include "user.h"
#include "server.h"
#include <string.h>
#include "../file-control/file-handler.h"
#define OUTOFSESSION -90

void createWatchDir(user_t* user);
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
    pthread_cancel(user->watchDirThread.thread);
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
    int i = 0;
    while(i < USERSESSIONNUMBER) {
        if(isSessionAvailable(*user, i)){
            user->clientThread[i] = clientThread;
            return true;
        }
        i++;
    }
    return false;
}

client_thread_argument* createClientThreadArgument(bool* isThreadComplete, char* dirPath, int sessionSocket) {
    client_thread_argument* argument = (client_thread_argument*) calloc(1, sizeof(thread_argument));

    argument->isThreadComplete = isThreadComplete;
    argument->clientDirPath = calloc(strlen(dirPath)+1, sizeof(char));
    strcpy(argument->clientDirPath, dirPath);
    argument->socket = sessionSocket;

    return argument;
}

int startNewSession(user_list* user, int sessionSocket, char* userDirPath) {
    if(hasAvailableSession(user->user)) {
        sem_wait( &(user->user.userAccessSem));
        if(hasAvailableSession(user->user)) {
            d_thread* newClientThread = (d_thread*) calloc(1, sizeof(d_thread));
            client_thread_argument* argument =
                    createClientThreadArgument(
                                &newClientThread->isThreadComplete,
                                userDirPath,
                                sessionSocket
                            );

            pthread_create(&newClientThread->thread, NULL, clientConnThread, argument);
            pthread_detach(newClientThread->thread);

            if(!addSession(&user->user, newClientThread)) {
                sem_post(&(user->user.userAccessSem));
                return OUTOFSESSION;
            }
            sem_post(&(user->user.userAccessSem));
            return 0;

        }
        else {
            sem_post(&(user->user.userAccessSem));
            return OUTOFSESSION;
        }
    }
    return OUTOFSESSION;

}

user_list* createUser(char* username) {
    user_list* newUser = calloc(1, sizeof(user_list));
    newUser->prev = NULL;
    newUser->next = NULL;
    newUser->user.clientThread[0] = NULL;
    newUser->user.clientThread[1] = NULL;
    newUser->user.syncSocketList = NULL;
    newUser->user.watchDirThread.isThreadComplete = true;

    newUser->user.username = (char*) calloc(strlen(username) + 1, sizeof(char));

    strcpy(newUser->user.username, username);
    sem_init(&newUser->user.userAccessSem, 0, 1);

    return newUser;
}

int startUserSession( char* username, int socket) {
    user_list* user = NULL;
    user_list etmp;
    char* userSafe = strcatSafe(username, "\0");
    char* dirPath = getuserDirPath(userSafe);

    etmp.user.username = userSafe;
    user_list* newUser = createUser(userSafe);
    pthread_mutex_lock( &connectedUsersMutex);
    DL_SEARCH(connectedUserListHead, user, &etmp, userCompare);
    if(!user){
        DL_APPEND(connectedUserListHead, newUser);
        user = newUser;
    }

    pthread_mutex_unlock(&connectedUsersMutex);
    int result = startNewSession(user, socket, dirPath);
    free(dirPath);
    free(userSafe);
    return result;
}

void addSyncDir(int dirSocket, user_t* user) {
    sem_wait(&user->userAccessSem);
    socket_conn_list* newElement = initSocketConnList(dirSocket);
    DL_APPEND(user->syncSocketList, newElement);
    if(user->watchDirThread.isThreadComplete) {
        createWatchDir(user);
    }
    sem_post(&user->userAccessSem);
}

void createWatchDir(user_t* user) {
    char* dirPath = getuserDirPath(user->username);
    watch_dir_argument* argument = calloc(1, sizeof(watch_dir_argument));
    argument->dirPath = dirPath;
    argument->socketConnList = user->syncSocketList;
    user->watchDirThread.isThreadComplete = false;

    pthread_create(&user->watchDirThread.thread, NULL, watchDir, argument);
}

user_list* findUser(char* username){
    user_list* user = NULL;
    user_list etmp;
    etmp.user.username = username;

    pthread_mutex_lock( &connectedUsersMutex);
    DL_SEARCH(connectedUserListHead, user, &etmp, userCompare);
    pthread_mutex_unlock( &connectedUsersMutex);
    return user;
}
