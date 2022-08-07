//
// Created by augusto on 04/08/2022.
//

#ifndef DROPBOX_SISOP2_USER_H
#define DROPBOX_SISOP2_USER_H

int startUserSession( char* username, int socket) ;
bool hasAvailableSession(user_t user);
void freeUserList(user_list* userList);
bool hasSessionOpen(user_t user);
user_list* findUser(char* username);

void addSyncDir(int dirSocket, user_t* user);
#endif //DROPBOX_SISOP2_USER_H
