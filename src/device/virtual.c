#include "device/virtual.h"
#include <linux/uinput.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "logging/log.h"

int create_virtual_device(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        ERROR("failed to open /dev/uinput");
        return -1;
    }

    int evbits[] = {EV_KEY, EV_SYN, EV_REL};
    for (size_t i = 0; i < sizeof(evbits) / sizeof(evbits[0]); i++) {
        if (ioctl(fd, UI_SET_EVBIT, evbits[i]) < 0) {
            ERROR("UI_SET_EVBIT failed for %d", evbits[i]);
            close(fd);
            return -1;
        }
    }

    for (int code = 1; code < KEY_MAX; code++) {
        if (ioctl(fd, UI_SET_KEYBIT, code) < 0) {
            ERROR("UI_SET_KEYBIT failed for code %d", code);
            close(fd);
            return -1;
        }
    }

    int relbits[] = {REL_X, REL_Y, REL_WHEEL, REL_HWHEEL};
    for (size_t i = 0; i < sizeof(relbits) / sizeof(relbits[0]); i++) {
        if (ioctl(fd, UI_SET_RELBIT, relbits[i]) < 0) {
            ERROR("UI_SET_RELBIT failed for %d", relbits[i]);
            close(fd);
            return -1;
        }
    }

    struct uinput_setup usetup;
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234;
    usetup.id.product = 0x5678;
    strcpy(usetup.name, "Tin virtual device");

    if (ioctl(fd, UI_DEV_SETUP, &usetup) < 0) {
        ERROR("UI_DEV_SETUP failed");
        close(fd);
        return -1;
    }

    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        ERROR("UI_DEV_CREATE failed");
        close(fd);
        return -1;
    }

    return fd;
}

void emit_event(int fd, int type, int code, int val) {
    struct input_event ie;

    ie.type = type;
    ie.code = code;
    ie.value = val;
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;

    if (write(fd, &ie, sizeof(ie)) != sizeof(ie))
        ERROR("write event failed (type=%d code=%d val=%d)", type, code, val);
}
