//
// Created by augusto on 03/08/2022.
//

#include "server.h"
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include "user.h"
#include "server_functions.h"

#define MAX 2048

#include <unistd.h>




int connectUser(int socket);
int connectSyncDir(int socket, char* username);
void* clientConnThread(void* voidArg)
{
    client_thread_argument* argument = voidArg;
    char* path = argument->clientDirPath;
    int socket = argument->socket;

    char currentCommand[13];
    bzero(currentCommand, sizeof(currentCommand));


    //waiting for command
    printf("waiting for first command\n");
    for (;;) {
        bzero(currentCommand, sizeof(currentCommand));

        // read the message from client and copy it in buffer
        recv(socket, currentCommand, sizeof(currentCommand), 0);
        printf("COMMAND: %s\n", currentCommand);

        if(strcmp(currentCommand, commands[UPLOAD]) ==0 ) {
            sem_wait(argument->userAccessSem);
            upload(socket, path );
            sem_post(argument->userAccessSem);
        } else if(strcmp(currentCommand, commands[DOWNLOAD]) ==0 ) {
            sem_wait(argument->userAccessSem);
            download(socket, path);
            sem_post(argument->userAccessSem);
        } else if(strcmp(currentCommand, commands[LIST]) ==0 ) {
            list();
        } else if(strcmp(currentCommand, commands[SYNC]) ==0 ) {
            sync_dir(socket);
        }



        if (strlen(currentCommand) == 0 || strcmp(currentCommand, commands[EXIT]) == 0) {
            printf("Server Exit...\n");
            close(socket);
            free(argument->clientDirPath);
            *argument->isThreadComplete = true;
            free(argument);
            pthread_cond_signal(&closedUserConnection);
            return NULL;
        }
    }

}
void writeMessageToSocket(int socket, char* message) {
    write(socket, message, strlen(message)+1);
}
void* newConnection(void* arg) {
    int socket = * (int*) arg;
    free(arg);
    char newSocketType[USERNAMESIZE];
    bzero(newSocketType, sizeof(newSocketType));
    recv(socket, newSocketType, sizeof(newSocketType), 0);
    connectUser(socket);

}

int connectSyncDir(int socket, char* username) {
    user_list* user = findUser(username);
    if(user == NULL) {
        // sync dir connection of user logged out
        close(socket);
        return -1;
    }
    addSyncDir(socket, &user->user);

}

int connectUser(int socket) {
    char username[USERNAMESIZE];
    char buff[BUFFERSIZE];
    bzero(username, USERNAMESIZE);
    recv(socket, username, USERNAMESIZE, 0);
    if(startUserSession(username, socket) != 0) {
        writeMessageToSocket(socket, "FALSE");
        close(socket);
        return OUTFOSYNCERROR;
    }

    bzero(buff, BUFFERSIZE);
    writeMessageToSocket(socket, "TRUE");

    // get username to find in connected usersList
    recv(socket, buff, BUFFERSIZE, 0);
    if(strcmp(buff, endCommand) != 0){
        close(socket);
        return OUTFOSYNCERROR;
    }
    connectSyncDir(socket, username);
    return 0;
}

void* userDisconnectedEvent(void *arg) {
    while(1) {
        user_list* currentUser = NULL, *userTmp = NULL;

        pthread_cond_wait(&closedUserConnection, &connectedUsersMutex);
        DL_FOREACH_SAFE(connectedUserListHead, currentUser, userTmp) {
            if(!hasSessionOpen(currentUser->user)) {
                sem_wait(&currentUser->user.userAccessSem);
                DL_DELETE(connectedUserListHead, currentUser);

                freeUserList(currentUser);
            }
        }
        pthread_mutex_unlock(&connectedUsersMutex);
    }
}


// Driver function
int main()
{
    pthread_mutex_init(&connectedUsersMutex, NULL);
    char rootPath[KBYTE] = "/home/augusto/repositorios/ufrgs/dropbox-sisop2/watch_folder/";
    file_info_list* filesInDir = getListOfFiles(rootPath);
    printFileInfoList(filesInDir);
    connectedUserListHead = NULL;

    if (pthread_mutex_init(&connectedUsersMutex, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    pthread_t userDisconnectedThread;
    pthread_create(&userDisconnectedThread, NULL, userDisconnectedEvent, NULL);
    pthread_detach(userDisconnectedThread);

    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERVERPORT);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Server listening..\n");
    len = sizeof(cli);

    int i = 0;
    while( (connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t*)&len))  || i < 5)
    {
        puts("Connection accepted");
        pthread_t newUserThread;
        int* newSocket = calloc(1, sizeof(int ));
        *newSocket = connfd;


        pthread_create(&newUserThread, NULL, newConnection, newSocket);

        pthread_detach(newUserThread);
        //Reply to the client

    }
}


