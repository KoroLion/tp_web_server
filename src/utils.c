//
// Created by Артем on 19.10.2021.
//

#include "string.h"

#include "headers/utils.h"

bool has_double_dot(const char *s) {
    return strstr(s, "/..") != 0;
}

bool starts_with(const char *s, const char *subs) {
    char *subs_ptr = strstr(s, subs);
    return (subs_ptr - s) == 0;
}
