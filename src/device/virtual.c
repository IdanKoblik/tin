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

struct capture_link {
    struct Node *node;
    volatile sig_atomic_t *running;
    atomic_uint seq;

    int mouse_fd;
    int keyboard_fd;

    struct Display display;
    enum Edge edge;

    int switching;

    atomic_int remote;

    int x, y;
    int depth;

    pthread_mutex_t held_lock;
    uint8_t held[KEY_MAX / 8 + 1];
};

struct capture_source {
    struct capture_link *link;
    int fd;
    const char *label;
    int is_mouse;
};

static int clamp_int(int value, int lo, int hi) {
    if (value < lo)
        return lo;

    if (value > hi)
        return hi;

    return value;
}

static void set_grab(int fd, int on, const char *label) {
    if (fd < 0)
        return;

    if (ioctl(fd, EVIOCGRAB, on ? 1 : 0) < 0)
        WARN("Failed to %s the %s", on ? "grab" : "release", label);
}

static int forward_event(struct capture_link *link, uint16_t type, uint16_t code, int32_t value) {
    input_packet packet;
    packet.version = INPUT_PACKET_VERSION;
    packet.seq = atomic_fetch_add(&link->seq, 1);
    packet.type = type;
    packet.code = code;
    packet.value = value;

    return input_packet_send(link->node, link->node->peer_fd, &packet, link->running);
}

static void note_key(struct capture_link *link, const struct input_event *ie) {
    if (ie->type != EV_KEY || ie->code > KEY_MAX || ie->value == 2)
        return;

    pthread_mutex_lock(&link->held_lock);
    if (ie->value)
        link->held[ie->code / 8] |= (uint8_t)(1u << (ie->code % 8));
    else
        link->held[ie->code / 8] &= (uint8_t)~(1u << (ie->code % 8));
    pthread_mutex_unlock(&link->held_lock);
}

static void release_held(struct capture_link *link) {
    uint8_t held[sizeof(link->held)];

    pthread_mutex_lock(&link->held_lock);
    memcpy(held, link->held, sizeof(held));
    memset(link->held, 0, sizeof(link->held));
    pthread_mutex_unlock(&link->held_lock);

    int released = 0;
    for (int code = 0; code <= KEY_MAX; code++) {
        if (!(held[code / 8] & (1u << (code % 8))))
            continue;

        if (!forward_event(link, EV_KEY, (uint16_t)code, 0))
            return;

        released++;
    }

    if (released)
        forward_event(link, EV_SYN, SYN_REPORT, 0);
}

static int at_edge(const struct capture_link *link, int dx, int dy) {
    switch (link->edge) {
    case EDGE_LEFT:
        return link->x == 0 && dx < 0;
    case EDGE_RIGHT:
        return link->x == (int)link->display.width - 1 && dx > 0;
    case EDGE_TOP:
        return link->y == 0 && dy < 0;
    case EDGE_BOTTOM:
        return link->y == (int)link->display.height - 1 && dy > 0;
    default:
        return 0;
    }
}

static int inward(enum Edge edge, int dx, int dy) {
    switch (edge) {
    case EDGE_LEFT:
        return -dx;
    case EDGE_RIGHT:
        return dx;
    case EDGE_TOP:
        return -dy;
    case EDGE_BOTTOM:
        return dy;
    default:
        return 0;
    }
}

static int depth_limit(const struct capture_link *link) {
    if (link->edge == EDGE_LEFT || link->edge == EDGE_RIGHT)
        return (int)link->display.width;

    return (int)link->display.height;
}

static void enter_remote(struct capture_link *link) {
    set_grab(link->mouse_fd, 1, "mouse");
    set_grab(link->keyboard_fd, 1, "keyboard");

    link->depth = 0;
    atomic_store(&link->remote, 1);

    INFO("Pointer pushed into the %s edge, the peer has the input", edge_to_string(link->edge));
}

static void leave_remote(struct capture_link *link) {
    if (!atomic_exchange(&link->remote, 0))
        return;

    release_held(link);

    set_grab(link->mouse_fd, 0, "mouse");
    set_grab(link->keyboard_fd, 0, "keyboard");

    INFO("Pointer came back over the %s edge, the input is local again", edge_to_string(link->edge));
}

