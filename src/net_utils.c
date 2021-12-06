//
// Created by Артем on 19.10.2021.
//

#include "strings.h"
#include "string.h"

#include "stdbool.h"
#include "stdlib.h"
#include "unistd.h"
#include "fcntl.h"

#include "netinet/in.h"
#include "arpa/inet.h"

#include "sys/socket.h"

#include "headers/net_utils.h"

#define LISTEN_BACKLOG 128

/*void get_ip(const char *ip, struct sockaddr_in cli) {
    inet_ntop(AF_INET, &cli.sin_addr.s_addr, ip, 64);
}*/

int set_nonblock(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int create_server(const char *addr, const int port, bool blocking) {
    struct sockaddr_in serveraddr;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return sockfd;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        return -1;
    }

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);

    if (strlen(addr) == 0) {
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, addr, &serveraddr.sin_addr) <= 0) {
            close(sockfd);

            return -1;
        }
    }

    if (bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) != 0) {
        close(sockfd);
        return -1;
    }

    if (!blocking) {
        set_nonblock(sockfd);
    }

    if (listen(sockfd, LISTEN_BACKLOG) != 0) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int read_http_request(int sock, struct SocketData *sd, unsigned long max_size) {
    while (true) {
        while (sd->real_size + CHUNK_SIZE + 1 > sd->max_size) {
            sd->max_size *= 2;
            sd->data = realloc(sd->data, sd->max_size);
        }

        long bytes_read = recv(sock, sd->data + sd->real_size, CHUNK_SIZE, MSG_DONTWAIT);
        if (bytes_read <= 0) {
            if (bytes_read == 0 || errno == EWOULDBLOCK || errno == EAGAIN) {
                return READ_PART;
            } else {
                return SOCK_CLOSED;
            }
        }

        sd->real_size += bytes_read;
        if (sd->real_size > max_size) {
            return SOCK_ERROR; // packet is larger than allowed
        }

        sd->data[sd->real_size] = 0;

        if (strstr(sd->data, "\r\n\r\n") != 0) {
            return READ_DONE; // packet fully read
        }
    }
}

struct SocketData* malloc_sd(int sock) {
    struct SocketData *sd;

    sd = malloc(sizeof(struct SocketData));
    sd->fd = sock;
    sd->max_size = CHUNK_SIZE + 1;
    sd->real_size = 0;
    sd->data = malloc(sd->max_size);
    sd->data[0] = 0;
    sd->response = NULL;
    sd->done = false;

    return sd;
}

void free_sd(struct SocketData *sd) {
    if (sd->data != NULL) {
        free(sd->data);
        sd->data = NULL;
    }

    if (sd->response != NULL) {
        if (sd->response->fd != 0) {
            fclose(sd->response->fd);
            sd->response->fd = 0;
        }

        if (sd->response->data != NULL) {
            free(sd->response->data);
            sd->response->data = NULL;
        }

        free(sd->response);
    }

    free(sd);
}
