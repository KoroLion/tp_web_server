#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "unistd.h"

#include "sys/socket.h"
#include "sys/select.h"

#include "errno.h"

#include "signal.h"

#include "headers/utils.h"
#include "headers/fs_utils.h"
#include "headers/http_utils.h"
#include "headers/net_utils.h"

char BIND_ADDR[16] = "127.0.0.1";
int PORT = 8082;

const int PACKET_MAX_SIZE = 1 * 1024 * 1024; // 1 MB

struct SocketReadData {
    char *data;
    long max_size;
    long real_size;
};

struct WorkerThreadArg {
    int thread_id;
    int listenfd;
    char *data;
};

const char BASE_DIR[] = "./";
const char DEFAULT_FILE[] = "index.html";


int read_socket(int sock, struct SocketReadData **socket_read_data) {
    struct SocketReadData *srd = socket_read_data[sock];
    if (srd == NULL) {
        srd = malloc(sizeof(struct SocketReadData));
        srd->max_size = CHUNK_SIZE + 1;
        srd->real_size = 0;
        srd->data = malloc(srd->max_size);
        srd->data[0] = 0;
        socket_read_data[sock] = srd;
    }

    while (true) {
        while (srd->real_size + CHUNK_SIZE + 1 > srd->max_size) {
            srd->max_size *= 2;
            srd->data = realloc(srd->data, srd->max_size);
        }

        long bytes_read = recv(sock, srd->data + srd->real_size, CHUNK_SIZE, MSG_DONTWAIT);
        if (bytes_read <= 0) {
            if (bytes_read == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                return 0; // continue read
            } else {
                return -1; // socket closed by itself
            }
        }

        srd->real_size += bytes_read;
        if (srd->real_size > PACKET_MAX_SIZE) {
            return -2; // packet is larger than allowed
        }

        srd->data[srd->real_size] = 0;

        if (strstr(srd->data, "\r\n\r\n") != 0) {
            return 1; // packet fully read
        }
    }
}

void clear_socket_read(int sock, struct SocketReadData **socket_read_data) {
    struct SocketReadData *srd = socket_read_data[sock];
    if (srd != NULL) {
        if (srd->data != NULL) {
            free(srd->data);
            srd->data = NULL;
        }
        free(srd);
        socket_read_data[sock] = NULL;
    }
}

void respond_to_data(void *arg) {
    struct WorkerThreadArg *wta = (struct WorkerThreadArg*)arg;
    int connfd = wta->listenfd;
    char *data = wta->data;
    int tid = wta->thread_id;

    struct Request req = parse_request(data);
    data = NULL;
    printf("T#%d, S#%d: %s %s\n", tid, connfd, req.method, req.url);

    if (has_double_dot(req.url)) {
        response_text(connfd, 403, "url contains double dot!");
        close_socket(connfd);
        return;
    }

    bool is_get = strcmp(req.method, "GET") == 0;
    bool is_head = strcmp(req.method, "HEAD") == 0;
    if (!is_get && !is_head) {
        response_text(connfd, 405, "405 (method not allowed)");
        close_socket(connfd);
        return;
    }

    unsigned path_length = strlen(req.url) + strlen(BASE_DIR) + strlen(DEFAULT_FILE) + 16;
    char *path_buf = malloc(path_length);
    bool default_added = url_to_path(path_buf, path_length, req.url, BASE_DIR, DEFAULT_FILE);

    if (!is_regular_file(path_buf)) {
        if (default_added) {
            response_text(connfd, 403, "403 (No index file found)");
        } else {
            response_text(connfd, 404, "404 (Not found)");
        }
        free(path_buf);
        close_socket(connfd);
        return;
    }

    struct Content content = read_file(path_buf, is_head);
    free(path_buf);

    if (content.length == 0) {
        response_text(connfd, 403, "403 (Access denied)");
        free(content.data);
        close_socket(connfd);
        return;
    }

    response(connfd, 200, content);

    free(content.data);
    close_socket(connfd);
}

volatile sig_atomic_t sigterm_received = 0;

void sigterm_callback_handler() {
    printf("SIGTERM received! Attempting to close...\n");
    sigterm_received = 1;
}

void start_select(int server_sock) {
    struct SocketReadData **socket_read_data;
    socket_read_data = malloc(FD_SETSIZE * sizeof(struct SocketReadData*));
    for (int i = 0; i < FD_SETSIZE; i++) {
        socket_read_data[i] = NULL;
    }

    fd_set current_sockets, ready_sockets;
    FD_ZERO(&current_sockets);
    FD_SET(server_sock, &current_sockets);

    while (!sigterm_received) {
        ready_sockets = current_sockets;
        if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0) {
            printf("ERROR: Unable to select!\n");
            continue;
        }

        for (int sock = 0; sock < FD_SETSIZE; sock++) {
            if (!FD_ISSET(sock, &ready_sockets)) {
                continue;
            }

            if (sock == server_sock) {
                int new_sock = accept(server_sock, NULL, NULL);
                FD_SET(new_sock, &current_sockets);
                continue;
            }

            int res = read_socket(sock, socket_read_data);
            if (res == -1) {
                // socket closed by client
                // stop monitoring socket
                FD_CLR(sock, &current_sockets);

                printf("ERROR: Socket %d is unexpectedly closed\n", sock);
                clear_socket_read(sock, socket_read_data);
                close_socket(sock);
            } else if (res == 1) {
                // all headers are read
                // stop monitoring socket
                FD_CLR(sock, &current_sockets);
                shutdown(sock, SHUT_RD);

                struct SocketReadData *srd = socket_read_data[sock];
                struct WorkerThreadArg respond_arg;

                respond_arg.data = malloc(srd->real_size);
                memcpy(respond_arg.data, srd->data, srd->real_size);
                clear_socket_read(sock, socket_read_data);

                respond_arg.thread_id = 0;
                respond_arg.listenfd = sock;

                /*pthread_t tid;
                pthread_create(&tid, NULL, (void*) respond_to_data, (void*) &respond_arg);
                pthread_detach(tid);*/
                respond_to_data((void*) &respond_arg);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    long cpus_amount = sysconf(_SC_NPROCESSORS_ONLN);
    printf("%ld cpus available\r\n", cpus_amount);
    cpus_amount = 2;

    signal(SIGTERM, sigterm_callback_handler);

    if (argc == 3) {
        strncpy(BIND_ADDR, argv[1], 16);
        PORT = atoi(argv[2]);
    }

    int server_sock = create_server(BIND_ADDR, PORT, false);
    if (server_sock < 0) {
        printf("ERROR: Unable to create server!\n");
        return server_sock;
    }
    printf("INFO: Socket %d is listening at %s:%i\r\n", server_sock, BIND_ADDR, PORT);

    start_select(server_sock);

    printf("Closing server socket...\n");
    close(server_sock);

    return 0;
}
