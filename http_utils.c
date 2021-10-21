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

char* read_headers(int sock) {
    int buff_size = CHUNK_SIZE;
    int real_size = 0;
    char *buff = malloc(buff_size);
    while (strstr(buff, "\r\n\r\n") == 0) {
        if (real_size + CHUNK_SIZE > buff_size) {
            buff_size *= 2;
            buff = realloc(buff, buff_size);
        }
        int bytes_read = read(sock, buff + real_size, CHUNK_SIZE);
        if (bytes_read < 0) {
            return NULL;
        }
        real_size += bytes_read;
    }
    return buff;
}

struct Content read_content(int sock) {
    struct Content content;
    int data_size = 4096;
    int amount = 0;
    const int step = 1024;
    content.data = malloc(data_size);
    content.length = 0;
    while ((amount = read(sock, content.data + content.length, step)) > 0) {
        content.length += amount;
        if (content.length + step >= data_size) {
            data_size *= 2;
            content.data = realloc(content.data, data_size);
        }
    }
    return content;
}

char* create_headers(int status, char *type, long content_length) {
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
    write(connfd, headers, strlen(headers));
    free(headers);

    // we don't have body if it's HEAD
    if (content.data != NULL) {
        write(connfd, content.data, content.length);
    }

    shutdown(connfd, SHUT_RDWR);
    close(connfd);
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