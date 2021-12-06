//
// Created by KoroLion on 19.10.2021.
//

#ifndef WEB_SERVER_NET_UTILS_H
#define WEB_SERVER_NET_UTILS_H

#include "time.h"
#include "errno.h"

#include "http_utils.h"

#define CHUNK_SIZE 4096

#define READ_DONE 1
#define READ_PART 0
#define SOCK_CLOSED -1
#define SOCK_ERROR -2
#define SOCK_TOO_BIG -3

struct SocketData {
    int fd;

    char *data;
    unsigned long max_size;
    unsigned long real_size;

    struct Response *response;

    bool done;
    time_t time_active;
};

// void get_ip(const char *ip, struct sockaddr_in cli);
int create_server(const char *addr, int port, bool blocking);
int set_nonblock(int sock);

int read_http_request(int sock, struct SocketData *sd, unsigned long max_size);

struct SocketData* malloc_sd(int sock);
void free_sd(struct SocketData *sd);

#endif //WEB_SERVER_NET_UTILS_H
