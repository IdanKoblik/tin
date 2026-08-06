#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <unistd.h>
#include "audio/common.h"
#include "crypto/ed25519.h"
#include "logging/log.h"
#include "net/node.h"
#include "net/role.h"

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
    printf("./tin <role (host | connect)> <addr>\n");
}

void *audio_thread(void *arg) {
    return run_audio(arg, &running);
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
    if (argc < 3) {
        usage();
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

    AudioDevice *dev = audio_create();
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

cleanup:
    node_cleanup(&node);
    return 0;
}
