//
// Created by Артем on 19.10.2021.
//
#include "headers/http_utils.h"
#include "headers/fs_utils.h"
#include "headers/utils.h"

#include "string.h"
#include "stdlib.h"

struct Request parse_request(char* buff) {
    struct Request req;

    int i = 0;
    while (buff[i] != ' ' && i < strlen(buff) && i < sizeof(req.method)) {
        req.method[i] = buff[i];
        i++;
    }
    if (i < MAX_BUFF) {
        req.method[i] = 0;
    }

    i++;
    int j = 0;
    while (buff[i] != '?' && buff[i] != ' ' && i < strlen(buff) && i < sizeof(req.url)) {
        req.url[j] = buff[i];
        j++; i++;
    }
    if (j < MAX_BUFF) {
        req.url[j] = 0;
    }
    urldecode(req.url, req.url);

    return req;
}

char* create_headers(int status, char *type, unsigned long content_length) {
    char format_str[] =
            "HTTP/1.1 %i %i\r\n"
            "Server: LioKor C Web Server\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %i\r\n"
            "Connection: close\r\n"
            "\r\n";
    unsigned max_length = strlen(format_str) + strlen(type) + 32;
    char *headers = malloc(max_length);
    snprintf(headers, max_length, format_str, status, status, type, content_length);
    return headers;
}

void response(int connfd, int status, struct Content content) {
    char *headers = create_headers(status, content.type, content.length);
    unsigned long headers_length = strlen(headers);

    unsigned long total_size = headers_length + content.length;
    char *data = malloc(total_size);
    memcpy(data, headers, strlen(headers));
    free(headers);
    headers = NULL;

    // we don't have body if it's HEAD
    if (content.data != NULL) {
        memcpy(data + headers_length, content.data, content.length);
    }

    ssize_t total_bytes_send = 0;
    while (total_bytes_send < total_size) {
        ssize_t bytes_send = send(connfd, data + total_bytes_send, total_size - total_bytes_send, MSG_NOSIGNAL);
        if (bytes_send <= 0) {
            free(data);
            return;
        }
        total_bytes_send += bytes_send;
    }
    free(data);
}

void response_text(int connfd, int status, const char *text) {
    struct Content content;
    content.length = strlen(text);
    content.data = malloc(content.length + 1);
    strncpy(content.data, text, content.length);
    strncpy(content.type, "text/plain", sizeof(content.type));

    response(connfd, status, content);

    free(content.data);
}

bool url_to_path(char *path_buf, unsigned path_length, const char *url, const char *base_dir, const char *default_file) {
    strncpy(path_buf, base_dir, path_length);
    strncat(path_buf, url, path_length);
    if (is_directory(path_buf)) {
        strncat(path_buf, "/", path_length);
        strncat(path_buf, default_file, path_length);
        return true;
    }
    return false;
}

void urldecode(char *dst, const char *src) {
    // Source: https://stackoverflow.com/questions/2673207/c-c-url-decode-library/2766963
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a')
                a -= 'a'-'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a'-'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}