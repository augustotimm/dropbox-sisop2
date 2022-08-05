//
// Created by augusto on 03/08/2022.
//

#ifndef DROPBOX_SISOP2_SERVER_H
#define DROPBOX_SISOP2_SERVER_H
#include <semaphore.h>
#include "../lib/helper.h"
user_list* connectedUserListHead = NULL;
sem_t userListWrite;

#endif //DROPBOX_SISOP2_SERVER_H
