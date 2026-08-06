#pragma once

#include <string.h>

#define PACK_ENCODE(buf, offset, field) \
    do { \
        memcpy((buf) + (offset), &(field), sizeof(field)); \
        (offset) += sizeof(field); \
    } while (0)

#define PACK_DECODE(buf, offset, field) \
    do { \
        memcpy(&(field), (buf) + (offset), sizeof(field)); \
        (offset) += sizeof(field); \
    } while (0)

