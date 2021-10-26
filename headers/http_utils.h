//
// Created by Артем on 19.10.2021.
//

#ifndef WEB_SERVER_HTTP_UTILS_H
#define WEB_SERVER_HTTP_UTILS_H

#include "arpa/inet.h"
#include "fs_utils.h"

#define MAX_BUFF 1024
#define CHUNK_SIZE 4096

struct Request {
    char method[MAX_BUFF];
    char url[MAX_BUFF];
};

struct Request parse_request(char* buff);
char* read_headers(int sock, int tid);
struct Content read_content(int sock);
char* create_headers(int status, char *type, long content_length);
void response(int connfd, int status, struct Content content);
void response_text(int connfd, int status, const char *text);
bool url_to_path(char *path, unsigned path_length, const char *url, const char *base_dir, const char *default_file);
void urldecode(char *dst, const char *src);

#endif //WEB_SERVER_HTTP_UTILS_H
