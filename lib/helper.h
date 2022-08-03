//
// Created by augusto on 02/08/2022.
//

#ifndef DROPBOX_SISOP2_HELPER_H
#define DROPBOX_SISOP2_HELPER_H
#include "../lib/utlist.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>


typedef struct thread_list {
    pthread_t thread;
    bool isThreadComplete;
    struct thread_list *next, *prev;
} thread_list;

thread_list* initThreadListElement();
#endif //DROPBOX_SISOP2_HELPER_H
