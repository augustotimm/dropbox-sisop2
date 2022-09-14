//
// Created by Augusto on 01/09/2022.
//
#include<stdio.h>
#include<string.h>
#include<stdbool.h>
#include <stdlib.h>
#include "replica-manager.h"
#include "../../lib/helper.h"
#include "../server.h"
#include "../user.h"
#include "../server_functions.h"
#include <arpa/inet.h>



replica_info_list* createReplica(char* ip, int port, int isPrimary, int electionValue);

replica_info_list* readConfig(char* filePath) {
    replica_info_list* replicaList = NULL;

    FILE *fp;
    char row[KBYTE];
    char *token;

    fp = fopen(filePath,"r");

    int rowCount = 0;

    while (feof(fp) != true)
    {
        fgets(row, KBYTE, fp);
        printf("Row: %s", row);

        token = strtok(row, ",");

        int tokenCount = 0;
        char* ipAddr;
        int port;
        int isPrimary;
        int replicaElectionValue;
        if(rowCount > 0) {
            while(token != NULL)
            {
                printf("Token: %s\n", token);

                if(tokenCount == 0) {
                    ipAddr = token;
                }

                if(tokenCount == 1) {
                    sscanf(token, "%d", &port);
                }

                if(tokenCount == 2) {
                    sscanf(token, "%d", &isPrimary);
                }

                if(tokenCount == 3) {
                    sscanf(token, "%d", &replicaElectionValue);
                }

                tokenCount += 1;
                token = strtok(NULL, ",");
            }

            replica_info_list *newReplica = createReplica(ipAddr, port, isPrimary, replicaElectionValue);
            DL_APPEND(replicaList, newReplica);
        }
        if(rowCount == 0) {
            while(token != NULL)
            {
                printf("Token: %s\n", token);

                if(tokenCount == 0) {
                    sscanf(token, "%d", &replicaElectionValue);
                    electionValue = replicaElectionValue;
                }

                if(tokenCount == 1) {
                    sscanf(token, "%d", &port);
                    electionPort = port;
                }

                tokenCount += 1;
                token = strtok(NULL, ",");
            }
        }



        rowCount += 1;
    }

    return replicaList;
}

replica_info_list* createReplica(char* ip, int port, int isPrimary, int electionValue) {
    struct replica_info_list* replica = (replica_info_list*)
            calloc(1, sizeof(replica_info_list));

    replica->prev = NULL;
    replica->next = NULL;
    replica->replica.port = port;
    replica->replica.isPrimary = !! isPrimary;
    replica->replica.electionValue = electionValue;

    replica->replica.ipAddr = calloc(strlen(ip) +1, sizeof(char));
    strcpy(replica->replica.ipAddr, ip);

    return replica;
}

int isPrimaryCompare(replica_info_list* a, replica_info_list* b ) {
    if(a->replica.isPrimary == b->replica.isPrimary)
        return 0;
    else
        return -1;
}

replica_info_list* findPrimaryReplica(replica_info_list* replicaList) {

    replica_info_list *replica = NULL;
    replica_info_list  etmp;

    etmp.replica.isPrimary = true;

    pthread_mutex_lock(&connectedReplicaListMutex);
    DL_SEARCH(replicaList, replica, &etmp, isPrimaryCompare);
    pthread_mutex_unlock(&connectedReplicaListMutex);

    return  replica;
}

int sendElectionMessage(replica_info_t replica) {
    struct sockaddr_in servaddr;
    char buff[20];
    bzero(buff, sizeof(buff));


    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(replica.ipAddr);
    servaddr.sin_port = htons(replica.port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Election socket to backup replica creation failed\nbackupId: %d\n", replica.electionValue);
    }
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        printf("election connection failed backupId: %d\n", replica.electionValue);
    }

    write(sockfd, &socketTypes[ELECTIONSOCKET], sizeof(socketTypes[ELECTIONSOCKET]));
    recv(sockfd, buff, sizeof(buff), 0);
    bzero(buff, sizeof(buff));

    sprintf(buff, "%d", electionValue);

    write(sockfd, buff, strlen(buff));

    bzero(buff, sizeof(buff));

    read(sockfd, buff, sizeof(buff), 0);

    close(sockfd);

    if(strcmp(buff, endCommand) != 0) {
        return -1;
    }

    return 0;
}

int sendCoordinatorMessage (replica_info_t replica){
    struct sockaddr_in servaddr;
    char buff[20];
    bzero(buff, sizeof(buff));


    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(replica.ipAddr);
    servaddr.sin_port = htons(replica.port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Election socket to backup replica creation failed\nbackupId: %d\n", replica.electionValue);
    }
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        printf("election connection failed backupId: %d\n", replica.electionValue);
    }

    write(sockfd, &socketTypes[ELECTIONCOORDSOCKET], sizeof(socketTypes[ELECTIONCOORDSOCKET]));
    recv(sockfd, buff, sizeof(buff), 0);

    bzero(buff, sizeof(buff));

    sprintf(buff, "%d", electionValue);

    write(sockfd, buff, strlen(buff));

    close(sockfd);

    if(strcmp(buff, endCommand) != 0) {
        return -1;
    }

    return 0;
}

void* startElection(){
    printf("\n--------Starting Election Process--------\n");
    pthread_mutex_lock(&connectedReplicaListMutex);
    replica_info_list* element = NULL;

    bool hasHigherValue = false;
    DL_FOREACH(replicaList, element) {
        if(element->replica.electionValue > electionValue) {
            if(sendElectionMessage(element->replica) == 0) {
                hasHigherValue = true;
            }
        }
    }

    if(!hasHigherValue) {
        element = NULL;
        DL_FOREACH(replicaList, element) {
            sendCoordinatorMessage(element->replica);
        }
        printf("\nI BECAME PRIMARY\n");
        isPrimary = true;
    }

    pthread_mutex_unlock(&connectedReplicaListMutex);

};

