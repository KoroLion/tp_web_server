#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "unistd.h"

#include "netinet/in.h"
#include "sys/socket.h"

#include "pthread.h"
#include "signal.h"

#include "headers/utils.h"
#include "headers/fs_utils.h"
#include "headers/http_utils.h"
#include "headers/net_utils.h"

const char BIND_ADDR[] = "127.0.0.1";
const int PORT = 8081;

int socket_arr_size = 0;
char **socket_data_arr;
int *socket_size_arr;
int *socket_real_size_arr;

const char BASE_DIR[] = "./";
const char DEFAULT_FILE[] = "index.html";


int read_socket(int sock) {
    printf("Reading from %d\n", sock);
    if (socket_data_arr[sock] == NULL) {
        socket_size_arr[sock] = CHUNK_SIZE + 1;
        socket_real_size_arr[sock] = 0;
        socket_data_arr[sock] = malloc(CHUNK_SIZE + 1);
    }

    if (socket_real_size_arr[sock] + CHUNK_SIZE + 1 > socket_size_arr[sock]) {
        socket_size_arr[sock] *= 2;
        socket_data_arr[sock] = realloc(socket_data_arr[sock], socket_size_arr[sock]);
    }

    int bytes_read = recv(sock, socket_data_arr[sock] + socket_real_size_arr[sock], CHUNK_SIZE, 0);
    if (bytes_read <= 0) {
        return -1;
    }
    socket_real_size_arr[sock] += bytes_read;
    socket_data_arr[sock][socket_real_size_arr[sock]] = 0;

    if (strstr(socket_data_arr[sock], "\r\n\r\n") == 0) {
        return 0; // not found
    } else {
        return 1; // found
    }
}

void respond_to_data(int connfd, char *data) {
    struct Request req = parse_request(data);
    /*printf("***************\n");
    printf("%s\n", headers);
    printf("***************\n");*/
    printf("%s %s\n", req.method, req.url);
    // printf("-----------\n");

    free(data);

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
pthread_mutex_t mutex_lock;

void sigpipe_callback_handler() {
    printf("ERROR: SIGPIPE!\n");
}

void sigterm_callback_handler() {
    sigterm_received = 1;
}

struct WorkerThreadArg {
    int thread_id;
    int listenfd;
};

void worker_thread(void *arg) {
    struct WorkerThreadArg *wta = (struct WorkerThreadArg*)arg;

    fd_set current_sockets, ready_sockets;
    FD_ZERO(&current_sockets);
    FD_SET(wta->listenfd, &current_sockets);

    while (!sigterm_received) {
        ready_sockets = current_sockets;
        if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0) {
            printf("ERROR: Unable to select!\n");
            continue;
        }

        while (FD_SETSIZE > socket_arr_size) {
            printf("WARNING: realloc of socket_data to fit for %d sockets\n", FD_SETSIZE);
            socket_arr_size *= 2;
            socket_data_arr = realloc(socket_data_arr, sizeof(*socket_data_arr) * socket_arr_size);
            socket_real_size_arr = realloc(socket_real_size_arr, sizeof(*socket_real_size_arr) * socket_arr_size);
            socket_size_arr = realloc(socket_size_arr, sizeof(*socket_size_arr) * socket_arr_size);
        }

        for (int i = 0; i < FD_SETSIZE; i++) {
            if (FD_ISSET(i, &ready_sockets)) {
                if (i == wta->listenfd) {
                    int sock = accept(wta->listenfd, NULL, NULL);
                    FD_SET(sock, &current_sockets);
                } else {
                    int res = read_socket(i);
                    if (res == -1) {
                        FD_CLR(i, &current_sockets);
                        printf("ERROR: Socket %d is unexpectedly closed\n", i);
                        shutdown(i, SHUT_RDWR);
                        close(i);
                    } else if (res == 1) {
                        FD_CLR(i, &current_sockets);
                        shutdown(i, SHUT_RD);

                        char *data = malloc(socket_real_size_arr[i]);
                        data = memcpy(data, socket_data_arr[i], socket_real_size_arr[i]);
                        socket_data_arr[i] = NULL;

                        respond_to_data(i, data);

                        shutdown(i, SHUT_RDWR);
                        close(i);
                    }
                }
            }
        }

        /*int sock = accept(wta->listenfd, NULL, NULL);
        printf("BUSY: thread #%d\n", wta->thread_id);
        handle_connection(sock, wta->thread_id);
        printf("FREE: thread #%d\n", wta->thread_id);*/
    }
}

int main() {
    long cpus_amount = sysconf(_SC_NPROCESSORS_ONLN);
    printf("%ld cpus available\n", cpus_amount);
    cpus_amount = 1;

    socket_arr_size = 1024;
    socket_data_arr = malloc(socket_arr_size * sizeof(char*));
    for (int i = 0; i < socket_arr_size; i++) {
        socket_data_arr[i] = NULL;
    }
    socket_size_arr = malloc(socket_arr_size * sizeof(int));
    socket_real_size_arr = malloc(socket_arr_size * sizeof(int));

    // signal(SIGPIPE, sigpipe_callback_handler);
    // signal(SIGTERM, sigterm_callback_handler);

    int sockfd = create_server(BIND_ADDR, PORT);
    if (sockfd < 0) {
        printf("ERROR: Unable to create server!");
        return sockfd;
    }
    printf("INFO: Socket is listening at :%i\n", PORT);

    pthread_t *worker_threads = malloc(cpus_amount * sizeof(pthread_t));
    for (int i = 0; i < cpus_amount; i++) {
        struct WorkerThreadArg *wta = malloc(sizeof(struct WorkerThreadArg));
        wta->listenfd = sockfd;
        wta->thread_id = i;
        pthread_create(&worker_threads[i], NULL, (void*) worker_thread, (void*) wta);
    }

    for (int i = 0; i < cpus_amount; i++) {
        pthread_join(worker_threads[i], NULL);
    }
    free(worker_threads);

    /*while (!sigterm_received) {
        struct sockaddr_in cli;
        int len = sizeof(cli);
        int connfd = accept(sockfd, (struct sockaddr*)&cli, (socklen_t*)&len);
        if (connfd < 0) {
            printf("ERROR: Unable to accept connection!\n");
            return -1;
        }

        pthread_mutex_lock(&mutex);
        printf("Pushing to stack %d\n", connfd);
        stack_push(sockets_stack, connfd);
        pthread_mutex_unlock(&mutex);

        // pthread_t thread;
        int *arg = malloc(sizeof(int));
        *arg = connfd;

        // printf("*********\n");
        handle_connection((void*) arg);
        // printf("---------\n");
    }*/

    printf("Closing server socket...");
    close(sockfd);

    return 0;
}
