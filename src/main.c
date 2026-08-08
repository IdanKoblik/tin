#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <pwd.h>
#include <sodium.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <unistd.h>
#include "audio/common.h"
#include "capability.h"
#include "crypto/ed25519.h"
#include "display/common.h"
#include "logging/log.h"
#include "net/node.h"
#include "net/role.h"
#include "device/discovery.h"
#include "device/virtual.h"

#define DEFAULT_PORT (uint16_t)6969

static volatile sig_atomic_t running = 1;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

static void usage() {
    printf("./tin <role (host | connect)> <addr> <caps (mic | speaker | input-send | "
           "input-recv, comma separated)> [edge (left | right | top | bottom)]\n");
    printf("\n  edge  input-send only: the screen side that hands the input to the peer.\n");
    printf("        Push the pointer into it to take the peer over, push back out to\n");
    printf("        come home. Left out, every event goes to both machines at once.\n");
}

static char *get_tin_config_path(void) {
    const char *home = getenv("HOME");

    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw)
            home = pw->pw_dir;
    }

    if (!home)
        return NULL;

    char *path = malloc(strlen(home) + strlen("/.config/tin") + 1);
    if (!path)
        return NULL;

    sprintf(path, "%s/.config/tin", home);
    return path;
}

struct AudioLink {
    struct Node *node;
    const char *source;
    volatile sig_atomic_t *running;
};

static void *run_audio_link(void *arg) {
    struct AudioLink *link = arg;

    if (!handle_audio(link->node, link->source, link->running))
        ERROR("The audio link stopped early");

    *link->running = 0;
    return NULL;
}

int main(int argc, char *argv[]) {
    openlog(NULL, LOG_PID | LOG_PERROR, LOG_USER);
    if (argc < 4) {
        usage();
        return 1;
    }

    if (sodium_init() < 0) {
        ERROR("Failed to initialise libsodium");
        return 1;
    }

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    struct sigaction ignore;
    sigemptyset(&ignore.sa_mask);
    ignore.sa_flags = 0;
    ignore.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &ignore, NULL);

    enum Role role = string_to_role(argv[1]);
    if (role == UNKNOWN) {
        usage();
        return 1;
    }

    enum Capability cap = string_to_capability(argv[3]);
    if (!cap) {
        usage();
        return 1;
    }

    enum Edge edge = EDGE_NONE;
    if (argc > 4) {
        edge = string_to_edge(argv[4]);
        if (edge == EDGE_NONE) {
            usage();
            return 1;
        }

        if (!(cap & INPUT_SEND))
            WARN("An edge only means something to an input-send node, ignoring it");
    }

    struct Node node;
    int node_ready = 0;

    int keyboard_fd = -1;
    int mouse_fd = -1;

    const char *audio_source = "tinphones-pro-max-xl";
    char *picked_source = NULL;
    if (cap & AUDIO_CAPTURE) {
        picked_source = prompt_audio_source();
        if (!picked_source)
            goto cleanup;

        audio_source = picked_source;
    }

    struct Display display = {0};
    if ((cap & INPUT_ANY) && !display_init(&display))
        WARN("Could not read the display size, continuing without it");

    if (cap & INPUT_SEND) {
        keyboard_fd = open_selected_device(KEYBOARD, "keyboard");
        if (keyboard_fd < 0)
            WARN("No keyboard selected, only the mouse will be shared");

        mouse_fd = open_selected_device(MOUSE, "mouse");
        if (mouse_fd < 0)
            WARN("No mouse selected, only the keyboard will be shared");

        if (keyboard_fd < 0 && mouse_fd < 0) {
            ERROR("input-send needs at least one device to capture from");
            goto cleanup;
        }
    }

    const char *ip = argv[2];
    uint16_t port = DEFAULT_PORT;

    char *colon = strchr(argv[2], ':');
    if (colon) {
        *colon = '\0';
        port = (uint16_t)strtoul(colon + 1, NULL, 10);
    }

    if (!node_init(&node, ip, port))
        goto cleanup;

    node_ready = 1;
    node.role = role;
    node.cap = cap;
    node_set_running(&node, &running);

    char *cfg_path = get_tin_config_path();
    int keys_loaded = key_pair_load(cfg_path, &node);
    free(cfg_path);
    if (!keys_loaded) {
        ERROR("Failed to load ed25519 key pair");
        goto cleanup;
    }

    if (role == SERVER) {
        if (!node_listen(&node))
            goto cleanup;
    } else if (!node_connect(&node))
        goto cleanup;

    if (!node_handshake(&node, &running)) {
        ERROR("Failed to preform handshake");
        goto cleanup;
    }

    INFO("Node is up and running!");

    pthread_t audio_tid;
    int audio_started = 0;
    struct AudioLink audio = {.node = &node, .source = audio_source, .running = &running};

    if (node.cap & AUDIO_ANY) {
        int err = pthread_create(&audio_tid, NULL, run_audio_link, &audio);
        if (err) {
            ERROR("Failed to start the audio link: %s", strerror(err));
            goto cleanup;
        }

        audio_started = 1;
    }

    if (node.cap & INPUT_ANY) {
        if (handle_input(&node, mouse_fd, keyboard_fd, &display, edge, &running) < 0)
            ERROR("Cannot handle input");

        running = 0;
        node_shutdown_peer(&node);
    }

    if (audio_started)
        pthread_join(audio_tid, NULL);

cleanup:
    running = 0;

    if (keyboard_fd >= 0)
        close(keyboard_fd);

    if (mouse_fd >= 0)
        close(mouse_fd);

    if (node_ready)
        node_cleanup(&node);

    free(picked_source);
    closelog();
    return 0;
}
