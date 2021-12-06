cmake_minimum_required(VERSION 3.20)
project(web_server C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -pedantic -Werror -Wno-unused-variable)

add_executable(web_server
        src/main.c
        src/http_utils.c
        src/net_utils.c
        src/utils.c
        src/fs_utils.c
        src/list.c
        src/test.c
        src/worker_thread.c
        src/app.c
        )
target_link_libraries(web_server pthread)
