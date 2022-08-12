//
// Created by augusto on 04/08/2022.
//
#include "../lib/helper.h"
#include "user.h"
#include "server.h"
#include <string.h>
#include "../file-control/file-handler.h"
#include "server_globals.h"
#define OUTOFSESSION -90

void createWatchDir(user_t* user);
void freeUserList(user_list* userList);
socket_conn_list* addSocket(socket_conn_list* head, int socket, struct in_addr ipAddr, bool isListener);

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

    socket_conn_list *currentConn = NULL, *connTmp = NULL;
    DL_FOREACH_SAFE(user->syncSocketList, currentConn, connTmp) {
        close(currentConn->socket);

        DL_DELETE(user->syncSocketList, currentConn);
        free(currentConn);
    }

    received_file_list *currentFile = NULL, *fileTmp = NULL;
    DL_FOREACH_SAFE(user->filesReceived, currentFile, fileTmp) {
        free(currentFile->fileName);

        DL_DELETE(user->filesReceived, currentFile);
        free(currentFile);
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

client_thread_argument* createClientThreadArgument(bool* isThreadComplete, char* dirPath, int sessionSocket, sem_t* sem, received_file_list *filesReceived) {
    client_thread_argument* argument = (client_thread_argument*) calloc(1, sizeof(client_thread_argument));

    argument->isThreadComplete = isThreadComplete;
    argument->clientDirPath = calloc(strlen(dirPath)+1, sizeof(char));
    argument->filesReceived = filesReceived;
    strcpy(argument->clientDirPath, dirPath);
    argument->socket = sessionSocket;
    argument->userAccessSem = sem;

    return argument;
}

int startNewSession(user_list* user, int sessionSocket, char* userDirPath) {
    if(hasAvailableSession(user->user)) {
        if(hasAvailableSession(user->user)) {
            d_thread* newClientThread = (d_thread*) calloc(1, sizeof(d_thread));
            client_thread_argument* argument =
                    createClientThreadArgument(
                                &newClientThread->isThreadComplete,
                                userDirPath,
                                sessionSocket,
                                &user->user.userAccessSem,
                                user->user.filesReceived
                            );

            pthread_create(&newClientThread->thread, NULL, clientListen, argument);
            pthread_detach(newClientThread->thread);

            if(!addSession(&user->user, newClientThread)) {
                return OUTOFSESSION;
            }
            return 0;

        }
        else {
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
    newUser->user.filesReceived = NULL;

    DL_APPEND(newUser->user.filesReceived, createReceivedFile("\n", -1));


    newUser->user.username = (char*) calloc(strlen(username) + 1, sizeof(char));

    strcpy(newUser->user.username, username);
    sem_init(&newUser->user.userAccessSem, 0, 1);

    return newUser;
}

int startUserSession( char* username, int socket, struct in_addr ipAddr) {
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
    sem_wait(&user->user.userAccessSem);


    int result = startNewSession(user, socket, dirPath);
    free(dirPath);
    free(userSafe);
    sem_post(&newUser->user.userAccessSem);
    return result;
}

int compareSocketConn(socket_conn_list* a, socket_conn_list* b) {
    if(a->ipAddr.s_addr == b->ipAddr.s_addr)
        return 0;
    else
        return -1;
}

socket_conn_list* addSocket(socket_conn_list* head, int socket, struct in_addr ipAddr, bool isListener) {
    socket_conn_list *conn = NULL;
    socket_conn_list etmp;
    etmp.ipAddr = ipAddr;
    DL_SEARCH(head, conn, &etmp, compareSocketConn);

    if(conn == NULL) {
        socket_conn_list *newElement = initSocketConnList(socket, ipAddr, isListener);
        DL_APPEND(head, newElement);
    }
    else {
        if(isListener) {
            conn->listenerSocket = socket;
        }
        else {
            conn->socket = socket;
        }
    }
    return head;

}

void addSyncDir(int dirSocket, user_t* user, struct in_addr ipAddr) {
    addNewSocketConn(user, dirSocket, ipAddr, false);

    if(user->watchDirThread.isThreadComplete) {
        sem_wait(&user->userAccessSem);
        createWatchDir(user);
        sem_post(&user->userAccessSem);
    }
}

void createWatchDir(user_t* user) {
    char* dirPath = getuserDirPath(user->username);
    watch_dir_argument* argument = calloc(1, sizeof(watch_dir_argument));
    argument->dirPath = dirPath;
    argument->socketConnList = user->syncSocketList;
    argument->userSem = &user->userAccessSem;
    argument->filesReceived = user->filesReceived;
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

void addNewSocketConn(user_t* user, int socket, struct in_addr ipAddr, bool isListener) {

    sem_wait(&user->userAccessSem);
    socket_conn_list* newSocket = addSocket(user->syncSocketList, socket, ipAddr, isListener);
    if (user->syncSocketList == NULL) {
        user->syncSocketList = newSocket;
    }
    sem_post(&user->userAccessSem);
}
