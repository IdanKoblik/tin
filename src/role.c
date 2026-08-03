#include "role.h"

enum Role string_to_role(const char *str) {
    if (!strcmp(str, "host"))
        return SERVER;

    if (!strcmp(str, "connect"))
        return CLIENT;

    return UNKNOWN;
}
