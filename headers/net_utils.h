//
// Created by Артем on 19.10.2021.
//

#ifndef WEB_SERVER_NET_UTILS_H
#define WEB_SERVER_NET_UTILS_H

void get_ip(const char *ip, struct sockaddr_in cli);
int create_server(const char *addr, int port);

#endif //WEB_SERVER_NET_UTILS_H
