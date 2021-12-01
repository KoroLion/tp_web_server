#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "unistd.h"

#include "sys/socket.h"
#include "sys/select.h"
#include "sys/sendfile.h"

#include "errno.h"

#include "signal.h"
#include "fcntl.h"

#include "headers/utils.h"
#include "headers/fs_utils.h"
#include "headers/http_utils.h"
#include "headers/net_utils.h"

char BIND_ADDR[16] = "127.0.0.1";
int PORT = 8082;

const int PACKET_MAX_SIZE = 1 * 1024 * 1024; // 1 MB

struct SocketData {
    char *data;
    long max_size;
    long real_size;

    struct Response *response;

    bool done;
};

struct WorkerThreadArg {
    int thread_id;
    int listenfd;
    char *data;
};

const char BASE_DIR[] = "./";
const char DEFAULT_FILE[] = "index.html";


int read_socket(int sock, struct SocketData *sd) {
    while (true) {
        while (sd->real_size + CHUNK_SIZE + 1 > sd->max_size) {
            sd->max_size *= 2;
            sd->data = realloc(sd->data, sd->max_size);
        }

        long bytes_read = recv(sock, sd->data + sd->real_size, CHUNK_SIZE, MSG_DONTWAIT);
        if (bytes_read <= 0) {
            if (bytes_read == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                return 0; // continue read
            } else {
                return -1; // socket closed by itself
            }
        }

        sd->real_size += bytes_read;
        if (sd->real_size > PACKET_MAX_SIZE) {
            return -2; // packet is larger than allowed
        }

        sd->data[sd->real_size] = 0;

        if (strstr(sd->data, "\r\n\r\n") != 0) {
            return 1; // packet fully read
        }
    }
}

struct Response get_response(char *data) {
    struct Request req = parse_request(data);

    printf("%s %s\n", req.method, req.url);

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
    int res = read_socket(sock, sd);
    if (res == -1) {
        // socket closed by client => it's an error
        sd->done = true;
        return NULL;
    } else if (res == 1) {
        // all headers are read
        shutdown(sock, SHUT_RD);
        return sd->data;
    } else {
        return NULL;
    }
}

void clear_socket_read(int sock, struct SocketData **socket_data) {
    struct SocketData *sd = socket_data[sock];
    if (sd != NULL) {
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

        socket_data[sock] = NULL;
    }
}

void send_response(int sock, struct SocketData *sd) {
    struct Response *resp = sd->response;

    if (resp->data_offset < resp->data_length) {
        char *pos = resp->data + resp->data_offset;
        size_t to_send = resp->data_length - resp->data_offset;

        off_t sent = send(sock, pos, to_send, 0);
        if (sent > 0) {
            resp->data_offset += sent;
        }
        if (sent < 0) {
            sd->done = true;
        }
    } else if (resp->fd_offset < resp->fd_length) {
        size_t to_send = resp->fd_length - resp->fd_offset;

        size_t sent = sendfile(sock, fileno(resp->fd), &resp->fd_offset, to_send);
        if (sent < 0) {
            sd->done = true;
        }
    } else {
        sd->done = true;
    }
}

volatile sig_atomic_t sigterm_received = 0;

void sigterm_callback_handler() {
    printf("SIGTERM received! Attempting to close...\n");
    sigterm_received = 1;
}

struct SocketData* init_sd(int sock, struct SocketData **socket_data) {
    struct SocketData *sd = socket_data[sock];
    if (sd == NULL) {
        sd = malloc(sizeof(struct SocketData));
        sd->max_size = CHUNK_SIZE + 1;
        sd->real_size = 0;
        sd->data = malloc(sd->max_size);
        sd->data[0] = 0;
        sd->response = NULL;
        sd->done = false;
        socket_data[sock] = sd;
    }
    return sd;
}

void start_select(int server_sock) {
    struct SocketData **socket_data;
    socket_data = malloc(FD_SETSIZE * sizeof(struct SocketData*));
    for (int i = 0; i < FD_SETSIZE; i++) {
        socket_data[i] = NULL;
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
                set_nonblock(new_sock);
                init_sd(new_sock, socket_data);

                FD_SET(new_sock, &current_sockets);
            } else {
                struct SocketData *sd = socket_data[sock];

                if (sd->response == NULL) {
                    char *request = read_request(sock, sd);
                    if (request != NULL) {
                        struct Response resp = get_response(request);
                        sd->response = malloc(sizeof(resp));
                        memcpy(sd->response, &resp, sizeof(resp));
                    }
                } else {
                    send_response(sock, sd);
                }

                if (sd->done) {
                    printf("ERROR: Socket %d is closed\n", sock);
                    FD_CLR(sock, &current_sockets);

                    shutdown(sock, SHUT_RDWR);
                    close_socket(sock);

                    clear_socket_read(sock, socket_data);
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    long cpus_amount = sysconf(_SC_NPROCESSORS_ONLN);
    printf("%ld cpus available\r\n", cpus_amount);
    cpus_amount = 2;

    signal(SIGTERM, sigterm_callback_handler);
    signal(SIGPIPE, SIG_IGN);

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
