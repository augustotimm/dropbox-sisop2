//
// Created by augusto on 24/07/2022.
//

#include "file-handler.h"
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>

struct stat info;
struct tm* tm_info;
time_t  epoch_time;

time_t getFileLastModifiedEpoch(char* pathname) {
    char buffer[16];

    stat(pathname, &info);
    epoch_time = time(&info.st_mtime);

    return epoch_time;
}
