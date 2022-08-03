//
// Created by augusto on 02/08/2022.
//
#include "helper.h"

thread_list* initThreadListElement() {
    thread_list* newElement = calloc(1, sizeof(thread_list));
    newElement->isThreadComplete = false;
    newElement->next = NULL;
    newElement->prev = NULL;

    return newElement;
}