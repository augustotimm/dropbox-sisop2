//
// Created by augusto on 04/08/2022.
//
#include "../lib/helper.h"
#include "user.h"
#include "server.h"
#include <string.h>
#include "../file-control/file-handler.h"
#include <sys/stat.h>

#define OUTOFSESSION -90

void freeUserList(user_list* userList);
socket_conn_list* addSocket(socket_conn_list* head, int socket, char* sessionCode, bool isListener);
char* getuserDirPath(char* username);

int userCompare(user_list* a, user_list* b) {
    return strcmp(a->user.username,b->user.username);
}

bool isSessionAvailable(user_t user, int sessionNumber) {
    return user.clientThread[sessionNumber] == NULL || user.clientThread[sessionNumber]->isThreadComplete;
}

bool isSessionOpen(user_t user, int sessionNumber) {
    if(!isPrimary)
        return user.clientThread[sessionNumber] != NULL;
    else
        return user.clientThread[sessionNumber] != NULL && !user.clientThread[sessionNumber]->isThreadComplete;
}

bool isSessionClosed(user_t user, int sessionNumber) {
    return user.clientThread[sessionNumber] != NULL && user.clientThread[sessionNumber]->isThreadComplete;
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

    free(user->userAccessSem);

    for(int i = 0; i < USERSESSIONNUMBER; i++ ) {
        if(user->clientThread[i] != NULL) {
            free(user->clientThread[i]->ipAddr);
            free(user->clientThread[i]);
            user->clientThread[i] = NULL;
        }
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
    if(isPrimary) {
        while (i < USERSESSIONNUMBER) {
            if (isSessionAvailable(*user, i)) {
                user->clientThread[i] = clientThread;
                return true;
            }
            i++;
        }
    } else {
        while (i < USERSESSIONNUMBER) {
            if (user->clientThread[i] == NULL) {
                user->clientThread[i] = clientThread;
                return true;
            }
            i++;
        }
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

int startNewSession(user_list* user, int sessionSocket, char* userDirPath, char* ipAddr, int port, char* sessionCode) {
    if(hasAvailableSession(user->user)) {
        user_session_t* newClientSession = (user_session_t*) calloc(1, sizeof(user_session_t));


        if(isPrimary) {
            newClientSession->isThreadComplete = false;
            newClientSession->sessionSocket = sessionSocket;

            client_thread_argument* argument =
                    createClientThreadArgument(
                            &newClientSession->isThreadComplete,
                            userDirPath,
                            sessionSocket,
                            &user->user
                    );

            pthread_create(&newClientSession->thread, NULL, clientListen, argument);
            pthread_detach(newClientSession->thread);
        } else newClientSession->isThreadComplete = true;

        newClientSession->sessionCode = strcatSafe(sessionCode, "\0");

        newClientSession->ipAddr = strcatSafe(ipAddr, "\0");
        newClientSession->frontEndPort = port;

        if(!addSession(&user->user, newClientSession)) {
            pthread_cancel(newClientSession);
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

    newUser->user.userAccessSem = calloc(1, sizeof(pthread_mutex_t));
    newUser->user.username = (char*) calloc(strlen(username) + 1, sizeof(char));
    pthread_mutex_init(newUser->user.userAccessSem, NULL);
    strcpy(newUser->user.username, username);

    return newUser;
}

int startUserSession( char* username, int socket, char* ipAddr, int port, char* sessionCode) {
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
    pthread_mutex_lock(user->user.userAccessSem);
    pthread_mutex_unlock(&connectedUsersMutex);



    int result = startNewSession(user, socket, dirPath, ipAddr, port, sessionCode);
    free(dirPath);
    free(userSafe);
    pthread_mutex_unlock(user->user.userAccessSem);
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

    pthread_mutex_lock(user->userAccessSem);
    socket_conn_list* newSocket = addSocket(user->syncSocketList, socket, sessionCode, isListener);
    if (user->syncSocketList == NULL) {
        user->syncSocketList = newSocket;
    }
    pthread_mutex_unlock(user->userAccessSem);
}

void freeSession(user_t* user, int sessionNumber) {
    free(user->clientThread[sessionNumber]->ipAddr);
    free(user->clientThread[sessionNumber]->sessionCode);
    free(user->clientThread[sessionNumber]);
    user->clientThread[sessionNumber] = NULL;
}

void closeUserSession(char* username, char* sessionCode) {
    user_list* userList = findUser(username);
    if(userList != NULL) {
        pthread_mutex_lock(userList->user.userAccessSem);
        for (int i = 0; i < USERSESSIONNUMBER; ++i) {
            if(userList->user.clientThread[i] != NULL) {
                if(strcmp(sessionCode, userList->user.clientThread[i]->sessionCode) == 0) {
                    freeSession(&userList->user, i);
                }
            }

        }

        if(!hasSessionOpen(userList->user)) {
            pthread_mutex_lock( &connectedUsersMutex);

            DL_DELETE(connectedUserListHead, userList);
            freeUserList(userList);

            pthread_mutex_unlock( &connectedUsersMutex);

        } else
            pthread_mutex_unlock(userList->user.userAccessSem);
    }
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