//
// Created by KoroLion on 12/7/2021.
//

#include "pthread.h"
#include "signal.h"

#include "headers/net_utils.h"
#include "headers/worker_thread.h"


int app_run(const char *bind_addr, int port, int threads_amount) {
    signal(SIGINT, sigterm_callback_handler);
    signal(SIGTERM, sigterm_callback_handler);

    signal(SIGPIPE, SIG_IGN);

    int server_sock = create_server(bind_addr, port, false);
    if (server_sock < 0) {
        perror("ERROR: Unable to create server!");
        return -1;
    }
    printf("INFO: Socket %d is listening at %s:%i\r\n", server_sock, bind_addr, port);

    pthread_t thread_pool[threads_amount];
    for (int i = 0; i < threads_amount; i++) {
        struct WorkerThreadArg wta;
        wta.server_sock = server_sock;

        int res = pthread_create(&(thread_pool[i]), NULL, worker_thread, (void *) &wta);
        if (res != 0) {
            perror("ERROR: Unable to create thread!");
            return -1;
        }
    }

    for (int i = 0; i < threads_amount; i++) {
        pthread_join(thread_pool[i], NULL);
    }

    printf("INFO: Closing server socket...\n");
    shutdown(server_sock, SHUT_RDWR);
    close(server_sock);

    return 0;
}
