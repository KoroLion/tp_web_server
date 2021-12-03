//
// Created by Артем on 19.10.2021.
//
#include "headers/http_utils.h"
#include "headers/utils.h"

#include "stdio.h"
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

struct Response response_file(int status, struct File f) {
    char *headers = create_headers(status, f.type, f.length);

    struct Response resp;

    resp.data_offset = 0;
    resp.data_length = strlen(headers);
    resp.data = malloc(resp.data_length);
    strncpy(resp.data, headers, resp.data_length);

    resp.fd_offset = 0;
    resp.fd_length = f.length;
    resp.fd = f.fd;

    return resp;
}

struct Response response_text(int status, const char *text) {
    unsigned long text_len = strlen(text);

    char *headers = create_headers(status, "text/plain", text_len);
    unsigned long headers_len = strlen(headers);

    struct Response resp;
    resp.fd = 0;
    resp.fd_length = 0;
    resp.fd_offset = 0;

    resp.data_offset = 0;
    resp.data_length = headers_len + text_len;
    resp.data = malloc(headers_len + text_len);

    memcpy(resp.data, headers, headers_len);
    memcpy(resp.data + headers_len, text, text_len);

    free(headers);

    return resp;
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