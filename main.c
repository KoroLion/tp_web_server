#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "unistd.h"

#include "sys/socket.h"
#include "sys/epoll.h"
#include "sys/select.h"

#include "errno.h"
#include "fcntl.h"

#include "pthread.h"
#include "signal.h"

#include "headers/utils.h"
#include "headers/fs_utils.h"
#include "headers/http_utils.h"
#include "headers/net_utils.h"

#define MAX_EVENTS 2048

char BIND_ADDR[16] = "127.0.0.1";
int PORT = 8082;


const int PACKET_MAX_SIZE = 1 * 1024 * 1024; // 1 MB

struct SocketReadData {
    char *data;
    long max_size;
    long real_size;
};

struct SocketReadData **socket_read_data;

const char BASE_DIR[] = "./";
const char DEFAULT_FILE[] = "index.html";


int read_socket(int sock) {
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

void clear_socket_read(int sock) {
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

void close_socket(int sock) {
    shutdown(sock, SHUT_RDWR);
    close(sock);
}

void respond_to_data(int connfd, char *data, int tid) {
    struct Request req = parse_request(data);
    free(data);
    data = NULL;
    printf("T#%d, S#%d: %s %s\n", tid, connfd, req.method, req.url);

    if (has_double_dot(req.url)) {
        response_text(connfd, 403, "url contains double dot!");
        return;
    }

    bool is_get = strcmp(req.method, "GET") == 0;
    bool is_head = strcmp(req.method, "HEAD") == 0;
    if (!is_get && !is_head) {
        response_text(connfd, 405, "405 (method not allowed)");
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
        return;
    }

    struct Content content = read_file(path_buf, is_head);
    free(path_buf);

    if (content.length == 0) {
        response_text(connfd, 403, "403 (Access denied)");
        free(content.data);
        return;
    }

    response(connfd, 200, content);

    free(content.data);
}

volatile sig_atomic_t sigterm_received = 0;

void sigterm_callback_handler() {
    sigterm_received = 1;
}

struct WorkerThreadArg {
    int thread_id;
    int listenfd;
};

int epoll_ctl_add(int epfd, int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

void worker_thread(void *arg) {
    struct WorkerThreadArg *wta = (struct WorkerThreadArg*)arg;

    int epfd = epoll_create(1);
    epoll_ctl_add(epfd, wta->listenfd, EPOLLIN | EPOLLOUT | EPOLLET);

    struct epoll_event events[MAX_EVENTS];
    while (!sigterm_received) {

        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        printf("%d descriptors are ready!\n", nfds);

        for (int i = 0; i < nfds; i++) {
            struct epoll_event event = events[i];
            int sock = event.data.fd;

            if (sock == wta->listenfd) {
                int new_sock = accept(wta->listenfd, NULL, NULL);
                epoll_ctl_add(epfd, new_sock, EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP);
            } else if (event.events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP)) {
                int res = read_socket(sock);
                if (res != 0) {
                    printf("clearing %d\n", sock);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
                }

                if (res == -1) {
                    printf("ERROR: Socket %d is unexpectedly closed\n", sock);
                    clear_socket_read(sock);
                    close_socket(sock);
                } else if (res == 1) {
                    shutdown(sock, SHUT_RD);

                    struct SocketReadData *srd = socket_read_data[sock];
                    char *data = malloc(srd->real_size);
                    data = memcpy(data, srd->data, srd->real_size);

                    clear_socket_read(sock);

                    respond_to_data(sock, data, wta->thread_id);

                    close_socket(sock);
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    long cpus_amount = sysconf(_SC_NPROCESSORS_ONLN);
    printf("%ld cpus available\r\n", cpus_amount);
    cpus_amount = 1;

    socket_read_data = malloc(FD_SETSIZE * sizeof(struct SocketReadData*));
    for (int i = 0; i < FD_SETSIZE; i++) {
        socket_read_data[i] = NULL;
    }

    // signal(SIGPIPE, sigpipe_callback_handler);
    // signal(SIGTERM, sigterm_callback_handler);

    if (argc == 3) {
        strncpy(BIND_ADDR, argv[1], 16);
        PORT = atoi(argv[2]);
    }

    int server_sock = create_server(BIND_ADDR, PORT);
    if (server_sock < 0) {
        printf("ERROR: Unable to create server!\n");
        return server_sock;
    }
    int flags = fcntl(server_sock, F_GETFL, 0);
    fcntl(server_sock, F_SETFL, flags | O_NONBLOCK);
    printf("INFO: Socket %d is listening at %s:%i\r\n", server_sock, BIND_ADDR, PORT);

    pthread_t *worker_threads = malloc(cpus_amount * sizeof(pthread_t));
    for (int i = 0; i < cpus_amount; i++) {
        struct WorkerThreadArg *wta = malloc(sizeof(struct WorkerThreadArg));
        wta->listenfd = server_sock;
        wta->thread_id = i;
        pthread_create(&worker_threads[i], NULL, (void*) worker_thread, (void*) wta);
    }

    for (int i = 0; i < cpus_amount; i++) {
        pthread_join(worker_threads[i], NULL);
    }
    free(worker_threads);

    printf("Closing server socket...\n");
    close(server_sock);

    return 0;
}
