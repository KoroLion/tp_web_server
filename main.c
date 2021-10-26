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

const char BASE_DIR[] = "./";
const char DEFAULT_FILE[] = "index.html";


void handle_connection(int connfd, int tid) {
    printf("T#%d: reading headers\n", tid);
    char* headers = read_headers(connfd, tid);
    if (headers == NULL) {
        printf("ERROR: error while reading a request\n");
        return;
    }
    if (strlen(headers) == 0) {
        printf("ERROR: connection closed before request was read\n");
        free(headers);
        return;
    }

    struct Request req = parse_request(headers);
    /*printf("***************\n");
    printf("%s\n", headers);
    printf("***************\n");*/
    printf("T#%d: %s %s\n", tid, req.method, req.url);
    // printf("-----------\n");

    free(headers);

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

    while (!sigterm_received) {
        int sock = accept(wta->listenfd, NULL, NULL);
        printf("BUSY: thread #%d\n", wta->thread_id);
        handle_connection(sock, wta->thread_id);
        printf("FREE: thread #%d\n", wta->thread_id);
    }
}

int main() {
    long cpus_amount = sysconf(_SC_NPROCESSORS_ONLN);
    printf("%ld cpus available\n", cpus_amount);
    // cpus_amount = 2;

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
