#include "display/common.h"
#include "display/driver/wayland.h"
#include "logging/log.h"
#include <stdlib.h>
#include <string.h>

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
