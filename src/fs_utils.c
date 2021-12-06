//
// Created by Артем on 19.10.2021.
//
#include "headers/fs_utils.h"

char* get_ext(char *fname) {
    unsigned ext_begins = strlen(fname) - 1;
    unsigned ext_len = 0;

    while (fname[ext_begins] != '.') {
        if (ext_begins == 0) {
            char *res = malloc(1);
            res[0] = 0;
            return res;
        }
        ext_begins--;
        ext_len++;
    }
    ext_begins++;

    char *res = malloc(ext_len + 1);
    strncpy(res, fname + ext_begins, ext_len);
    return res;
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
