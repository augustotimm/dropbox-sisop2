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

    for(rowCount = 0; rowCount <=2; rowCount++)
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
    printf("[findPrimaryReplica]");
    replica_info_list *replica = NULL;
    replica_info_list  etmp;

    etmp.replica.isPrimary = true;

    pthread_mutex_lock(&connectedReplicaListMutex);
    printf("\n----[findPrimaryReplica]connectedReplicaListMutex locked----\n");
    DL_SEARCH(replicaList, replica, &etmp, isPrimaryCompare);
    pthread_mutex_unlock(&connectedReplicaListMutex);
    printf("\n----[findPrimaryReplica]connectedReplicaListMutex unlocked----\n");


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

    printf("\nElection message successful");
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
    replica_info_list* element = NULL;

    if(!isPrimary){
        deletePrimary();
    }

    bool hasHigherValue = false;

    pthread_mutex_lock(&connectedReplicaListMutex);
    printf("\n----[startElection]connectedReplicaListMutex locked----\n");
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
        pthread_cond_signal(&electionFinished);

    }

    pthread_mutex_unlock(&connectedReplicaListMutex);
    printf("\n----[startElection]connectedReplicaListMutex unlocked----\n");

    printf("\nFinished election");
};

int sendMessageToFrontEnd(user_session_t session, const char* message){
    struct sockaddr_in servaddr;
    char buff[20];
    bzero(buff, sizeof(buff));


    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(session.ipAddr);
    servaddr.sin_port = htons(session.frontEndPort);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Failed sending message: %s, to session: %s", message, session.sessionCode);
        return -1;
    }
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        printf("Failed sending message: %s, to session: %s", message, session.sessionCode);
        return -1;
    }

    write(sockfd, message, strlen(message));
    recv(sockfd, buff, sizeof(buff), 0);

    close(sockfd);

    if(strcmp(buff, endCommand) != 0) {
        return -1;
    }

    return 0;
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

        if (strcmp(currentCommand, commands[EXIT]) == 0 || strlen(currentCommand) == 0) {
            return 0;
        }

        if(*isElectionRunning){
            return 0;
        }

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
            printf("\nnew user connected: %s\n", username);
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
    printf("\n----[updatePrimary]connectedReplicaListMutex locked----\n");
    DL_SEARCH(replicaList, replica, &etmp, replicaCompare);
    if(replica == NULL) {
        printf("\n\n--------New Primary Replica Not FOUND--------\n");
        exit(-99);
    } else {
        replica->replica.isPrimary = true;
        printf("\nUpdated primary");
    }

    pthread_mutex_unlock(&connectedReplicaListMutex);
    printf("\n----[updatePrimary]connectedReplicaListMutex unlocked----\n");

}

void deletePrimary() {
    replica_info_list *replica = NULL, *replicaTmp = NULL;

    printf("\n----[deletePrimary]connectedReplicaListMutex locked----\n");
    DL_FOREACH_SAFE(replicaList, replica, replicaTmp) {
        if(replica->replica.isPrimary == true) {
            DL_DELETE(replicaList, replica);
            free(replica->replica.ipAddr);
            free(replica);
        }
    }

    printf("\n----[deletePrimary]connectedReplicaListMutex unlocked----\n");
}

int sendNewPrimaryBackupMessage(replica_info_t replica) {
    struct sockaddr_in servaddr;
    char buff[20];
    bzero(buff, sizeof(buff));


    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(replica.ipAddr);
    servaddr.sin_port = htons(replica.port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Election socket to backup replica creation failed\nbackupId: %d\n", replica.electionValue);
        return -1;
    }
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        printf("election connection failed backupId: %d\n", replica.electionValue);
        return -1;
    }

    write(sockfd, &socketTypes[NEWPRIMARYSOCKET], sizeof(socketTypes[NEWPRIMARYSOCKET]));
    recv(sockfd, buff, sizeof(buff), 0);

    close(sockfd);

    printf("\nSent new primary message to replica: %d", replica.electionValue);

    return 0;
}

void broadcastNewPrimaryToBackups(){
    printf("\n[broadcastNewPrimaryToBackups]");

    pthread_mutex_lock(&connectedReplicaListMutex);

    replica_info_list* element = NULL;

    DL_FOREACH(replicaList, element) {
        sendNewPrimaryBackupMessage(element->replica);
    }

    pthread_mutex_unlock(&connectedReplicaListMutex);
}

void* sendBackupReadyMessage(void* args) {
    printf("\n[sendBackupReadyMessage]");
    replica_info_list* primary = findPrimaryReplica(replicaList);
    struct sockaddr_in servaddr;
    char buff[20];
    bzero(buff, sizeof(buff));


    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(primary->replica.ipAddr);
    servaddr.sin_port = htons(primary->replica.port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Backup ready socket creation failed\nbackupId: %d\n", primary->replica.electionValue);
    }
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        printf("Backup ready connection failed backupId: %d\n", primary->replica.electionValue);
    }

    write(sockfd, &socketTypes[RESTARTEDSOCKET], sizeof(socketTypes[RESTARTEDSOCKET]));
    recv(sockfd, buff, sizeof(buff), 0);


    close(sockfd);

    if(strcmp(buff, endCommand) != 0) {
        return -1;
    }

    return 0;
}
