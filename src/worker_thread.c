//
// Created by KoroLion on 12/7/2021.
//

#include "time.h"
#include "fcntl.h"
#include "pthread.h"
#include "signal.h"

#include "sys/timerfd.h"
#include "sys/epoll.h"
#include "sys/sendfile.h"

#include "headers/worker_thread.h"
#include "headers/list.h"
#include "headers/utils.h"
#include "headers/net_utils.h"
#include "headers/http_utils.h"
#include "headers/fs_utils.h"
#include "headers/settings.h"
#include "headers/wsgi.h"

pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t stop_received = 0;

struct Response get_response(char *data) {
    struct Request req = parse_request(data);
    if (strlen(req.method) == 0 || strlen(req.url) == 0) {
        return response_text(400, "bad request");
    }

    printf("T#%d: %s %s\n", get_tid_hash(), req.method, req.url);

    if (has_double_dot(req.url)) {
        return response_text(403, "url contains double dot!");
    }

    if (strncmp(req.url, "/static/", 7) != 0) {
        struct Response resp;
        resp.fd = 0;
        resp.fd_length = 0;
        resp.fd_offset = 0;

        char *wsgi_answer = get_wsgi_answer();
        resp.data_length = strlen(wsgi_answer);
        resp.data = malloc(resp.data_length + 1);
        memcpy(resp.data, wsgi_answer, resp.data_length);
        resp.data[resp.data_length] = 0;
        free(wsgi_answer);

        resp.data_offset = 0;

        return resp;
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

    if (res == SOCK_TOO_BIG) {
        printf("WARN: Read limit reached, closing connection!\n");
        sd->done = true;
        return NULL;
    } else if (res == SOCK_CLOSED || res == SOCK_ERROR) {
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

        off_t sent = sendfile(sock, fileno(resp->fd), &resp->fd_offset, to_send);
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

struct ListNode* close_socket(struct ListNode *l, int epfd, struct SocketData *sd) {
    epoll_ctl_del(epfd, sd->fd);
    if (l != NULL) {
        list_delete(&l, sd);
    }

    shutdown(sd->fd, SHUT_RDWR);
    close(sd->fd);

    free_sd(sd);

    return l;
}

struct ListNode* handle_accept(struct ListNode *l, int server_sock, int epfd) {

    pthread_mutex_lock(&accept_mutex);
    int new_sock = accept(server_sock, NULL, NULL);
    pthread_mutex_unlock(&accept_mutex);

    if (new_sock == -1) {
        // other threads could've already accepted this connection
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            perror("ERROR: Unable to accept");
        }
        return l;
    }

    set_nonblock(new_sock);

    struct SocketData *new_sd = malloc_sd(new_sock);
    epoll_ctl_add(epfd, new_sock, new_sd, EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLHUP);

    new_sd->time_active = time(NULL);
    list_insert(&l, new_sd);

    return l;
}

struct ListNode* handle_client(struct ListNode *l, struct epoll_event event, int epfd, struct SocketData *sd) {
    int sock = sd->fd;
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
            sd->time_active = time(NULL);

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
        l = close_socket(l, epfd, sd);
    }

    return l;
}

struct ListNode* check_sockets_timeout(struct ListNode *l, int epfd) {
    struct ListNode *cur_node = l;
    time_t cur_time = time(NULL);
    while (cur_node != NULL) {
        struct SocketData *sd = cur_node->ptr;
        unsigned diff = cur_time - sd->time_active;

        bool read_timeout = sd->response == NULL && diff > READ_TIMEOUT_S;
        bool write_timeout = sd->response && diff > WRITE_TIMEOUT_S;

        if (read_timeout || write_timeout) {
            printf("WARN: Socket timeout!\n");
            l = close_socket(l, epfd, sd);
            cur_node = l;
        } else {
            cur_node = cur_node->next;
        }
    }

    return l;
}

void close_client_sockets(struct ListNode *l, int epfd) {
    struct ListNode *cur_node = l;
    while (cur_node != NULL) {
        struct SocketData *sd = cur_node->ptr;
        close_socket(NULL, epfd, sd);
        cur_node = cur_node->next;
    }
    list_free(&l);
}

void* worker_thread(void* arg) {
    printf("INFO: Worker thread #%d started!\n", get_tid_hash());

    struct WorkerThreadArg *wta = arg;
    int server_sock = wta->server_sock;

    int epfd = epoll_create(1);
    struct epoll_event events[MAX_EVENTS];

    struct SocketData *server_sd = malloc_sd(server_sock);
    epoll_ctl_add(epfd, server_sock, server_sd, EPOLLIN | EPOLLOUT);

    struct ListNode *l = NULL;

    while (!stop_received) {
        // timeout to check for sigterm and socket timeout
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, 1500);

        for (int i = 0; i < nfds; i++) {
            struct epoll_event event = events[i];
            struct SocketData *sd = event.data.ptr;
            int sock = sd->fd;

            if (sock == server_sock) {
                l = handle_accept(l, server_sock, epfd);
            } else {
                l = handle_client(l, event, epfd, sd);
            }
        }

        l = check_sockets_timeout(l, epfd);
    }

    close_client_sockets(l, epfd);

    epoll_ctl_del(epfd, server_sock);
    close(epfd);

    free_sd(server_sd);

    return NULL;
}
