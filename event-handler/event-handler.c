//
// Created by augusto on 02/08/2022.
//

#include "event-handler.h"


void *createdFile(void *args) {
    struct inotify_event *event =  args;
    printf("created file\nargs: %s", (char*) args);
}

void *updatedFile(void *args) {
    struct inotify_event *event =  args;
    printf("updated file\nargs: %s", (char*) args);
}

void *deletedFile(void *args) {
    struct inotify_event *event =  args;
    printf("updated file\nargs: %s", (char*) args);
}

void *createdDir(void *args) {
    struct inotify_event *event =  args;
    printf("updated file\nargs: %s", (char*) args);
}