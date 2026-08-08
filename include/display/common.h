#pragma once

#include <stdint.h>

struct Display {
    uint32_t width;
    uint32_t height;
};

enum Edge {
    EDGE_NONE = 0,
    EDGE_LEFT,
    EDGE_RIGHT,
    EDGE_TOP,
    EDGE_BOTTOM
};

enum Edge string_to_edge(const char *str);
const char *edge_to_string(enum Edge edge);

typedef struct DisplayOps {
    int (*display_get_size)(struct Display *display);
} DisplayOps;

int display_init(struct Display *display);
