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

#define PORT 8889
#define MAX 2048
#define SA struct sockaddr

#include <unistd.h>

char commands[5][13] = {"upload", "download", "list", "get_sync_dir", "exit"};

//TODO change to relative path



void* clientConnThread(void* voidArg)
{
    thread_argument* argument = voidArg;
    char buff[MAX];
    bzero(buff, MAX);
    int socket =  (int *) argument->argument;

    strcpy(buff, "TRUE");
    write(socket, buff, sizeof(buff));

    char currentCommand[13];
    bzero(currentCommand, sizeof(currentCommand));
    //waiting for command
    printf("waiting for first command\n");
    for (;;) {
        bzero(buff, MAX);

        // read the message from client and copy it in buffer
        recv(socket, currentCommand, sizeof(currentCommand), 0);
        printf("COMMAND: %s\n", currentCommand);

        if(strcmp(currentCommand, commands[UPLOAD]) ==0 ) {
            upload(socket);
        } else if(strcmp(currentCommand, commands[DOWNLOAD]) ==0 ) {
            download(socket);
        } else if(strcmp(currentCommand, commands[LIST]) ==0 ) {
            list();
        } else if(strcmp(currentCommand, commands[SYNC]) ==0 ) {
            sync();
        }



        if (strlen(currentCommand) == 0 || strcmp(currentCommand, commands[EXIT]) == 0) {
            printf("Server Exit...\n");
            close(socket);

            *argument->isThreadComplete = true;
            return NULL;
        }
    }

}

void* connectUser(void* arg) {
    int socket = * (int*) arg;
    char username[USERNAMESIZE];
    bzero(username, USERNAMESIZE);
    recv(socket, username, USERNAMESIZE, 0);
    if(!startUserSession(username, socket)) {
        close(socket);
    }
    free(arg);
}

void* userDisconnector(void *arg) {
    while(1) {
        thread_list* currentThread = NULL, *tmp = NULL;
        user_list* currentUser = NULL, *userTmp = NULL;

        sleep(10);
        DL_FOREACH_SAFE(connectedUserListHead, currentUser, userTmp) {
            if(!hasAvailableSession(currentUser->user)) {
                pthread_t thread;
                pthread_create(&thread, NULL, killUser, currentUser);
                pthread_detach(thread);
            }
        }
    }
}


// Driver function
int main()
{
    sem_init(&userListWrite, 0, 1);
    connectedUserListHead = NULL;
    strcpy(path, "/home/augusto/repositorios/ufrgs/dropbox-sisop2/watch_folder/");

    pthread_t userDisconnectorThread;
    pthread_create(&userDisconnectorThread, NULL, userDisconnector, NULL);

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
    servaddr.sin_port = htons(PORT);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
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


        pthread_create(&newUserThread, NULL, connectUser, newSocket);

        pthread_detach(newUserThread);
        //Reply to the client

    }
}


