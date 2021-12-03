//
// Created by Артем on 19.10.2021.
//

#ifndef WEB_SERVER_NET_UTILS_H
#define WEB_SERVER_NET_UTILS_H

#define READ_DONE 1
#define READ_PART 0
#define SOCK_CLOSED -1
#define SOCK_ERROR -2

struct SocketData {
    int fd;

    char *data;
    long max_size;
    long real_size;

    struct Response *response;

    bool done;
};

void get_ip(const char *ip, struct sockaddr_in cli);
int create_server(const char *addr, int port, bool blocking);
void set_nonblock(int sock);

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

void close_socket(int sock) {
    shutdown(sock, SHUT_RDWR);
    close(sock);
}

#endif //WEB_SERVER_NET_UTILS_H
