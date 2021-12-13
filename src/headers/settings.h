//
// Created by KoroLion on 12/7/2021.
//

#ifndef WEB_SERVER_SETTINGS_H
#define WEB_SERVER_SETTINGS_H

#define DEFAULT_BIND_ADDR "127.0.0.1"
#define DEFAULT_PORT 8081

#define ACCEPT_BACKLOG 128
#define MAX_EVENTS 2048

#define READ_TIMEOUT_S 30
#define WRITE_TIMEOUT_S 30

#define PACKET_MAX_SIZE 1 * 1024 * 1024 // 1 MB

#define DEFAULT_BASE_DIR "./"
#define DEFAULT_FILE "index.html"

#endif //WEB_SERVER_SETTINGS_H
