//
// Created by Артем on 19.10.2021.
//
#include "headers/fs_utils.h"

char* get_ext(char *fname) {
    int i = strlen(fname);
    int j = 0;
    char *temp = malloc(strlen(fname));
    while (fname[i] != '.' && i >= 0) {
        temp[j++] = fname[i--];
    }
    temp[j] = 0;

    char *ext = malloc(j);
    i = j - 1;
    j = 0;
    while (i >= 0) {
        ext[j++] = temp[i--];
    }
    ext[j] = 0;

    free(temp);
    return ext;
}

const char* get_type(char *ext) {
    const int MAX_SIZE = 16;

    if (strncmp(ext, "html", MAX_SIZE) == 0) {
        return "text/html";
    } else if (strncmp(ext, "css", MAX_SIZE) == 0) {
        return "text/css";
    } else if (strncmp(ext, "js", MAX_SIZE) == 0) {
        return "text/javascript";
    } else if (strncmp(ext, "jpg", MAX_SIZE) == 0 || strncmp(ext, "jpeg", MAX_SIZE) == 0) {
        return "image/jpeg";
    } else if (strncmp(ext, "png", MAX_SIZE) == 0) {
        return "image/png";
    } else if (strncmp(ext, "gif", MAX_SIZE) == 0) {
        return "image/gif";
    } else if (strncmp(ext, "swf", MAX_SIZE) == 0) {
        return "application/x-shockwave-flash";
    } else {
        return "text/plain";
    }
}

bool is_regular_file(const char *fpath) {
    struct stat path_stat;
    if (stat(fpath, &path_stat) != 0) {
        return false;
    }
    return S_ISREG(path_stat.st_mode);
}

bool is_directory(const char *fpath) {
    struct stat path_stat;
    if (stat(fpath, &path_stat) != 0) {
        return false;
    }
    return S_ISDIR(path_stat.st_mode);
}

struct File open_file(char *fpath) {
    struct File f;

    f.fd = fopen(fpath, "rb");;
    if (f.fd == 0) {
        f.length = 0;
        return f;
    }

    fseek(f.fd, 0, SEEK_END);
    f.length = ftell(f.fd);
    fseek(f.fd, 0, SEEK_SET);

    char *ext = get_ext(fpath);
    const char *type = get_type(ext);
    free(ext);

    strncpy(f.type, type, sizeof(f.type));

    return f;
}
