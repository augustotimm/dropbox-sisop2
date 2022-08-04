#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "../lib/helper.h"
#define PORT 8888

#define MAX 80
#define SA struct sockaddr

char commands[5][13] = {"upload", "download", "list", "get_sync_dir", "exit"};

void clientThread(int sockfd)
{
    char request[MAX];
    char buff[MAX];
    char username[USERNAMESIZE];
    int n;

    printf("Enter username: ");
    fgets(username, USERNAMESIZE, stdin);
    write(sockfd, username, sizeof(username));

    for (;;) {
        bzero(buff, sizeof(buff));
        printf("Enter the string : ");
        n = 0;
        fgets(request, sizeof(request), stdin);

        if(strcmp(request, commands[DOWNLOAD])){
            printf("Enter the name of the file : ");
            fgets(buff, sizeof(buff), stdin);
            write(connfd, packet, sizeof(packet));
        }

        if ((strncmp(buff, "exit", 4)) == 0) {
            printf("Client Exit...\n");
            break;
        }
    }
}

int main()
{
    int sockfd, connfd;
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
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

    // function for chat
    clientThread(sockfd);

    // close the socket
    close(sockfd);
}