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

    size_t i = 0;
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
    free(headers);

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

void urldecode(char *dst, const char *src) {
    char hex_num[5];

    unsigned src_len = strlen(src);
    unsigned j = 0;
    unsigned i = 0;
    while (i < src_len) {
        char a = src[i + 1];
        char b = src[i + 2];
        if (src[i] == '%' && isxdigit(a) && isxdigit(b)) {
            hex_num[0] = '0';
            hex_num[1] = 'x';
            hex_num[2] = a;
            hex_num[3] = b;
            hex_num[4] = 0;

            dst[j++] = (char)strtol(hex_num, NULL, 16);

            i += 3;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = 0;
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
