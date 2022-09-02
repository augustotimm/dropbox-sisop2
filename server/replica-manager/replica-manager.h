//
// Created by Augusto on 01/09/2022.
//

#ifndef DROPBOX_SISOP2_REPLICA_MANAGER_H
#define DROPBOX_SISOP2_REPLICA_MANAGER_H
#include "../../lib/utlist.h"
#include <semaphore.h>

typedef struct replica_info_t {
    char* ipAddr;
    int port;
    bool isPrimary;
} replica_info_t;

typedef struct replica_info_list {
    replica_info_t replica;
    struct replica_info_list *prev, *next;
} replica_info_list;


replica_info_list* readConfig(char* filePath);
replica_info_list* findPrimaryReplica(replica_info_list* replicaList);
void* startElection();

#endif //DROPBOX_SISOP2_REPLICA_MANAGER_H
