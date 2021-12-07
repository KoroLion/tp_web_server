//
// Created by KoroLion on 10/19/2021.
//

#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"

#include "headers/http_utils.h"
#include "headers/test.h"
#include "headers/settings.h"
#include "headers/app.h"

int main(int argc, char *argv[]) {
    if (argc == 2 && strncmp(argv[1], "test", strlen(argv[1])) == 0) {
        printf("INFO: Testing...\n");
        test();
        printf("INFO: All tests have passed!\n");
        return 0;
    }

    char bind_addr[16];
    strncpy(bind_addr, DEFAULT_BIND_ADDR, 16);
    if (argc >= 2) {
        strncpy(bind_addr, argv[1], 16);
    }

    int port = DEFAULT_PORT
    if (argc >= 3) {
        port = (int)strtol(argv[2], NULL, 10);
        if (port < 0 || port > 65535) {
            perror("ERROR: Incorrect port specified!");
            return -1;

        }
    }

    int cpus_amount = (int)sysconf(_SC_NPROCESSORS_ONLN);
    printf("INFO: %d cpus available\r\n", cpus_amount);

    return app_run(bind_addr, port, cpus_amount);
}
