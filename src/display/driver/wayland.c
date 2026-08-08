#include "display/driver/wayland.h"
#include "display/common.h"
#include "logging/log.h"
#include <string.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

static void geometry(void *data, struct wl_output *output, int32_t x, int32_t y, int32_t physical_width,
                     int32_t physical_height, int32_t subpixel, const char *make, const char *model,
                     int32_t transform) {
    (void)data;
    (void)output;
    (void)x;
    (void)y;
    (void)physical_width;
    (void)physical_height;
    (void)subpixel;
    (void)make;
    (void)model;
    (void)transform;
}

static void mode(void *data, struct wl_output *output, uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
    (void)output;
    (void)refresh;
    struct Display *display = data;

    if ((flags & WL_OUTPUT_MODE_CURRENT) && !display->width && !display->height) {
        display->width = width;
        display->height = height;
    }
}

static void done(void *data, struct wl_output *output) {
    (void)data;
    (void)output;
}

static void scale(void *data, struct wl_output *output, int32_t factor) {
    (void)data;
    (void)output;
    (void)factor;
}

static const struct wl_output_listener output_listener = {
    geometry,
    mode,
    done,
    scale
};

void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    if (!data) {
        ERROR("provided data is null.");
        return;
    }

    struct Display *display = data;
    if (strcmp(interface, wl_output_interface.name) == 0) {
        struct wl_output *output = wl_registry_bind(registry, name, &wl_output_interface, 2);
        wl_output_add_listener(output, &output_listener, display);
    }
}

struct wl_registry_listener registry_listener = {.global = registry_handle_global};

int display_get_size(struct Display *display) {
    if (!display) {
        ERROR("Cannot find target display");
        return 0;
    }

    struct wl_display *dis = wl_display_connect(NULL);
    if (!dis) {
        ERROR("Cannot get wayland display");
        return 0;
    }

    struct wl_registry *registry = wl_display_get_registry(dis);
    if (!registry) {
        ERROR("Cannot get display registry");
        return 0;
    }

    wl_registry_add_listener(registry, &registry_listener, display);
    wl_display_roundtrip(dis); // receive globals, bind wl_output
    wl_display_roundtrip(dis); // receive the output's geometry/mode/done

    wl_registry_destroy(registry);
    wl_display_disconnect(dis);

    return 1;
}

const DisplayOps WAYLAND_DISPLAY_OPS = {
.display_get_size = display_get_size
};
