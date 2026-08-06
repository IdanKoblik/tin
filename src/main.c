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
#include "logging/log.h"
#include "net/node.h"
#include "net/role.h"
#include "ui.h"

#define DEFAULT_PORT (uint16_t)6969

static volatile sig_atomic_t running = 1;

// The audio thread spends most of its life parked in a blocking recv() or
// write() that clearing the flag cannot wake, so the handler tears the socket
// down too. shutdown() is async-signal-safe; close() here would not be, since
// the fd is still in use on the other thread.
static volatile sig_atomic_t audio_fd = -1;

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
    if (audio_fd >= 0)
        shutdown((int)audio_fd, SHUT_RDWR);
}

static void usage() {
    printf("./tin <role (host | connect)> <addr> <caps (mic | speaker | input, comma separated)>\n");
}

void *audio_thread(void *arg) {
    return run_audio(arg, &running);
}

static char *prompt_audio_source(void) {
    size_t count = 0;
    struct AudioSource *list = fetch_audio_sources(&count);
    if (!list) {
        ERROR("no audio sources available");
        return NULL;
    }

    char lines[count][sizeof(list[0].description) + sizeof(list[0].name) + 8];
    const char *options[count];
    for (size_t i = 0; i < count; i++) {
        snprintf(lines[i], sizeof(lines[i]), "%s -> %s", list[i].description[0] ? list[i].description : list[i].name, list[i].name);
        options[i] = lines[i];
    }

    int idx = select_menu("Select audio source", options, count);
    if (idx < 0) {
        free(list);
        return NULL;
    }

    printf("selected: %s\n", list[idx].name);
    char *picked = strdup(list[idx].name);
    free(list);
    return picked;
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

int main(int argc, char *argv[]) {
    openlog(NULL, LOG_PID | LOG_PERROR, LOG_USER);
    if (argc < 4) {
        usage();
        return 1;
    }

    // Picks the CPU-specific implementations and seeds the RNG. Nothing else
    // in libsodium is safe to call before this returns.
    if (sodium_init() < 0) {
        ERROR("Failed to initialise libsodium");
        return 1;
    }

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);

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

    char *audio_source = "tinphones-pro-max-xl";
    if (cap & AUDIO_CAPTURE) {
        audio_source = prompt_audio_source();
    }

    const char *ip = argv[2];
    uint16_t port = DEFAULT_PORT;

    char *colon = strchr(argv[2], ':');
    if (colon) {
        *colon = '\0';
        port = (uint16_t)strtoul(colon + 1, NULL, 10);
    }

    struct Node node;
    if (!node_init(&node, ip, port))
        goto cleanup;

    node.role = role;
    node.cap = cap;
    char *cfg_path = get_tin_config_path();
    if (!key_pair_load(cfg_path, &node)) {
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

    if (node.cap & AUDIO_ANY) {
        AudioDevice *dev = audio_create(audio_source);
        if (!dev) {
            ERROR("Failed to create audio device");
            goto cleanup;
        }

        pthread_t audio_tid;
        struct AudioThread audio = {.node = &node, .dev = dev};

        audio_fd = node.peer_fd;
        int err = pthread_create(&audio_tid, NULL, audio_thread, &audio);
        if (err) {
            ERROR("Failed to start the audio thread: %s", strerror(err));
            audio_destroy(dev);
            goto cleanup;
        }

        pthread_join(audio_tid, NULL);
        audio_fd = -1;

        audio_destroy(dev);
    }

cleanup:
    node_cleanup(&node);
    return 0;
}
