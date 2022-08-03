//
// Created by augusto on 02/08/2022.
//

#ifndef DROPBOX_SISOP2_EVENT_HANDLER_H
#define DROPBOX_SISOP2_EVENT_HANDLER_H
#include<stdio.h>
#include<sys/inotify.h>

void *createdFile(void *args);
void *updatedFile(void *args);
void *deletedFile(void *args);
void *createdDir(void *args);
#endif //DROPBOX_SISOP2_EVENT_HANDLER_H
