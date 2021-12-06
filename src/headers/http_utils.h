//
// Created by Артем on 19.10.2021.
//

#ifndef WEB_SERVER_HTTP_UTILS_H
#define WEB_SERVER_HTTP_UTILS_H

#include "arpa/inet.h"
#include "fs_utils.h"

#define MAX_BUFF 1024

struct Request {
    char method[MAX_BUFF];
    char url[MAX_BUFF];
};

struct Response {
    char *data;
    long data_length;
    off_t data_offset;

    FILE *fd;
    long fd_length;
    off_t fd_offset;
};

struct Request parse_request(char* buff);
char* create_headers(int status, char *type, unsigned long content_length);

struct Response response_file(int status, struct File content);
struct Response response_text(int status, const char *text);

bool url_to_path(char *path, unsigned path_length, const char *url, const char *base_dir, const char *default_file);
void urldecode(char *dst, const char *src);

#endif //WEB_SERVER_HTTP_UTILS_H
