//
// Created by augusto on 04/08/2022.
//

#ifndef DROPBOX_SISOP2_USER_H
#define DROPBOX_SISOP2_USER_H
char* getuserDirPath(char* username);
int startUserSession( char* username, int socket) ;

typedef struct start_user_argument {
    char* username;
    int socket;
} start_user_argument;

#endif //DROPBOX_SISOP2_USER_H
