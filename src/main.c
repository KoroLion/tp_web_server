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

bool handle_args(int argc, char *argv[], bool *testing, char *bind_addr, int *port, int *cores) {
    if (argc == 2 && strncmp(argv[1], "test", strlen(argv[1])) == 0) {
        *testing = true;
    }

    if (argc >= 2) {
        strncpy(bind_addr, argv[1], 16);
    }

    if (argc >= 3) {
        *port = (int)strtol(argv[2], NULL, 10);
        if (*port < 0 || *port > 65535) {
            perror("ERROR: Incorrect port specified!");
            return true;
        }
    }

    if (argc >= 4) {
        *cores = (int)strtol(argv[3], NULL, 10);
        if (*cores <= 0 || *cores > 65535) {
            perror("ERROR: Incorrect cores amount specified!");
            return true;
        }
    }

    return false;
}

int main(int argc, char *argv[]) {
    bool testing = false;
    char bind_addr[16] = DEFAULT_BIND_ADDR;
    int port = DEFAULT_PORT;

    int cpus_amount = (int)sysconf(_SC_NPROCESSORS_ONLN);
    int cores = cpus_amount;

    if (handle_args(argc, argv, &testing, bind_addr, &port, &cores)) {
        printf("INFO: Usage: web_server <test | bind_addr> <port> <cpu_cores>");
        return -1;
    }

    if (testing) {
        printf("INFO: Testing...\n");
        test();
        printf("INFO: All tests have passed!\n");
        return 0;
    }

    printf("INFO: Using %d/%d cpus\n", cores, cpus_amount);
    printf("INFO: Serving %s\n", DEFAULT_BASE_DIR);

    int res = app_run(bind_addr, port, cores);

    return res;
}
