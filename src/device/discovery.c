#include "device/discovery.h"

#include <libudev.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "logging/log.h"

#define EXPAND_LIST(list_ptr, capacity_var) do {                                                        \
    (capacity_var) = (capacity_var) == 0 ? 16 : (capacity_var) * 2;                                     \
    struct device_entry *temp = realloc((list_ptr), (capacity_var) * sizeof(struct device_entry));      \
    if (!temp) {                                                                                        \
        ERROR("memory reallocation failed");                                                            \
        free(list_ptr);                                                                                 \
        exit(EXIT_FAILURE);                                                                             \
    }                                                                                                   \
    (list_ptr) = temp;                                                                                  \
} while(0)

#define BIT_SET(arr, bit) (((arr)[(bit) / (8 * sizeof(long))] >> ((bit) % (8 * sizeof(long)))) & 1UL)

static int probe_caps(const char *devnode, unsigned long *rel_bits, size_t rel_sz, unsigned long *key_bits, size_t key_sz) {
    int fd = open(devnode, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
        return 0;

    int ok = ioctl(fd, EVIOCGBIT(EV_REL, rel_sz), rel_bits) >= 0 && ioctl(fd, EVIOCGBIT(EV_KEY, key_sz), key_bits) >= 0;

    close(fd);
    return ok;
}

static int is_real_mouse(const char *devnode) {
    unsigned long rel_bits[(REL_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {0};
    unsigned long key_bits[(KEY_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {0};
    if (!probe_caps(devnode, rel_bits, sizeof(rel_bits), key_bits, sizeof(key_bits)))
        return 0;

    return BIT_SET(rel_bits, REL_X) && BIT_SET(rel_bits, REL_Y) && BIT_SET(key_bits, BTN_LEFT);
}

static int is_real_keyboard(const char *devnode) {
    unsigned long rel_bits[(REL_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {0};
    unsigned long key_bits[(KEY_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {0};
    if (!probe_caps(devnode, rel_bits, sizeof(rel_bits), key_bits, sizeof(key_bits)))
        return 0;

    if (BIT_SET(rel_bits, REL_X) || BIT_SET(rel_bits, REL_Y))
        return 0;

    return BIT_SET(key_bits, KEY_A) && BIT_SET(key_bits, KEY_Z) && BIT_SET(key_bits, KEY_SPACE) && BIT_SET(key_bits, KEY_ENTER);
}

struct device_entry *fetch_devices(enum devices types, size_t *out_count) {
    if (out_count) *out_count = 0;

    struct udev *udev = udev_new();
    if (!udev) {
        ERROR("failed to create udev");
        return NULL;
    }

    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);

    size_t capacity = 16;
    size_t count = 0;

    struct device_entry *list = malloc(capacity * sizeof(struct device_entry));
    if (!list) {
        ERROR("memory allocation failed");
        udev_enumerate_unref(enumerate);
        udev_unref(udev);
        return NULL;
    }

    struct udev_list_entry *entry;
    udev_list_entry_foreach(entry, devices) {
        const char *path = udev_list_entry_get_name(entry);
        struct udev_device *dev = udev_device_new_from_syspath(udev, path);
        if (!dev) continue;

        const char *node = udev_device_get_devnode(dev);

        struct udev_device *parent =
            udev_device_get_parent_with_subsystem_devtype(dev, "input", NULL);
        const char *name = parent ? udev_device_get_sysattr_value(parent, "name") : NULL;

        int kbd_match = (types & KEYBOARD) && node && is_real_keyboard(node);
        int mouse_match = (types & MOUSE) && node && is_real_mouse(node);

        if (node && name && (kbd_match || mouse_match) && strstr(node, "/event")) {
            if (count >= capacity)
                EXPAND_LIST(list, capacity);

            snprintf(list[count].name, sizeof(list[count].name), "%s", name);
            snprintf(list[count].devnode, sizeof(list[count].devnode), "%s", node);
            count++;
        }

        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
    udev_unref(udev);

    if (count == 0) {
        WARN("no devices found");
        free(list);
        return NULL;
    }

    if (out_count) *out_count = count;
    return list;
}

