//
// Created by augusto on 06/08/2022.
//
#include "../lib/helper.h"

#ifndef DROPBOX_SISOP2_SERVER_FUNCTIONS_H
#define DROPBOX_SISOP2_SERVER_FUNCTIONS_H

void upload(int socket, char* filePath, char* fileName);
void download(int socket, char* path, received_file_list* list);

void list();
int sync_dir(int socket);

int sendFile(int socket, char* filepath);
int receiveFile(int socket, char* fileName);

#endif //DROPBOX_SISOP2_SERVER_FUNCTIONS_H
