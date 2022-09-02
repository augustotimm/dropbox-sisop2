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


replica_info_list* createReplica(char* ip, int port, int isPrimary);

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

                tokenCount += 1;
                token = strtok(NULL, ",");
            }

            replica_info_list *newReplica = createReplica(ipAddr, port, isPrimary);
            DL_APPEND(replicaList, newReplica);
        }

        rowCount += 1;
    }

    return replicaList;
}

replica_info_list* createReplica(char* ip, int port, int isPrimary) {
    struct replica_info_list* replica = (replica_info_list*)
            calloc(1, sizeof(replica_info_list));

    replica->prev = NULL;
    replica->next = NULL;
    replica->replica.port = port;
    replica->replica.isPrimary = !! isPrimary;

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
void* startElection(){
    printf("\n--------Starting Election Process--------\n");
};