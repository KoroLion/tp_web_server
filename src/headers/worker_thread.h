//
// Created by KoroLion on 12/7/2021.
//

#ifndef WEB_SERVER_WORKER_THREAD_H
#define WEB_SERVER_WORKER_THREAD_H

#include "sys/epoll.h"

#include "list.h"
#include "net_utils.h"

struct WorkerThreadArg {
    int server_sock;
};

struct Response get_response(char *data);
char* read_request(int sock, struct SocketData *sd);
void send_response(int sock, struct SocketData *sd);
void close_client_sockets(struct ListNode *l, int epfd);

void sigterm_callback_handler();

struct ListNode* close_socket(struct ListNode *l, int epfd, struct SocketData *sd);
struct ListNode* handle_accept(struct ListNode *l, int server_sock, int epfd);
struct ListNode* handle_client(struct ListNode *l, struct epoll_event event, int epfd, struct SocketData *sd);
struct ListNode* check_sockets_timeout(struct ListNode *l, int epfd);

void* worker_thread(void* arg);

#endif //WEB_SERVER_WORKER_THREAD_H
