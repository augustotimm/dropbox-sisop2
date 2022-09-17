//
// Created by Augusto on 01/09/2022.
//

#ifndef DROPBOX_SISOP2_REPLICA_MANAGER_H
#define DROPBOX_SISOP2_REPLICA_MANAGER_H
#include "../../lib/utlist.h"
#include <semaphore.h>
#include "../../lib/helper.h"

typedef struct replica_info_t {
    char* ipAddr;
    int port;
    bool isPrimary;
    int electionValue;
} replica_info_t;

typedef struct replica_info_list {
    replica_info_t replica;
    struct replica_info_list *prev, *next;
} replica_info_list;

replica_info_list* readConfig(char* filePath);
replica_info_list* findPrimaryReplica(replica_info_list* replicaList);
void* startElection();

int backupListenForMessage(int socket, char* rootFolderPath, bool *isElectionRunning);
void updatePrimary(int replicaElectionValue);
void deletePrimary();


int sendMessageToFrontEnd(user_session_t session, const char* message);
void broadcastNewPrimaryToBackups();
#endif //DROPBOX_SISOP2_REPLICA_MANAGER_H

// front end

// reconnect front end to new primary
