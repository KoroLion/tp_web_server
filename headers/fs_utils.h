//
// Created by Артем on 19.10.2021.
//

#ifndef WEB_SERVER_FS_UTILS_H
#define WEB_SERVER_FS_UTILS_H

#include "stdio.h"
#include "stdbool.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"

#include "sys/types.h"
#include "sys/stat.h"

struct Content {
    char *path;
    char *data;
    unsigned long length;
    char type[128];
};

char* get_ext(char *fname);
char* get_type(char *ext);

bool is_regular_file(const char *fpath);
bool is_directory(const char *fpath);
struct Content read_file(char *fpath, bool only_head);

#endif //WEB_SERVER_FS_UTILS_H
