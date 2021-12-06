#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "unistd.h"

#include "sys/socket.h"
#include "sys/timerfd.h"
#include "sys/epoll.h"
#include "sys/sendfile.h"

#include "time.h"

#include "pthread.h"
#include "signal.h"
#include "fcntl.h"

#include "headers/test.h"
#include "headers/utils.h"
#include "headers/fs_utils.h"
#include "headers/http_utils.h"
#include "headers/net_utils.h"

#define MAX_EVENTS 2048

#define READ_TIMEOUT_S 2
#define WRITE_TIMEOUT_S 2

char BIND_ADDR[16] = "127.0.0.1";
int PORT = 8081;

const int PACKET_MAX_SIZE = 1 * 1024 * 1024; // 1 MB

const char BASE_DIR[] = "./";
const char DEFAULT_FILE[] = "index.html";

pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t stop_received = 0;

struct WorkerThreadArg {
    int server_sock;
};

struct Response get_response(char *data) {
    struct Request req = parse_request(data);
    if (strlen(req.method) == 0 || strlen(req.url) == 0) {
        return response_text(400, "bad request");
    }

    printf("T#%d: %s %s\n", get_tid_hash(), req.method, req.url);

    if (has_double_dot(req.url)) {
        return response_text(403, "url contains double dot!");
    }

    bool is_get = strcmp(req.method, "GET") == 0;
    bool is_head = strcmp(req.method, "HEAD") == 0;
    if (!is_get && !is_head) {
        return response_text(405, "405 (method not allowed)");
    }

    unsigned path_length = strlen(req.url) + strlen(BASE_DIR) + strlen(DEFAULT_FILE) + 16;
    char *path_buf = malloc(path_length);
    bool default_added = url_to_path(path_buf, path_length, req.url, BASE_DIR, DEFAULT_FILE);

    if (!is_regular_file(path_buf)) {
        free(path_buf);

        if (default_added) {
            return response_text(403, "403 (No index file found)");
        } else {
            return response_text(404, "404 (Not found)");
        }
    }

    struct File file = open_file(path_buf);
    free(path_buf);

    if (file.length == 0) {
        return response_text(403, "403 (Access denied)");
    }

    struct Response resp = response_file(200, file);
    if (is_head) {
        resp.fd_length = 0;
    }
    return resp;
}

char* read_request(int sock, struct SocketData *sd) {
    int res = read_http_request(sock, sd, PACKET_MAX_SIZE);

    if (res == SOCK_CLOSED || res == SOCK_ERROR) {
        sd->done = true;
        return NULL;
    } else if (res == READ_DONE) {
        shutdown(sock, SHUT_RD);
        return sd->data;
    } else {
        return NULL;
    }
}

void send_response(int sock, struct SocketData *sd) {
    struct Response *resp = sd->response;

    if (resp->data_offset < resp->data_length) {
        char *pos = resp->data + resp->data_offset;
        size_t to_send = resp->data_length - resp->data_offset;

        off_t sent = send(sock, pos, to_send, 0);
        if (sent < 0) {
            sd->done = true;
            return;
        }

        if (sent > 0) {
            resp->data_offset += sent;
        }
    } else if (resp->fd_offset < resp->fd_length) {
        size_t to_send = resp->fd_length - resp->fd_offset;

        size_t sent = sendfile(sock, fileno(resp->fd), &resp->fd_offset, to_send);
        if (sent < 0) {
            sd->done = true;
            return;
        }
    } else {
        sd->done = true;
    }
}

void sigterm_callback_handler() {
    printf("SIGTERM received! Attempting to close...\n");
    stop_received = 1;
}

void close_socket(int epfd, int time_epfd, struct SocketData *sd) {
    epoll_ctl_del(epfd, sd->fd);
    epoll_ctl_del(time_epfd, sd->tfd);

    close(sd->tfd);

    shutdown(sd->fd, SHUT_RDWR);
    close(sd->fd);

    free_sd(sd);
}

