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
const int MAX_REQUEST = 4096;

const char BASE_DIR[] = "./";
const char DEFAULT_FILE[] = "index.html";

void handle_connection(void* connfd_ptr/*, char *ip, int port*/) {
    int connfd = *((int*)connfd_ptr);

    char *buff = malloc(MAX_REQUEST);
    read(connfd, buff, MAX_REQUEST);

    struct Request req = parse_request(buff);
    free(buff);
    // printf("%s %s (%s:%i)\n", req.method, req.url, ip, port);
    printf("%s %s\n", req.method, req.url);

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

void sigpipe_callback_handler() {
    printf("ERROR: SIGPIPE!\n");
}

void sigterm_callback_handler() {
    sigterm_received = 1;
}

int main() {
    signal(SIGPIPE, sigpipe_callback_handler);
    //signal(SIGTERM, sigterm_callback_handler);

    int sockfd = create_server(BIND_ADDR, PORT);
    if (sockfd < 0) {
        printf("ERROR: Unable to create server!");
        return sockfd;
    }
    printf("INFO: Socket is listening at :%i\n", PORT);

    while (!sigterm_received) {
        struct sockaddr_in cli;
        int len = sizeof(cli);
        int connfd = accept(sockfd, (struct sockaddr*)&cli, (socklen_t*)&len);
        if (connfd < 0) {
            printf("ERROR: Unable to accept connection!\n");
            return -1;
        }

        /*char *ip = malloc(64);
        get_ip(ip, cli);
        int port = cli.sin_port;
        printf("Connection from %s:%i\n", ip, port);
        free(ip);*/

        pthread_t thread;
        int *arg = malloc(sizeof(int));
        *arg = connfd;

        int res = pthread_create(&thread, NULL, (void*) handle_connection, (void*) arg);
        if (res != 0) {
            printf("ERROR: unable to create thread with error code %d", res);
        } else {
            pthread_detach(thread);
        }

        // handle_connection((void*) arg);
    }

    printf("Closing server socket...");
    close(sockfd);

    return 0;
}
