//
// Created by Артем on 19.10.2021.
//

#ifndef WEB_SERVER_UTILS_H
#define WEB_SERVER_UTILS_H

#include "stdbool.h"
#include "ctype.h"

#include "sys/epoll.h"
#include "sys/timerfd.h"

unsigned get_tid_hash();
bool has_double_dot(const char *s);
bool starts_with(const char *s, const char *subs);

int epoll_ctl_add(int epfd, int fd, void *ptr, uint32_t events);
int epoll_ctl_del(int epfd, int fd);

int set_timeout(int tfd, int timeout_s);

#endif //WEB_SERVER_UTILS_H