void* worker_thread(void* arg) {
    printf("INFO: Worker thread #%d started!\n", get_tid_hash());

    struct WorkerThreadArg *wta = arg;
    int server_sock = wta->server_sock;

    int time_epfd = epoll_create(1);
    struct epoll_event time_events[MAX_EVENTS];

    int epfd = epoll_create(1);
    struct epoll_event events[MAX_EVENTS];

    struct SocketData *server_sd = malloc_sd(server_sock);
    epoll_ctl_add(epfd, server_sock, server_sd, EPOLLIN | EPOLLOUT);

    while (!stop_received) {
        // timeout to check for sigterm and socket timeout
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, 1500);

        for (int i = 0; i < nfds; i++) {
            struct epoll_event event = events[i];

            struct SocketData *sd = event.data.ptr;
            int sock = sd->fd;

            if (sock == server_sock) {
                pthread_mutex_lock(&accept_mutex);
                int new_sock = accept(server_sock, NULL, NULL);
                pthread_mutex_unlock(&accept_mutex);

                set_nonblock(new_sock);

                struct SocketData *new_sd = malloc_sd(new_sock);
                epoll_ctl_add(epfd, new_sock, new_sd, EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLHUP);

                new_sd->tfd = timerfd_create(CLOCK_MONOTONIC, 0);
                if (new_sd->tfd == -1) {
                    perror("Unable to create timeout!");
                    stop_received = 1;
                    return NULL;
                }
                set_timeout(new_sd->tfd, READ_TIMEOUT_S);
                epoll_ctl_add(time_epfd, new_sd->tfd, new_sd, EPOLLIN);
            } else {
                bool ready_to_read = event.events & EPOLLIN;
                bool ready_to_write = event.events & EPOLLOUT;
                bool read_closed = event.events & EPOLLRDHUP;
                bool closed = event.events & EPOLLHUP;

                if (sd->response == NULL && read_closed) {
                    closed = true;
                }

                if (!closed && ready_to_read && sd->response == NULL) {
                    char *request = read_request(sock, sd);
                    if (request != NULL) {
                        printf("Updating timeout\n");
                        set_timeout(sd->tfd, WRITE_TIMEOUT_S);

                        struct Response resp = get_response(request);
                        sd->response = malloc(sizeof(resp));
                        memcpy(sd->response, &resp, sizeof(resp));
                    }
                }

                if (!closed && ready_to_write && sd->response != NULL ) {
                    send_response(sock, sd);
                }

                if (sd->done || closed) {
                    if (closed) {
                        perror("ERROR: Socket is closed but not done!");
                    }/* else {
                        printf("INFO: Socket is closed!\n");
                    }*/
                    close_socket(epfd, time_epfd, sd);
                }
            }
        }

        nfds = epoll_wait(time_epfd, time_events, MAX_EVENTS, 0);
        for (int i = 0; i < nfds; i++) {
            struct epoll_event event = time_events[i];

            if (event.events & EPOLLIN) {
                struct SocketData *sd = event.data.ptr;

                printf("T#%d: WARN: Socket timeout!\n", get_tid_hash());

                close_socket(epfd, time_epfd, sd);
            }
        }
    }

    free_sd(server_sd);
    close(epfd);

    return NULL;
}

int main(int argc, char *argv[]) {
    test();

    long cpus_amount = sysconf(_SC_NPROCESSORS_ONLN);
    printf("%ld cpus available\r\n", cpus_amount);
    // cpus_amount = 2;

    signal(SIGINT, sigterm_callback_handler);
    signal(SIGTERM, sigterm_callback_handler);
    signal(SIGPIPE, SIG_IGN);

    if (argc == 3) {
        strncpy(BIND_ADDR, argv[1], 16);
        long port = strtol(argv[2], NULL, 10);
        if (port < 0 || port > 65535) {
            perror("ERROR: Incorrect port specified!");
            return -1;

        }
        PORT = (int)port;
    }

    int server_sock = create_server(BIND_ADDR, PORT, false);
    if (server_sock < 0) {
        perror("ERROR: Unable to create server!");
        return server_sock;
    }
    printf("INFO: Socket %d is listening at %s:%i\r\n", server_sock, BIND_ADDR, PORT);


    pthread_t thread_pool[cpus_amount];
    for (int i = 0; i < cpus_amount; i++) {
        struct WorkerThreadArg wta;
        wta.server_sock = server_sock;

        int res = pthread_create(&(thread_pool[i]), NULL, worker_thread, (void*)&wta);
        if (res != 0) {
            perror("ERROR: Unable to create thread!");
            return res;
        }
    }

    for (int i = 0; i < cpus_amount; i++) {
        pthread_join(thread_pool[i], NULL);
    }

    printf("INFO: Closing server socket...\n");
    shutdown(server_sock, SHUT_RDWR);
    close(server_sock);

    return 0;
}
