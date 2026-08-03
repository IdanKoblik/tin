#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/syslog.h>
#include <unistd.h>
#include "logging/log.h"
#include "net/node.h"
#include "net/role.h"

#define DEFAULT_PORT (uint16_t)6969

volatile sig_atomic_t running = 1;

void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

void usage() {
    printf("./tin <role (host | connect)> <addr>\n");
}

int main(int argc, char *argv[]) {
    openlog(NULL, LOG_PID | LOG_PERROR, LOG_USER);
    if (argc < 3) {
        usage();
        return 1;
    }
   
    signal(SIGINT, handle_signal);
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

    if (role == SERVER && !node_listen(&node)) {
        goto cleanup;
    } else if (!node_connect(&node)) 
        goto cleanup;

    INFO("Node is up and running!");
    while (running) { }

cleanup:
    node_cleanup(&node);
    return 0;
}