static void track_pointer(struct capture_link *link, const struct input_event *ie) {
    if (!link->switching)
        return;

    if (ie->type != EV_REL || (ie->code != REL_X && ie->code != REL_Y))
        return;

    int dx = ie->code == REL_X ? ie->value : 0;
    int dy = ie->code == REL_Y ? ie->value : 0;

    if (atomic_load(&link->remote)) {
        int step = inward(link->edge, dx, dy);
        link->depth = clamp_int(link->depth + step, 0, depth_limit(link));

        if (step < 0 && link->depth == 0)
            leave_remote(link);

        return;
    }

    link->x = clamp_int(link->x + dx, 0, (int)link->display.width - 1);
    link->y = clamp_int(link->y + dy, 0, (int)link->display.height - 1);

    if (at_edge(link, dx, dy))
        enter_remote(link);
}

static void *run_capture_loop(void *arg) {
    struct capture_source *src = arg;
    struct capture_link *link = src->link;
    struct pollfd pfd = {.fd = src->fd, .events = POLLIN};
    unsigned long long forwarded = 0;

    while (link->running && *link->running) {
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

        if (src->is_mouse)
            track_pointer(link, &ie);

        if (link->switching && !atomic_load(&link->remote))
            continue;

        note_key(link, &ie);

        if (!forward_event(link, ie.type, ie.code, ie.value))
            break;

        forwarded++;
    }

    INFO("Capture of the %s stopped after %llu events", src->label, forwarded);
    if (link->switching && atomic_load(&link->remote))
        leave_remote(link);

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

static int link_setup_switching(struct capture_link *link, int mouse_fd) {
    if (link->edge == EDGE_NONE) {
        WARN("No switch edge given, every event is forwarded and both machines will react to it");
        return 0;
    }

    if (!link->display.width || !link->display.height) {
        WARN("Display size is unknown, cannot switch on the %s edge", edge_to_string(link->edge));
        return 0;
    }

    if (mouse_fd < 0) {
        WARN("No mouse to capture, nothing could ever reach the %s edge", edge_to_string(link->edge));
        return 0;
    }

    link->x = (int)link->display.width / 2;
    link->y = (int)link->display.height / 2;

    INFO("Input crosses to the peer at the %s edge of %u x %u",
         edge_to_string(link->edge), link->display.width, link->display.height);
    return 1;
}

static int run_capture(struct Node *node,
                       int mouse_fd,
                       int keyboard_fd,
                       const struct Display *display,
                       enum Edge edge,
                       volatile sig_atomic_t *running) {
    struct capture_link link;
    memset(&link, 0, sizeof(link));

    link.node = node;
    link.running = running;
    link.mouse_fd = mouse_fd;
    link.keyboard_fd = keyboard_fd;
    link.edge = edge;
    atomic_init(&link.seq, 1);
    atomic_init(&link.remote, 0);

    if (display)
        link.display = *display;

    int err = pthread_mutex_init(&link.held_lock, NULL);
    if (err) {
        ERROR("Failed to create the held-key lock: %s", strerror(err));
        return 0;
    }

    link.switching = link_setup_switching(&link, mouse_fd);

    struct capture_source sources[] = {
        {.link = &link, .fd = mouse_fd, .label = "mouse", .is_mouse = 1},
        {.link = &link, .fd = keyboard_fd, .label = "keyboard", .is_mouse = 0},
    };
    size_t source_count = sizeof(sources) / sizeof(sources[0]);

    pthread_t tids[sizeof(sources) / sizeof(sources[0])];
    size_t started = 0;

    for (size_t i = 0; i < source_count; i++) {
        if (sources[i].fd < 0) {
            WARN("No %s to capture from, skipping it", sources[i].label);
            continue;
        }

        err = pthread_create(&tids[started], NULL, run_capture_loop, &sources[i]);
        if (err) {
            ERROR("Failed to start the %s capture thread: %s", sources[i].label, strerror(err));
            continue;
        }

        started++;
    }

    if (!started)
        ERROR("No input devices to capture from");

    for (size_t i = 0; i < started; i++)
        pthread_join(tids[i], NULL);

    set_grab(mouse_fd, 0, "mouse");
    set_grab(keyboard_fd, 0, "keyboard");
    pthread_mutex_destroy(&link.held_lock);

    return started > 0;
}

int handle_input(struct Node *node,
                 int mouse_fd,
                 int keyboard_fd,
                 const struct Display *display,
                 enum Edge edge,
                 volatile sig_atomic_t *running) {
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

    return run_capture(node, mouse_fd, keyboard_fd, display, edge, running);
}
