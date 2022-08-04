//
// Created by augusto on 02/08/2022.
//

#ifndef DROPBOX_SISOP2_HELPER_H
#define DROPBOX_SISOP2_HELPER_H
#include "./utlist.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint-gcc.h>

#define FILENAMESIZE 64
#define KBYTE 1024

#define EXIT 4
#define SYNC 3
#define LIST 2
#define DOWNLOAD 1
#define UPLOAD 0

#define USERNAMESIZE 64

#define OUTFOSYNCERROR -99
char endCommand[6] = "\nend\n";

typedef struct packet_t{
    uint16_t type; //Tipo do pacote
    uint16_t seqn; //Número de sequência
    uint32_t total_size; //Número total de fragmentos
    uint16_t length; //Comprimento do payload
    const char* _payload; //Dados do pacote
} packet_t;


typedef struct thread_list {
    pthread_t thread;
    bool isThreadComplete;
    struct thread_list *next, *prev;
} thread_list;

typedef struct thread_argument {
    bool* isThreadComplete;
    void *argument;
} thread_argument;

thread_list* initThreadListElement();

//server comunication functions

int sendFile(int socket, char* filepath);
#endif //DROPBOX_SISOP2_HELPER_H
