//
// Created by augusto on 03/08/2022.
//

#ifndef DROPBOX_SISOP2_SERVER_H
#define DROPBOX_SISOP2_SERVER_H
#include <semaphore.h>
#include "../lib/helper.h"

extern user_list* connectedUserListHead;
extern pthread_mutex_t connectedUsersMutex;
extern pthread_cond_t closedUserConnection;
extern pthread_mutex_t connectedReplicaListMutex;

extern pthread_mutex_t backupConnectionMutex;
extern socket_conn_list* backupConnectionList;

extern char rootPath[KBYTE];

void* clientListen(void* conf);
#endif //DROPBOX_SISOP2_SERVER_H

// /home/timm/repos/ufrgs/dropbox-sisop2/LICENSE.txt