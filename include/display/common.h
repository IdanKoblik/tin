#pragma once

#include <stdint.h>

struct Display {
    uint32_t width;
    uint32_t height;
};

typedef struct DisplayOps {
    int (*display_get_size)(struct Display *display);
} DisplayOps;

int display_init(struct Display *display);
