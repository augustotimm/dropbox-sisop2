//
// Created by augusto on 02/08/2022.
//

#include "event-handler.h"
#include "../client-socket/test-client.h"

void *createdFile(void *args) {
    thread_argument *event =  args;
    char* filePath = event->argument;

    printf("created file\nargs: %s\n", filePath);
    sendMessage(filePath);
    *(event->isThreadComplete) = true;
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