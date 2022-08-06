//
// Created by augusto on 04/08/2022.
//

#ifndef DROPBOX_SISOP2_USER_H
#define DROPBOX_SISOP2_USER_H
char* getuserDirPath(char* username);
int startUserSession( char* username, int socket) ;
bool hasAvailableSession(user_t user);
void* killUser(void * voidUserList);

#endif //DROPBOX_SISOP2_USER_H
