//
// Created by augusto on 24/07/2022.
//

#include <sys/stat.h>
#include <time.h>

#ifndef DROPBOX_SISOP2_FILE_HANDLER_H
#define DROPBOX_SISOP2_FILE_HANDLER_H
time_t getFileLastModifiedEpoch(char* pathname);
#endif //DROPBOX_SISOP2_FILE_HANDLER_H
