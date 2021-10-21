//
// Created by Артем on 19.10.2021.
//

#include "strings.h"
#include "string.h"

#include "stdlib.h"
#include "unistd.h"

#include "netinet/in.h"
#include "arpa/inet.h"

#include "sys/socket.h"

void get_ip(const char *ip, struct sockaddr_in cli) {
    inet_ntop(AF_INET, &cli.sin_addr.s_addr, ip, 64);
}

int create_server(const char *addr, const int port) {
    struct sockaddr_in serveraddr;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return sockfd;
    }

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);

    if (strlen(addr) == 0) {
        serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, addr, &serveraddr.sin_addr) <= 0) {
            close(sockfd);

            return -1;
        }
    }

    if (bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) != 0) {
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 64) != 0) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}