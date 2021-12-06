//
// Created by Артем on 19.10.2021.
//

#include "string.h"
#include "pthread.h"

#include "headers/utils.h"

unsigned get_tid_hash() {
    return pthread_self() % 1000;
}

bool has_double_dot(const char *s) {
    return strstr(s, "/..") != 0;
}

bool starts_with(const char *s, const char *subs) {
    char *subs_ptr = strstr(s, subs);
    return (subs_ptr - s) == 0;
}

int epoll_ctl_add(int epfd, int fd, void *ptr, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = ptr;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

int epoll_ctl_del(int epfd, int fd) {
    return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
}

int set_timeout(int tfd, int timeout_s) {
    struct itimerspec ts;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;
    ts.it_value.tv_sec = timeout_s;
    ts.it_value.tv_nsec = 0;
    return timerfd_settime(tfd, 0, &ts, NULL);
}
