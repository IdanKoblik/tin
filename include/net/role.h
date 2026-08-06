#pragma once

#include <string.h>

enum Role {
    SERVER,
    CLIENT,
    UNKNOWN 
};

enum Role string_to_role(const char *str);
