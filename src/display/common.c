#include "display/common.h"
#include "display/driver/wayland.h"
#include "logging/log.h"
#include <stdlib.h>
#include <string.h>

enum Edge string_to_edge(const char *str) {
    if (!strcmp(str, "left"))
        return EDGE_LEFT;

    if (!strcmp(str, "right"))
        return EDGE_RIGHT;

    if (!strcmp(str, "top"))
        return EDGE_TOP;

    if (!strcmp(str, "bottom"))
        return EDGE_BOTTOM;

    return EDGE_NONE;
}

const char *edge_to_string(enum Edge edge) {
    switch (edge) {
    case EDGE_LEFT:
        return "left";
    case EDGE_RIGHT:
        return "right";
    case EDGE_TOP:
        return "top";
    case EDGE_BOTTOM:
        return "bottom";
    default:
        return "none";
    }
}

int display_init(struct Display *display) {
    if (!display) {
        ERROR("Cannot find display");
        return 0;
    }

    const char *session = getenv("XDG_SESSION_TYPE");
    if (!session) {
        ERROR("Cannot find session type");
        return 0;
    }

    if (strcmp(session, "wayland") == 0) {
        if (!WAYLAND_DISPLAY_OPS.display_get_size(display)) {
            ERROR("Cannot get display size");
            return 0;
        }
    }

    DEBUG("Display size: %u x %u", display->width, display->height);
    return 1;
}
