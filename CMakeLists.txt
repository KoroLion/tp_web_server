cmake_minimum_required(VERSION 3.20)
project(web_server C)

set(CMAKE_C_STANDARD 11)

add_executable(web_server
        main.c
        http_utils.c
        net_utils.c
        utils.c
        fs_utils.c
        )
target_link_libraries(web_server pthread)
