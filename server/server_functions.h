//
// Created by augusto on 06/08/2022.
//

#ifndef DROPBOX_SISOP2_SERVER_FUNCTIONS_H
#define DROPBOX_SISOP2_SERVER_FUNCTIONS_H

void upload(int socket, char* path);
void download(int socket, char* path);

void list();
int sync_dir(int socket);

#endif //DROPBOX_SISOP2_SERVER_FUNCTIONS_H