socket_conn_list *connectToBackups(replica_info_list *replicaList) {
    replica_info_list *elt = NULL;
    struct sockaddr_in servaddr;
    socket_conn_list *backupConnectionList = NULL;
    DL_FOREACH(replicaList, elt) {
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr(elt->replica.ipAddr);
        servaddr.sin_port = htons(elt->replica.port);

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            printf("Socket to backup replica creation failed...\n");
        }
        if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
            printf("Connection with a backup failed\n");
        }
        else {
            socket_conn_list *newConn = (socket_conn_list*) calloc(1, sizeof(socket_conn_list));
            newConn->socket = sockfd;
            newConn = NULL;

            newConn->prev = NULL;
            newConn->next = NULL;

            DL_APPEND(backupConnectionList, newConn);
        }
    }

    backupConnectionList;
}

int backupListenForMessage(int socket, char* rootFolderPath, bool *isElectionRunning) {
    char currentCommand[13];
    char buff[BUFFERSIZE];

    char fileName[FILENAMESIZE];
    char username[USERNAMESIZE];

    for (;;) {
        bzero(currentCommand, sizeof(currentCommand));
        bzero(fileName, sizeof(fileName));
        bzero(username, sizeof(username));
        bzero(buff, sizeof(buff));



        printf("\n[backupListenForMessage] WAITING\n");
        write(socket, &commands[WAITING], sizeof(commands[WAITING]));

        // read the message from client and copy it in buffer
        recv(socket, currentCommand, sizeof(currentCommand), 0);

        write(socket, &endCommand, strlen(endCommand));

        recv(socket, username, sizeof(username), 0);

        write(socket, &endCommand, strlen(endCommand));

        char* filePath = strcatSafe(rootFolderPath, username);
        char* clientDirPath = strcatSafe(filePath, "/");
        free(filePath);

        if(strcmp(currentCommand, commands[UPLOAD]) ==0 ) {
            char* fileName = download(socket, clientDirPath, NULL, false);
            if(fileName == NULL) {
                break;
            }
            free(fileName);
            printf("\n\n[listenForBackupMessage] finished upload\n\n");
        } else if(strcmp(currentCommand, commands[DELETE]) ==0 ) {
            recv(socket, fileName, sizeof(fileName), 0);
            deleteFile(fileName, clientDirPath);
            write(socket, &endCommand, sizeof(endCommand));

            printf("\n\b[listenForBackupMessage] finished delete\n\n");
        } else if(strcmp(currentCommand, commands[USERCONN]) ==0 ) {
            char ipAddr[BUFFERSIZE];

            bzero(ipAddr, sizeof(ipAddr));
            recv(socket, ipAddr, sizeof(ipAddr), 0);
            write(socket, &endCommand, sizeof(endCommand));

            recv(socket, buff, sizeof(buff), 0);
            write(socket, &endCommand, sizeof(endCommand));
            int port;
            sscanf(buff, "%d", &port);

            bzero(buff, sizeof(buff));


            recv(socket, buff, sizeof(buff), 0);

            if(startUserSession(username, socket, ipAddr, port, buff) != 0) {
                printf("[backupListenForMessage] Fail starting User session");
            }
        } else if(strcmp(currentCommand, commands[USERCLOSE]) ==0 ) {
            char sessionCode[BUFFERSIZE];
            bzero(sessionCode, sizeof(sessionCode));
            recv(socket, sessionCode, sizeof(sessionCode), 0);

            closeUserSession(username, sessionCode);
        }


        free(clientDirPath);

        if (strcmp(currentCommand, commands[EXIT]) == 0 || strlen(currentCommand) == 0) {
            return 0;
        }

        if(*isElectionRunning){
            return 0;
        }
    }
    return -1;
}

int replicaCompare(replica_info_list* a, replica_info_list* b) {
    if(a->replica.electionValue == b->replica.electionValue)
        return 0;
    else
        return -1;
}

void updatePrimary(int replicaElectionValue) {
    replica_info_list * replica = NULL;
    replica_info_list etmp;
    etmp.replica.electionValue = replicaElectionValue;

    pthread_mutex_lock(&connectedReplicaListMutex);
    DL_SEARCH(replicaList, replica, &etmp, replicaCompare);
    if(replica == NULL) {
        printf("\n\n--------New Primary Replica Not FOUND--------\n");
        exit(-99);
    } else {
        replica->replica.isPrimary = true;
    }
    pthread_mutex_unlock(&connectedReplicaListMutex);

}

int primaryCompare(replica_info_list* a, replica_info_list* b) {
    if(a->replica.isPrimary == b->replica.isPrimary)
        return 0;
    else
        return -1;
}

void deletePrimary() {
    replica_info_list *replica = NULL, *replicaTmp = NULL;
    replica_info_list etmp;
    etmp.replica.isPrimary = true;

    pthread_mutex_lock(&connectedReplicaListMutex);
    DL_FOREACH_SAFE(replicaList, replica, replicaTmp) {
        if(replica->replica.isPrimary) {
            DL_DELETE(replicaList, replica);
            free(replica->replica.ipAddr);
            free(replica);
        }
    }
    pthread_mutex_unlock(&connectedReplicaListMutex);

}
