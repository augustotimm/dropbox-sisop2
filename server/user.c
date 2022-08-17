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
socket_conn_list* addSocket(socket_conn_list* head, int socket, char* sessionCode, bool isListener);

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

bool addSession(user_t* user, user_session_t* clientThread){
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

client_thread_argument* createClientThreadArgument(bool* isThreadComplete, char* dirPath, int sessionSocket, user_t* user) {
    client_thread_argument* argument = (client_thread_argument*) calloc(1, sizeof(client_thread_argument));

    argument->isThreadComplete = isThreadComplete;
    argument->clientDirPath = calloc(strlen(dirPath)+1, sizeof(char));
    strcpy(argument->clientDirPath, dirPath);
    argument->socket = sessionSocket;
    argument->user = user;

    return argument;
}

int startNewSession(user_list* user, int sessionSocket, char* userDirPath) {
    if(hasAvailableSession(user->user)) {
        user_session_t* newClientThread = (user_session_t*) calloc(1, sizeof(user_session_t));
        client_thread_argument* argument =
                createClientThreadArgument(
                        &newClientThread->isThreadComplete,
                        userDirPath,
                        sessionSocket,
                        &user->user
                );

        pthread_create(&newClientThread->thread, NULL, clientListen, argument);
        pthread_detach(newClientThread->thread);

        if(!addSession(&user->user, newClientThread)) {
            return OUTOFSESSION;
        }
        return 0;

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
    newUser->user.filesReceived = NULL;

    DL_APPEND(newUser->user.filesReceived, createReceivedFile("\n", -1));


    newUser->user.username = (char*) calloc(strlen(username) + 1, sizeof(char));
    pthread_mutex_init(&newUser->user.userAccessSem, NULL);
    strcpy(newUser->user.username, username);

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
    pthread_mutex_lock(&user->user.userAccessSem);


    int result = startNewSession(user, socket, dirPath);
    free(dirPath);
    free(userSafe);
    pthread_mutex_unlock(&newUser->user.userAccessSem);
    return result;
}

int compareSocketConn(socket_conn_list* a, socket_conn_list* b) {
    return strcmp(a->sessionCode, b->sessionCode);
}

socket_conn_list* addSocket(socket_conn_list* head, int socket, char* sessionCode, bool isListener) {
    socket_conn_list *conn = NULL;
    socket_conn_list etmp;
    etmp.sessionCode = sessionCode;
    DL_SEARCH(head, conn, &etmp, compareSocketConn);

    if(conn == NULL) {
        socket_conn_list *newElement = initSocketConnList(socket, sessionCode, isListener);
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

void addSyncDir(int dirSocket, user_t* user, char* sessionCode) {
    addNewSocketConn(user, dirSocket, sessionCode, false);
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

void addNewSocketConn(user_t* user, int socket, char* sessionCode, bool isListener) {

    pthread_mutex_lock(&user->userAccessSem);
    socket_conn_list* newSocket = addSocket(user->syncSocketList, socket, sessionCode, isListener);
    if (user->syncSocketList == NULL) {
        user->syncSocketList = newSocket;
    }
    pthread_mutex_unlock(&user->userAccessSem);
}
