//
// Created by Артем on 19.10.2021.
//

#ifndef WEB_SERVER_UTILS_H
#define WEB_SERVER_UTILS_H

#include "stdbool.h"
#include "ctype.h"

bool has_double_dot(const char *s);
bool starts_with(const char *s, const char *subs);

#endif //WEB_SERVER_UTILS_H
