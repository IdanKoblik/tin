#include "device/virtual.h"
#include <linux/uinput.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include "capability.h"
#include "logging/log.h"
#include "protocols/input.h"

#define INPUT_POLL_TIMEOUT_MS 200

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

void destroy_virtual_device(int fd) {
    if (fd < 0)
        return;

    if (ioctl(fd, UI_DEV_DESTROY) < 0)
        WARN("UI_DEV_DESTROY failed");

    close(fd);
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

static int is_forwardable(uint16_t type) {
    return type == EV_KEY || type == EV_REL || type == EV_SYN;
}

struct capture_source {
    struct Node *node;
    int fd;
    const char *label;
    volatile sig_atomic_t *running;

    atomic_uint *seq;
};

static void *run_capture_loop(void *arg) {
    struct capture_source *src = arg;
    struct pollfd pfd = {.fd = src->fd, .events = POLLIN};
    unsigned long long forwarded = 0;

    while (src->running && *src->running) {
        int ready = poll(&pfd, 1, INPUT_POLL_TIMEOUT_MS);
        if (ready < 0) {
            if (errno == EINTR)
                continue;

            ERROR("poll failed on the %s", src->label);
            break;
        }

        if (ready == 0)
            continue;

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            WARN("The %s was disconnected", src->label);
            break;
        }

        struct input_event ie;
        ssize_t got = read(src->fd, &ie, sizeof(ie));
        if (got < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;

            ERROR("Failed to read from the %s", src->label);
            break;
        }

        if (got != (ssize_t)sizeof(ie)) {
            WARN("Short read of %zd bytes on the %s", got, src->label);
            break;
        }

        if (!is_forwardable(ie.type))
            continue;

        input_packet packet;
        packet.version = INPUT_PACKET_VERSION;
        packet.seq = atomic_fetch_add(src->seq, 1);
        packet.type = ie.type;
        packet.code = ie.code;
        packet.value = ie.value;

        if (!input_packet_send(src->node, src->node->peer_fd, &packet, src->running))
            break;

        forwarded++;
    }

    INFO("Capture of the %s stopped after %llu events", src->label, forwarded);
    return NULL;
}

static void run_replay_loop(struct Node *node, int uinput_fd, volatile sig_atomic_t *running) {
    struct pollfd pfd = {.fd = node->peer_fd, .events = POLLIN};
    uint32_t highest = 0;
    unsigned long long replayed = 0;

    while (running && *running) {
        int ready = poll(&pfd, 1, INPUT_POLL_TIMEOUT_MS);
        if (ready < 0) {
            if (errno == EINTR)
                continue;

            ERROR("poll failed on the peer socket");
            return;
        }

        if (ready == 0)
            continue;

        input_packet packet;
        if (!input_packet_recv(node, node->peer_fd, &packet, running))
            return;

        if (packet.version != INPUT_PACKET_VERSION) {
            WARN("Dropping input packet with unsupported version: %u", packet.version);
            continue;
        }

        if (highest && packet.seq > highest + 1)
            WARN("Sequence gap: expected %u, got %u", highest + 1, packet.seq);
        if (packet.seq > highest)
            highest = packet.seq;

        emit_event(uinput_fd, packet.type, packet.code, packet.value);

        if (++replayed % 500 == 0)
            INFO("Replayed %llu events", replayed);
    }
}

static int run_capture(struct Node *node, int mouse_fd, int keyboard_fd, volatile sig_atomic_t *running) {
    atomic_uint seq = 1;

    struct capture_source sources[] = {
        {.node = node, .fd = mouse_fd, .label = "mouse", .running = running, .seq = &seq},
        {.node = node, .fd = keyboard_fd, .label = "keyboard", .running = running, .seq = &seq},
    };
    size_t source_count = sizeof(sources) / sizeof(sources[0]);

    pthread_t tids[sizeof(sources) / sizeof(sources[0])];
    size_t started = 0;

    for (size_t i = 0; i < source_count; i++) {
        if (sources[i].fd < 0) {
            WARN("No %s to capture from, skipping it", sources[i].label);
            continue;
        }

        int err = pthread_create(&tids[started], NULL, run_capture_loop, &sources[i]);
        if (err) {
            ERROR("Failed to start the %s capture thread: %s", sources[i].label, strerror(err));
            continue;
        }

        started++;
    }

    if (!started) {
        ERROR("No input devices to capture from");
        return 0;
    }

    for (size_t i = 0; i < started; i++)
        pthread_join(tids[i], NULL);

    return 1;
}

int handle_input(struct Node *node, int mouse_fd, int keyboard_fd, volatile sig_atomic_t *running) {
    if (!node || !running) {
        ERROR("Cannot find required parms(node / running)");
        return -1;
    }

    if (node->cap & INPUT_RECEIVE) {
        int uinput_fd = create_virtual_device();
        if (uinput_fd < 0) {
            ERROR("Failed to create the virtual input device");
            return -1;
        }

        run_replay_loop(node, uinput_fd, running);
        destroy_virtual_device(uinput_fd);
        return 1;
    }

    return run_capture(node, mouse_fd, keyboard_fd, running);
}
